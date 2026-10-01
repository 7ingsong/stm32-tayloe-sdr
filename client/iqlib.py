import time
import struct
import serial
import numpy as np
from serial.tools import list_ports


FRAME_MAGIC = b"\xA5\x5A"
FRAME_HEADER_SIZE = 8
FRAME_MAX_PAYLOAD = 2048

CMD_PING = 0x01
CMD_IQ_STREAM = 0x30
CMD_IQ_STREAM_TX = 0x31
CMD_IQ_STREAM_TX_INFO = 0x32
CMD_IQ_STREAM_TX_START = 0x33
CMD_IQ_STREAM_TX_STOP = 0x34
CMD_IQ_STREAM_RX_START = 0x35
CMD_IQ_STREAM_RX_STOP = 0x36

RESP_ACK = 0x80
RESP_ERR = 0x81
RESP_IQ_STREAM = 0xB0
RESP_IQ_STREAM_RX = 0xB1
RESP_IQ_STREAM_TX_INFO = 0xB2

ERR_NAMES = {
    1: "ERR_BAD_MAGIC",
    2: "ERR_BAD_LENGTH",
    3: "ERR_BAD_CRC",
    4: "ERR_BAD_COMMAND",
    5: "ERR_BAD_PAYLOAD",
    6: "ERR_FIFO_OVERFLOW",
    7: "ERR_EEPROM",
    8: "ERR_FPGA",
    9: "ERR_BAD_IMAGE",
}


class ProtocolError(RuntimeError):
    pass


def score_port(port_info):
    name = (port_info.device or "").lower()
    description = (port_info.description or "").lower()
    manufacturer = (port_info.manufacturer or "").lower()
    product = (port_info.product or "").lower()
    interface = (port_info.interface or "").lower()

    score = 0
    if "usbmodem" in name or "acm" in name:
        score += 5
    if "usbserial" in name or "ttyusb" in name:
        score += 3
    if "cdc" in description or "virtual com" in description:
        score += 5
    if "usb" in description:
        score += 2
    if "stm" in manufacturer or "stmicro" in manufacturer:
        score += 3
    if "stm" in product or "cdc" in product:
        score += 2
    if "cdc" in interface:
        score += 2

    return score


def auto_detect_port():
    ports = list(list_ports.comports())
    scored = []

    for port_info in ports:
        port_score = score_port(port_info)
        if port_score > 0:
            scored.append((port_score, port_info.device))

    scored.sort(key=lambda item: (-item[0], item[1]))
    if not scored:
        raise SystemExit("could not auto-detect a USB CDC port; pass --port explicitly")

    if len(scored) == 1 or scored[0][0] > scored[1][0]:
        return scored[0][1]

    raise SystemExit("multiple possible USB CDC ports found; pass --port explicitly")

def cf32_bytes_to_u12(data: bytes) -> bytes:
    f32 = np.frombuffer(data, dtype='<f4')  # little-endian float32

    if f32.size % 2 != 0:
        raise ValueError("Number of float32 values should be even (I,Q pairs)")

    # clip к [-1.0, 1.0]
    clipped = np.clip(f32, -1.0, 1.0)

    # [-1,1] -> [0,4095], 0.0 -> 2048
    u12 = np.rint(clipped * 2047.0 + 2048.0).astype(np.int32)
    u12 = np.clip(u12, 0, 4095).astype('<u2')  # little-endian uint16

    return u12.tobytes()

def u12_bytes_to_cf32(data: bytes) -> bytes:
    u12 = np.frombuffer(data, dtype='<u2')  # little-endian uint16

    # [0,4095] -> [-1,1], 2048 -> 0.0
    f32 = ((u12.astype(np.float32) - 2048.0) / 2047.0).astype('<f4')

    return f32.tobytes()

def checksum16(init, data):
    total = init & 0xFFFF
    for value in data:
        total = (total + value) & 0xFFFF
    return total


def frame_checksum(cmd, seq, payload):
    length = len(payload)
    prefix = bytes((cmd, seq, length & 0xFF, (length >> 8) & 0xFF))
    total = checksum16(0, prefix)
    return checksum16(total, payload)


def build_frame(cmd, seq, payload=b""):
    if len(payload) > FRAME_MAX_PAYLOAD:
        raise ValueError(f"payload is too large: {len(payload)}")

    crc = frame_checksum(cmd, seq, payload)
    return bytes(
        (
            FRAME_MAGIC[0],
            FRAME_MAGIC[1],
            cmd,
            seq,
            len(payload) & 0xFF,
            (len(payload) >> 8) & 0xFF,
            crc & 0xFF,
            (crc >> 8) & 0xFF,
        )
    ) + payload


