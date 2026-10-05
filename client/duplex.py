from iqlib import (
    DeviceClient, ProtocolError, build_frame, u12_bytes_to_cf32,
    CMD_IQ_STREAM_TX, CMD_IQ_STREAM_TX_INFO, CMD_IQ_STREAM_TX_STOP, CMD_IQ_STREAM_RX_STOP,
    RESP_ACK, RESP_ERR, RESP_IQ_STREAM_RX, RESP_IQ_STREAM_TX_INFO, ERR_NAMES,
)
from send import GnuRadioSink
import queue
import socket
import struct
import threading
import time

RX_PORT = 2001  # device -> GNU Radio (UDP source)
TX_PORT = 2002  # GNU Radio (TCP sink) -> device


class DuplexClient:
    """Shares one serial port between RX and TX.

    A single reader thread owns serial reads: RX frames go to rx_queue,
    everything else goes to resp_queue. Writes happen only from the TX thread,
    and nothing calls reset_input_buffer(), so RX data is never dropped.
    """

    def __init__(self, client: DeviceClient):
        self.client = client
        self.rx_queue = queue.Queue(maxsize=1024)
        self.resp_queue = queue.Queue()
        self.running = threading.Event()
        self.rx_dropped = 0
        self.reader = threading.Thread(target=self._read_loop, daemon=True)

    def start(self):
        self.running.set()
        self.reader.start()

    def stop(self):
        self.running.clear()
        self.reader.join(timeout=5)

    def _read_loop(self):
        while self.running.is_set():
            try:
                frame = self.client.read_frame()
            except TimeoutError:
                continue
            except ProtocolError as e:
                print(f"Reader: {e}")
                continue

            if frame["cmd"] == RESP_IQ_STREAM_RX:
                try:
                    self.rx_queue.put_nowait(frame["payload"])
                except queue.Full:
                    self.rx_dropped += 1
            elif frame["cmd"] == RESP_ERR and frame["seq"] == 0:
                # Unsolicited device error (e.g. ERR_FIFO_OVERFLOW), not a reply to any request
                payload = frame["payload"]
                print(f"Device error: {ERR_NAMES.get(payload[0], payload[0]) if payload else '?'}")
            else:
                self.resp_queue.put(frame)

    def send(self, cmd, payload=b""):
        seq = self.client.next_seq()
        if seq == 0:  # seq 0 is used by unsolicited device errors
            seq = self.client.next_seq()
        self.client.serial.write(build_frame(cmd, seq, payload))
        return seq

    def request(self, cmd, cmd_resp=RESP_ACK, payload=b"", timeout=3.0):
        seq = self.send(cmd, payload)
        deadline = time.time() + timeout
        while True:
            remaining = deadline - time.time()
            if remaining <= 0:
                raise TimeoutError(f"no response to cmd 0x{cmd:02X}")
            try:
                frame = self.resp_queue.get(timeout=remaining)
            except queue.Empty:
                continue
            if frame["seq"] == seq:
                break

        if frame["cmd"] == RESP_ERR:
            payload = frame["payload"]
            if len(payload) >= 2:
                err_name = ERR_NAMES.get(payload[0], f"UNKNOWN_ERROR_{payload[0]}")
                raise ProtocolError(f"device returned {err_name} (code={payload[0]}, detail={payload[1]})")
            raise ProtocolError("device returned malformed error frame")
        if frame["cmd"] != cmd_resp:
            raise ProtocolError(f"unexpected response 0x{frame['cmd']:02X}")
        return frame["payload"]

    def iq_stream_tx_info(self, payload=b""):
        resp = self.request(CMD_IQ_STREAM_TX_INFO, cmd_resp=RESP_IQ_STREAM_TX_INFO, payload=payload)
        return struct.unpack("<HHHHHHH", resp)

    def send_iq_stream_tx(self, payload=b""):
        self.send(CMD_IQ_STREAM_TX, payload)


def rx_loop(duplex: DuplexClient, stop: threading.Event):
    # UDP so the GNU Radio UDP Source survives restarts of this script (TCP Source accepts only once)
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    try:
        while not stop.is_set():
            try:
                data = duplex.rx_queue.get(timeout=0.5)
            except queue.Empty:
                continue
            # One RX frame = 64 samples = 512 bytes cf32, matches the UDP Source payload size
            try:
                sock.sendto(u12_bytes_to_cf32(data), ("127.0.0.1", RX_PORT))
            except OSError:
                pass  # flowgraph not running yet: drop and keep streaming
    finally:
        sock.close()


def tx_loop(duplex: DuplexClient, dds, stop: threading.Event):
    iq_data = b""
    consumtion_fail2, dac_overflow2, tx_usb_overflow2, rx_usb_overflow2, adc_overflow2 = 0, 0, 0, 0, 0
    while not stop.is_set():
        BS, request_size, consumtion_fail, dac_overflow, tx_usb_overflow, rx_usb_overflow, adc_overflow = duplex.iq_stream_tx_info(payload=iq_data)
        if request_size >= BS:
            n = request_size // BS
            k = request_size % BS
            for _ in range(n - 1):
                duplex.send_iq_stream_tx(payload=dds.get_iq(BS // 4))

            last_chunk = dds.get_iq(BS // 4)
            if k >= 4:
                duplex.send_iq_stream_tx(payload=last_chunk)
                iq_data = dds.get_iq(k // 4)
            else:
                iq_data = last_chunk
        elif request_size >= 4:
            iq_data = dds.get_iq(request_size // 4)
        else:
            iq_data = b""

        if dac_overflow2 != dac_overflow or tx_usb_overflow2 != tx_usb_overflow or rx_usb_overflow2 != rx_usb_overflow or consumtion_fail != consumtion_fail2 or adc_overflow != adc_overflow2:
            print(f"Send IQ response: free={request_size}, dac_underrun={consumtion_fail}, dac_overflow={dac_overflow}, usb_to_host_overflow={tx_usb_overflow}, usb_from_host_overflow={rx_usb_overflow}, adc_overflow={adc_overflow}, rx_dropped={duplex.rx_dropped}")
            dac_overflow2, tx_usb_overflow2, rx_usb_overflow2, consumtion_fail2, adc_overflow2 = dac_overflow, tx_usb_overflow, rx_usb_overflow, consumtion_fail, adc_overflow


def main():
    client = DeviceClient()
    print(f"Using port {client.port}")

    resp = client.ping()
    print(f"Ping response: {resp.decode()}")

    dds = GnuRadioSink(host="127.0.0.1", port=TX_PORT)

    # Start commands go through the plain client before the reader thread takes over the port
    client.start_tx()
    client.start_rx()

    duplex = DuplexClient(client)
    duplex.start()

    stop = threading.Event()
    rx_thread = threading.Thread(target=rx_loop, args=(duplex, stop), daemon=True)
    rx_thread.start()

    try:
        tx_loop(duplex, dds, stop)
    except KeyboardInterrupt:
        pass
    finally:
        stop.set()
        rx_thread.join(timeout=2)
        try:
            duplex.request(CMD_IQ_STREAM_RX_STOP)
            duplex.request(CMD_IQ_STREAM_TX_STOP)
        finally:
            duplex.stop()
            dds.close()
            client.close()


if __name__ == "__main__":
    main()
