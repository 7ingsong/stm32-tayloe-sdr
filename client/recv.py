from iqlib import DeviceClient, ProtocolError, u12_bytes_to_cf32
import socket
import time

RX_PORT = 2001  # GNU Radio UDP Source, payload size 512 (one RX frame = 64 samples cf32)


def main():
    client = DeviceClient()
    print(f"Using port {client.port}")

    # UDP so the flowgraph can be started or restarted at any time
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)

    f = open("samples.cf32", "wb+")

    resp = client.ping()
    print(f"Ping response: {resp.decode()}")
    client.start_rx()

    try:
        deadline = time.time() + 100000
        while (deadline - time.time()) > 0:
            try:
                data = client.get_rx_iq_samples()
            except ProtocolError as e:
                # Stray non-RX frame (e.g. a device error report): skip it, keep streaming
                print(f"Skipped frame: {e}")
                continue
            except TimeoutError as e:
                print(f"{e}, still waiting")
                continue
            iq = u12_bytes_to_cf32(data)
            f.write(iq)
            f.flush()
            try:
                sock.sendto(iq, ("127.0.0.1", RX_PORT))
            except OSError:
                pass  # flowgraph not running yet: drop and keep streaming
    except KeyboardInterrupt:
        pass
    finally:
        client.stop_rx()
        f.close()
        sock.close()
        client.close()


if __name__ == "__main__":
    main()
