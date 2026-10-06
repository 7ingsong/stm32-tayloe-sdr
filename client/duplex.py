from iqlib import (
    DeviceClient, ProtocolError, build_frame, u12_bytes_to_cf32,
    CMD_IQ_STREAM_TX, CMD_IQ_STREAM_TX_INFO, CMD_IQ_STREAM_TX_STOP, CMD_IQ_STREAM_RX_STOP, CMD_SET_FREQ,
    RESP_ACK, RESP_ERR, RESP_IQ_STREAM_RX, RESP_IQ_STREAM_TX_INFO, ERR_NAMES,
)
from send import GnuRadioSink
import queue
import socket
import struct
import sys
import threading

RX_PORT = 2001  # device -> GNU Radio (UDP source)
TX_PORT = 2002  # GNU Radio (TCP sink) -> device


class DuplexClient:
    """Shares one serial port between RX and TX.

    A single reader thread owns serial reads: RX frames go to rx_queue,
    replies are routed by seq to the thread waiting for them. Writes are
    serialized by a lock, so the TX loop and console commands can share the
    port, and nothing calls reset_input_buffer(), so RX data is never dropped.
    """

    def __init__(self, client: DeviceClient):
        self.client = client
        self.rx_queue = queue.Queue(maxsize=1024)
        self.pending = {}  # seq -> Queue for the reply
        self.write_lock = threading.Lock()
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
                waiter = self.pending.get(frame["seq"])
                if waiter is not None:
                    waiter.put(frame)

    def send(self, cmd, payload=b"", waiter=None):
        with self.write_lock:
            seq = self.client.next_seq()
            if seq == 0:  # seq 0 is used by unsolicited device errors
                seq = self.client.next_seq()
            if waiter is not None:
                self.pending[seq] = waiter  # register before writing so a fast reply isn't missed
            self.client.serial.write(build_frame(cmd, seq, payload))
        return seq

    def request(self, cmd, cmd_resp=RESP_ACK, payload=b"", timeout=3.0):
        waiter = queue.Queue(maxsize=1)
        seq = self.send(cmd, payload, waiter=waiter)
        try:
            frame = waiter.get(timeout=timeout)
        except queue.Empty:
            raise TimeoutError(f"no response to cmd 0x{cmd:02X}") from None
        finally:
            self.pending.pop(seq, None)

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

    def set_frequency(self, hz=None):
        payload = b"" if hz is None else struct.pack("<I", int(hz))
        return struct.unpack("<I", self.request(CMD_SET_FREQ, payload=payload))[0]


def console_loop(duplex: DuplexClient, stop: threading.Event):
    print("Commands: f <Hz> (e.g. f 7100000), f (show current), q (quit)")
    for line in sys.stdin:
        parts = line.split()
        if not parts:
            continue
        try:
            if parts[0] == "q":
                stop.set()
                return
            if parts[0] == "f" and len(parts) == 1:
                print(f"LO = {duplex.set_frequency()} Hz")
            elif parts[0] == "f":
                print(f"LO = {duplex.set_frequency(int(parts[1]))} Hz")
            else:
                print("Unknown command")
        except ValueError:
            print("Bad frequency, use Hz, e.g. 7100000")
        except (ProtocolError, TimeoutError) as e:
            print(f"Command failed: {e}")


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
    print(f"LO = {client.set_frequency()} Hz")

    dds = GnuRadioSink(host="127.0.0.1", port=TX_PORT)

    # Start commands go through the plain client before the reader thread takes over the port
    client.start_tx()
    client.start_rx()

    duplex = DuplexClient(client)
    duplex.start()

    stop = threading.Event()
    rx_thread = threading.Thread(target=rx_loop, args=(duplex, stop), daemon=True)
    rx_thread.start()
    threading.Thread(target=console_loop, args=(duplex, stop), daemon=True).start()

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
