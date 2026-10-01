from iqlib import DeviceClient, u12_bytes_to_cf32
import socket
import time

def main():
    client = DeviceClient()
    print(f"Using port {client.port}")

    sock = socket.socket()
    sock.connect(("127.0.0.1", 2000))

    f = open ("samples.cf32","wb+")

    resp = client.ping()
    print(f"Ping response: {resp.decode()}")
    client.start_rx()
    
    deadline = time.time()+100000
    while (deadline-time.time())>0:
        data = client.get_rx_iq_samples()
        iq = u12_bytes_to_cf32(data)
        f.write(iq)
        f.flush()
        sock.sendall(iq)

    client.stop_rx()


if __name__ == "__main__":
    main()