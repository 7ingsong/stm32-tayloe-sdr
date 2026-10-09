#include "iq.h"
#include "dac.h"
#include "adc.h"
#include "utils.h"
#include "command.h"
#include "fifo.h"
#include "transport.h"
#include "stm32f10x.h"
#include "si5351.h"
#include "ssb_rx.h"
#include "mic.h"
#include "ssb_tx.h"
#include "spectrum.h"

typedef struct __attribute__((packed)) {
    uint32_t overflow;
    uint32_t tx;
} iq_stream_response_t;

typedef struct __attribute__((packed)) {
    uint16_t bs;    
    uint16_t free_space;
    uint16_t consumtion_fail;
    uint16_t dac_overflow;
    uint16_t tx_usb_overflow;
    uint16_t rx_usb_overflow;
    uint16_t adc_overflow;
} usb_stream_info_t;

static usb_stream_info_t resp_iq_stream_tx_info = {};

static fifo_t fifo_dac;
static uint8_t fifo_buffer_dac[1024*20+1]; // ~80 ms at 64 kHz: headroom for host latency spikes

static fifo_t fifo_adc;
[[maybe_unused]]static uint8_t fifo_buffer_adc[1024*6+1]; // 24 RX frames: USB drains it quickly

[[maybe_unused]]static uint8_t half = 0;

static int toggle = 0;

static uint32_t lo_frequency = LO_FREQ_DEFAULT;

// The ADC runs from boot for the I2S audio; RX_START/STOP only gate streaming it over USB
static int rx_streaming = 0;

// The DAC runs from boot fed by the mic SSB modulator; TX_START hands it to the host, TX_STOP gives it back
static int tx_from_mic = 1;

// On-board radio mode (iq.h): receive only, transmit only, or both chains at once (full duplex)
static uint8_t ptt = RADIO_RX;

#define RX_ACTIVE() (ptt != RADIO_TX)
#define TX_ACTIVE() (ptt != RADIO_RX)

uint8_t iq_get_ptt(void) {
    return ptt;
}

void iq_set_ptt(uint8_t mode) {
    ptt = mode > RADIO_DUPLEX ? RADIO_DUPLEX : mode;
}

#define RX_FRAMES_PER_PASS 4 // ~0.25 ms of checksums and copies

#define DAC_PRIME_BLOCKS 2 // mic and DAC clocks are locked, so this fill level never drifts

static void dac_restart_fifo(int prime_blocks) {
    static const uint32_t silence = 0x08000800;
    fifo_dac.tail = fifo_dac.head;
    for (int i = 0; i < prime_blocks * MIC_N_SAMPLES / 2; i++) {
        fifo_write(&fifo_dac, (const uint8_t*)&silence, sizeof(silence));
    }
}

/*
 * A mic block takes ~2.8 ms to modulate: in duplex that would make the I2S refill late (clicks), so like the
 * receiver it is only queued here and iq_tx_poll() works through it in small chunks, appending to the DAC fifo.
 * The DAC is primed two blocks ahead, so finishing a block within its 8 ms keeps the stream continuous.
 */
#define TX_CHUNK 64 // multiple of 8: ssb_tx_process() emits 8 DAC words per 8 kHz sample

static const uint16_t *mic_queue;
static int mic_queued;

static void tx_chunk(void) {
    static uint32_t iq[TX_CHUNK];
    int n = mic_queued < TX_CHUNK ? mic_queued : TX_CHUNK;
    if (TX_ACTIVE()) {
        ssb_tx_process(mic_queue, n, iq);
    } else {
        // Keep feeding the DAC so the fifo level (and thus the latency) stays put while receiving
        for (int i = 0; i < n; i++) {
            iq[i] = 0x08000800;
        }
    }
    fifo_write(&fifo_dac, (const uint8_t*)iq, n * sizeof(uint32_t));
    mic_queue += n;
    mic_queued -= n;
}

void on_mic(uint16_t *buf, int n) {
    if (!tx_from_mic) {
        return;
    }
    while (mic_queued > 0) {
        tx_chunk(); // previous block not finished (shouldn't happen): finish it now
    }
    mic_queue = buf;
    mic_queued = n;
}

void iq_tx_poll(void) {
    if (mic_queued > 0 && tx_from_mic) {
        tx_chunk();
    }
}

void iq_set_frequency(uint32_t frequency) {
    lo_frequency = frequency;
    si5351_set_frequency((int32_t)frequency, SI5351_DRIVE_STRENGTH_2MA);
}