class DeviceClient:
    def __init__(self, port=None, baudrate=115200, timeout = 3.0):
        if port==None:
            port = auto_detect_port()
        self.port = port
        self.serial = serial.Serial(port=port, baudrate=baudrate, timeout=timeout)
        self.seq = 1
        self.queue_data = bytearray()

    def close(self):
        self.serial.close()

    def next_seq(self):
        value = self.seq
        self.seq = (self.seq + 1) & 0xFF
        return value

    def read_exact(self, size):
        data = bytearray()
        while len(data) < size:
            chunk = self.serial.read(size - len(data))
            if not chunk:
                raise TimeoutError(f"timed out while waiting for {size} bytes")
            data.extend(chunk)
        return bytes(data)

    def read_frame(self):
        while True:
            first = self.serial.read(1)
            if not first:
                raise TimeoutError("timed out while waiting for frame start")
            if first != FRAME_MAGIC[:1]:
                continue

            second = self.serial.read(1)
            if not second:
                raise TimeoutError("timed out while waiting for frame magic")
            if second != FRAME_MAGIC[1:2]:
                continue

            header = self.read_exact(FRAME_HEADER_SIZE - 2)
            cmd = header[0]
            seq = header[1]
            length = header[2] | (header[3] << 8)
            crc = header[4] | (header[5] << 8)

            if length > FRAME_MAX_PAYLOAD:
                raise ProtocolError(f"invalid payload length {length}")

            payload = self.read_exact(length)
            calc_crc = frame_checksum(cmd, seq, payload)
            if calc_crc != crc:
                raise ProtocolError(
                    f"CRC mismatch: got 0x{crc:04X}, expected 0x{calc_crc:04X}, seq={seq}"
                )
            return {"cmd": cmd, "seq": seq, "payload": payload}

    def req_command(self, cmd, cmd_resp=RESP_ACK, payload=bytes()):
        seq = self.next_seq()
        self.serial.reset_input_buffer()
        self.serial.write(build_frame(cmd, seq, payload))

        while True:
            response = self.read_frame()

            if response["cmd"] == RESP_IQ_STREAM_RX:
                continue

            if response["seq"] != seq:
                continue

            break

        if response["cmd"] == RESP_ERR:
            payload = response["payload"]
            if len(payload) >= 2:
                err_code = payload[0]
                err_detail = payload[1]
                err_name = ERR_NAMES.get(err_code, f"UNKNOWN_ERROR_{err_code}")
                raise ProtocolError(
                    f"device returned {err_name} (code={err_code}, detail={err_detail})"
                )
            raise ProtocolError("device returned malformed error frame")

        if response["cmd"] != cmd_resp:
            raise ProtocolError(f"unexpected response 0x{response['cmd']:02X}")

        return response["payload"]

    def ping(self):
        return self.req_command(CMD_PING, cmd_resp=RESP_ACK)

    def start_tx(self):
        return self.req_command(CMD_IQ_STREAM_TX_START, cmd_resp=RESP_ACK)

    def stop_tx(self):
        return self.req_command(CMD_IQ_STREAM_TX_STOP, cmd_resp=RESP_ACK)

    def start_rx(self):
        return self.req_command(CMD_IQ_STREAM_RX_START, cmd_resp=RESP_ACK)

    def stop_rx(self):
        return self.req_command(CMD_IQ_STREAM_RX_STOP, cmd_resp=RESP_ACK)

    def cmd_iq_stream_tx_info(self, payload=bytes()):
        resp = self.req_command(CMD_IQ_STREAM_TX_INFO, cmd_resp=RESP_IQ_STREAM_TX_INFO, payload=payload)
        bs, free_space, consumtion_fail, dac_overflow, tx_usb_overflow, rx_usb_overflow = struct.unpack("<HHHHHH", resp)
        return bs, free_space, consumtion_fail, dac_overflow, tx_usb_overflow, rx_usb_overflow

    def get_rx_iq_samples(self, timeout=5.0):
        deadline = time.time() + timeout
        while time.time() < deadline:
            try:
                frame = self.read_frame()
                break
            except TimeoutError:
                continue

        if frame["cmd"] != RESP_IQ_STREAM_RX:
            raise ProtocolError(f"unexpected response 0x{frame['cmd']:02X} {frame['payload'].hex()}")

        # print(f"frame seq={frame['seq']} length={len(frame['payload'])}")
        return frame["payload"]

    def get_iq_stream_tx_info(self, timeout=5.0):
        deadline = time.time() + timeout
        while time.time() < deadline:
            try:
                frame = self.read_frame()
                break
            except TimeoutError:
                continue

        if frame["cmd"] != RESP_IQ_STREAM_TX_INFO:
            raise ProtocolError(f"unexpected response 0x{frame['cmd']:02X} {frame['payload'].hex()}")

        print(f"frame seq={frame['seq']} length={len(frame['payload'])}")

        resp = frame["payload"]
        request_size, overflow, tx_usb_overflow, rx_usb_overflow = struct.unpack("<IIII", resp)
        return request_size, overflow, tx_usb_overflow, rx_usb_overflow

    def send(self, cmd, payload=bytes()):
        seq = self.next_seq()
        self.serial.reset_input_buffer()
        self.serial.write(build_frame(cmd, seq, payload))

    def send_iq_stream_tx(self, payload=bytes()):
        self.send(CMD_IQ_STREAM_TX, payload=payload)

if __name__ == "__main__":
    pass