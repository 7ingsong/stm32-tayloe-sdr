from iqlib import DeviceClient, cf32_bytes_to_u12
import time
import math
import socket
import matplotlib.pyplot as plt

def calc_inc(f_out, f_clk):
    return int((1 << 32) * f_out / f_clk)

class GnuRadioSink:
    def __init__(self, host: str = "127.0.0.1", port: int = 2000, bufsize: int = 65536):
        self.host = host
        self.port = port
        self.bufsize = bufsize
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.sock.connect((self.host, self.port))
        self._buf = bytearray()

    def _recv_exact(self, size: int) -> bytes:
        while len(self._buf) < size:
            data = self.sock.recv(self.bufsize)
            if not data:
                raise ConnectionError("GNU Radio TCP source closed the connection")
            self._buf.extend(data)
        chunk = bytes(self._buf[:size])
        del self._buf[:size]
        return chunk

    def get_iq(self, n_samples):
        raw = self._recv_exact(n_samples * 8)
        return cf32_bytes_to_u12(raw)

    def close(self):
        self.sock.close()


class SinTxNoLUT:
    BITS = 12  # DAC resolution (12-bit: 0..4095)
    
    def __init__(self, f_out=1_000, f_clk=64_000):
        self.PHASE_INC = calc_inc(f_out, f_clk)
        self.phase = 0
        
        # Scaling parameters for 12-bit unsigned DAC
        self.max_val = (1 << self.BITS) - 1  # 4095
        self.mid_val = self.max_val / 2.0    # 2047.5

    def next(self):
        # 1. Update 32-bit phase accumulator
        self.phase = (self.phase + self.PHASE_INC) & 0xFFFFFFFF
        
        # 2. Convert 32-bit phase to radians: [0, 2^32) -> [0, 2*pi)
        rad_sin = (self.phase / (1 << 32)) * 2.0 * math.pi
        
        # 3. Add pi/2 (90 degrees) for the Cosine component
        rad_cos = rad_sin + (math.pi / 2.0)
        
        # 4. Compute sine and cosine using math.sin()
        sin_val = round(self.mid_val + self.mid_val * math.sin(rad_sin))
        cos_val = round(self.mid_val + self.mid_val * math.sin(rad_cos))
        
        return sin_val, cos_val

    def get_iq(self, n_samples):
        iq_data = bytearray()
        for _ in range(n_samples):
            s, c = self.next()
            iq_data.extend(s.to_bytes(2, 'little'))
            iq_data.extend(c.to_bytes(2, 'little'))
        return iq_data
    