uint32_t iq_get_frequency(void) {
    return lo_frequency;
}

void on_adc(uint32_t *buf, int n){
    spectrum_capture(buf, n);
    if (RX_ACTIVE() && tx_from_mic) { // also idle while the host transmits (TX_START)
        ssb_rx_process_adc(buf, n);
    }

    if (rx_streaming) {
        fifo_write(&fifo_adc, (uint8_t*)buf, n * sizeof(uint32_t));
    }

    toggle^=1;
}


void on_dac(uint32_t *buf, int n) {
    int size = n * sizeof(uint32_t);
    int got = fifo_read(&fifo_dac, (uint8_t*)buf, size);
    if (got < size) {
        // Underrun: pad with mid-scale silence instead of replaying the stale half-buffer
        for (int i = got / 4; i < n; i++) {
            buf[i] = 0x08000800;
        }
        resp_iq_stream_tx_info.consumtion_fail++;
    }

    half^=1;
}


void iq_init() {

    fifo_init(&fifo_dac, (uint8_t*)fifo_buffer_dac, sizeof(fifo_buffer_dac));
    fifo_init(&fifo_adc, (uint8_t*)fifo_buffer_adc, sizeof(fifo_buffer_adc));

    transport_init();
    
    dac_init();
    adc_init();
    adc_start();

    dac_restart_fifo(DAC_PRIME_BLOCKS);
    dac_start();
}

static void handle_ping(const frame_t* frame) {
    uint8_t payload[]={'P','O','N','G'};
    command_send(RESP_ACK, frame->command.seq, payload, sizeof(payload));
}

// Empty payload: report the current LO; 4-byte LE payload: set the LO in Hz. ACK carries the LO in effect.
static void handle_set_freq(const frame_t* frame) {
    if (frame->command.len == 4) {
        uint32_t frequency = (uint32_t)frame->payload[0] | ((uint32_t)frame->payload[1] << 8) |
                             ((uint32_t)frame->payload[2] << 16) | ((uint32_t)frame->payload[3] << 24);
        if (frequency < LO_FREQ_MIN || frequency > LO_FREQ_MAX) {
            command_send_error(frame->command.seq, ERR_BAD_PAYLOAD, CMD_SET_FREQ);
            return;
        }
        iq_set_frequency(frequency);
    } else if (frame->command.len != 0) {
        command_send_error(frame->command.seq, ERR_BAD_LENGTH, frame->command.len & 0xFF);
        return;
    }
    command_send(RESP_ACK, frame->command.seq, (const uint8_t*)&lo_frequency, sizeof(lo_frequency));
}

// Empty payload: report; 1 byte: 0 = receive, 1 = transmit. ACK carries the state in effect.
static void handle_ptt(const frame_t* frame) {
    if (frame->command.len == 1) {
        iq_set_ptt(frame->payload[0]); // 0 = RX, 1 = TX, 2 = duplex
    } else if (frame->command.len != 0) {
        command_send_error(frame->command.seq, ERR_BAD_LENGTH, frame->command.len & 0xFF);
        return;
    }
    command_send(RESP_ACK, frame->command.seq, &ptt, sizeof(ptt));
}

// Empty payload: report; 1 byte: set the on-board receiver volume (0 = mute, 255 max). ACK carries the volume.
static void handle_volume(const frame_t* frame) {
    if (frame->command.len == 1) {
        ssb_rx_set_volume(frame->payload[0]);
    } else if (frame->command.len != 0) {
        command_send_error(frame->command.seq, ERR_BAD_LENGTH, frame->command.len & 0xFF);
        return;
    }
    uint8_t v = ssb_rx_get_volume();
    command_send(RESP_ACK, frame->command.seq, &v, sizeof(v));
}

// Empty payload: report; 1 byte: set the on-board transmitter mic gain (0 = silence, 255 max). ACK carries the gain.
static void handle_mic_gain(const frame_t* frame) {
    if (frame->command.len == 1) {
        ssb_tx_set_gain(frame->payload[0]);
    } else if (frame->command.len != 0) {
        command_send_error(frame->command.seq, ERR_BAD_LENGTH, frame->command.len & 0xFF);
        return;
    }
    uint8_t g = ssb_tx_get_gain();
    command_send(RESP_ACK, frame->command.seq, &g, sizeof(g));
}

