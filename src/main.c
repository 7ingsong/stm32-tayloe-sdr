#include "utils.h"
#include "iq.h"
#include "i2s.h"
#include "si5351.h"
#include "mic.h"
#include "ssb_rx.h"
#include "i2c.h"
#include "ui.h"
#include "ili9488.h"
#include "spectrum.h"

// The I2S half-buffer is only 2.6 ms: refill it (and demodulate the next chunk) between every other step
static void audio_service(void) {
    i2s_dispatch();
    ssb_rx_poll();
}

int main() {
    si5351_init();
    si5351_clk2_8mhz();
    clock_init();
    I2C1_Config(); // si5351_init() set I2C timing up on the 8 MHz boot clock; redo it for PCLK1 = 36 MHz
    iq_set_frequency(LO_FREQ_DEFAULT);

    ui_init(); // OLED + encoder, before the streams start: bringing the display up blocks ~20 ms

    ili9488_init(); // 3.5" TFT on SPI2: spectrum + waterfall; blocks ~0.5 s, before the streams start
    spectrum_init();

    iq_init();

    mic_init(); // ADC3 on PA3, paced by the TIM3 started in iq_init()
    mic_start();

    i2s_init();
    i2s_start();

    while (1){
        iq_dispatch();
        audio_service();
        mic_dispatch();
        audio_service();
        ui_poll();
        audio_service();
        si5351_poll();
        audio_service();
        spectrum_poll();
        audio_service();
    }

    return 0;
}