class SinTx:
    BITS = 12
    
    def __init__(self, f_out=1_000, f_clk=64_000, N = 32):
        self.N = N
        self.PHASE_INC = calc_inc(f_out, f_clk)
        self.phase = 0
        self.sin_lut = []
        
        max_val = (1 << self.BITS) - 1  # 4095
        mid_val = max_val / 2.0         # 2047.5
        
        for i in range(self.N):
            v = round(mid_val + mid_val * math.sin(2 * math.pi * i / self.N))
            self.sin_lut.append(v)

    def next(self):
        self.phase = (self.phase + self.PHASE_INC) & 0xFFFFFFFF        
        index = (self.phase >> 27) & (self.N - 1)
        cos_index = (index + (self.N // 4)) % self.N        
        return self.sin_lut[index], self.sin_lut[cos_index]

    def get_iq(self, n_samples):
        iq_data = bytearray()
        for _ in range(n_samples):
            s, c = self.next()
            iq_data.extend(s.to_bytes(2, 'little'))
            iq_data.extend(c.to_bytes(2, 'little'))
        return iq_data

class GenMeander:
    BITS = 12  # DAC resolution (12-bit: 0..4095)

    def __init__(self, f_out=1_000, f_clk=64_000):
        self.PHASE_INC = calc_inc(f_out, f_clk)
        self.phase = 0

        self.max_val = (1 << self.BITS) - 1  # 4095

    def next(self):
        # 1. Update 32-bit phase accumulator
        self.phase = (self.phase + self.PHASE_INC) & 0xFFFFFFFF

        # 2. I: high for first half of the cycle, low for the second half
        sin_val = self.max_val if self.phase < (1 << 31) else 0

        # 3. Q: same square wave shifted by 90 degrees (quarter cycle)
        cos_phase = (self.phase + (1 << 30)) & 0xFFFFFFFF
        cos_val = self.max_val if cos_phase < (1 << 31) else 0

        return sin_val, cos_val

    def get_iq(self, n_samples):
        iq_data = bytearray()
        for _ in range(n_samples):
            s, c = self.next()
            iq_data.extend(s.to_bytes(2, 'little'))
            iq_data.extend(c.to_bytes(2, 'little'))
        return iq_data

def draw_plot():
    # --- Parameters: edit these as needed ---
    f_out = 16
    f_clk = 32
    n_samples = 16
 
    gen = GenMeander(f_out=f_out, f_clk=f_clk)
 
    sins, coss = [], []
    for _ in range(n_samples):
        s, c = gen.next()
        sins.append(s)
        coss.append(c)
 
    # --- Plot ---
    fig, ax = plt.subplots(figsize=(9, 5))
    ax.plot(sins, marker='o', label='I (sin)')
    ax.plot(coss, marker='s', label='Q (cos)')
    ax.set_title(f"12-bit DAC I/Q Output (f_out={f_out} Hz, f_clk={f_clk} Hz)")
    ax.set_xlabel("Sample index")
    ax.set_ylabel("DAC code (0-4095)")
    ax.set_ylim(-100, 4195)
    ax.grid(True, alpha=0.3)
    ax.legend()
    fig.tight_layout()
 
    fig.savefig("sintx_plot.png", dpi=150)
    plt.show()
 
    print("I values:", sins)
    print("Q values:", coss)

def main():
    # dds = GnuRadioSink(host="127.0.0.1", port=2000)
    # dds = SinTx(f_out=3000, f_clk=64000)
    dds = SinTxNoLUT(f_out=1000, f_clk=64000)
    
    # dds = GenMeander(f_out=1000, f_clk=64000)

    client = DeviceClient()
    print(f"Using port {client.port}")

    resp = client.ping()
    print(f"Ping response: {resp.decode()}")
    
    client.start_tx()
    iq_data = b""
    consumtion_fail2, dac_overflow2, tx_usb_overflow2, rx_usb_overflow2 = 0, 0, 0, 0
    deadline = time.time() + 10 #60
    while (deadline-time.time())>0:        
        BS, request_size, consumtion_fail, dac_overflow, tx_usb_overflow, rx_usb_overflow = client.cmd_iq_stream_tx_info(payload=iq_data)
        # print(f"Send IQ response: {request_size}, {consumtion_fail}, {dac_overflow}, {tx_usb_overflow}, {rx_usb_overflow}")
        if request_size>=BS:
            n = request_size//BS
            k = request_size%BS
            for _ in range(n-1):
                iq_data = dds.get_iq(BS//4)
                client.send_iq_stream_tx(payload=iq_data)

            last_chunk = dds.get_iq(BS//4)
            if k>=4:
                client.send_iq_stream_tx(payload=last_chunk)
                iq_data = dds.get_iq(k//4)
            else:
                iq_data = last_chunk
        elif request_size>=4:
            iq_data = dds.get_iq(request_size//4)
        else:
            iq_data = b""

        if dac_overflow2 != dac_overflow or tx_usb_overflow2 != tx_usb_overflow or rx_usb_overflow2 != rx_usb_overflow or consumtion_fail != consumtion_fail2:
            print(f"Send IQ response: {request_size}, {dac_overflow}, {tx_usb_overflow}, {rx_usb_overflow}")
            dac_overflow2, tx_usb_overflow2, rx_usb_overflow2, consumtion_fail2 = dac_overflow, tx_usb_overflow, rx_usb_overflow, consumtion_fail
    client.stop_tx()


if __name__ == "__main__":
    main()