// Empty payload: report; 3 bytes [rise_shift, fall_shift, smooth_bins]: set the TFT spectrum smoothing.
// ACK carries the values in effect (clamped).
static void handle_spectrum(const frame_t* frame) {
    if (frame->command.len == 3) {
        spectrum_smoothing_t s = {frame->payload[0], frame->payload[1], frame->payload[2]};
        spectrum_set_smoothing(s);
    } else if (frame->command.len != 0) {
        command_send_error(frame->command.seq, ERR_BAD_LENGTH, frame->command.len & 0xFF);
        return;
    }
    spectrum_smoothing_t s = spectrum_get_smoothing();
    uint8_t reply[] = {s.rise_shift, s.fall_shift, s.smooth_bins};
    command_send(RESP_ACK, frame->command.seq, reply, sizeof(reply));
}

void command_handler(const frame_t* frame) {
    switch (frame->command.cmd) {
        case CMD_PING:
            handle_ping(frame);
            break;
        case CMD_SET_FREQ:
            handle_set_freq(frame);
            break;
        case CMD_PTT:
            handle_ptt(frame);
            break;
        case CMD_VOLUME:
            handle_volume(frame);
            break;
        case CMD_MIC_GAIN:
            handle_mic_gain(frame);
            break;
        case CMD_SPECTRUM:
            handle_spectrum(frame);
            break;
        case CMD_IQ_STREAM_TX:
            fifo_write(&fifo_dac, frame->payload, frame->command.len);
            break;

        case CMD_IQ_STREAM_TX_START:
            tx_from_mic = 0;
            mic_queued = 0;
            dac_restart_fifo(0); // the host fills it from here
            command_send(RESP_ACK, frame->command.seq, 0, 0);
            break;
        case CMD_IQ_STREAM_TX_STOP:
            tx_from_mic = 1;
            dac_restart_fifo(DAC_PRIME_BLOCKS);
            command_send(RESP_ACK, frame->command.seq, 0, 0);
            break;

        case CMD_IQ_STREAM_RX_START:
            fifo_adc.tail = fifo_adc.head; // start from fresh samples
            rx_streaming = 1;
            command_send(RESP_ACK, frame->command.seq, 0, 0);
            break;
        case CMD_IQ_STREAM_RX_STOP:
            rx_streaming = 0;
            command_send(RESP_ACK, frame->command.seq, 0, 0);
            break;

        case CMD_IQ_STREAM_TX_INFO:
            fifo_write(&fifo_dac, frame->payload, frame->command.len);
            resp_iq_stream_tx_info.bs = FRAME_MAX_PAYLOAD;
            resp_iq_stream_tx_info.free_space = fifo_get_free_space(&fifo_dac);
            resp_iq_stream_tx_info.dac_overflow = fifo_get_overflow(&fifo_dac);
            resp_iq_stream_tx_info.tx_usb_overflow = transport_get_tx_overflow();
            resp_iq_stream_tx_info.rx_usb_overflow = transport_get_rx_overflow();
            resp_iq_stream_tx_info.adc_overflow = fifo_get_overflow(&fifo_adc);
            
            command_send(RESP_IQ_STREAM_TX_INFO, frame->command.seq, (const uint8_t*)&resp_iq_stream_tx_info, sizeof(resp_iq_stream_tx_info));
            break;

        default:
            command_send_error(frame->command.seq, ERR_BAD_COMMAND, frame->command.cmd);
            break;
    }
}

void iq_dispatch() {
    dac_dispatch();
    adc_dispatch();

    static uint8_t iq_usb_stream_seq = 0;
    uint8_t buf[FRAME_MAX_PAYLOAD];
    // Drain ready RX frames while USB has room: more than one per pass (one is too slow when TX traffic is
    // parsed too), but bounded, or a backlog after a host stall blocks the loop for ms and the I2S refill is late
    int budget = RX_FRAMES_PER_PASS;
    while (budget-- > 0 && fifo_get_filled(&fifo_adc) >= FRAME_MAX_PAYLOAD &&
           transport_get_tx_free_space() >= PACKAGE_HEADER_SIZE + FRAME_MAX_PAYLOAD) {
        fifo_read(&fifo_adc, buf, FRAME_MAX_PAYLOAD);
        command_send(RESP_IQ_STREAM_RX, iq_usb_stream_seq++, buf, FRAME_MAX_PAYLOAD);
    }

    command_dispatch(command_handler);
}