import sys
from iqlib import DeviceClient

# On-board transmitter mic gain: python3 mic_gain.py 0..255 (0 = silence, 16 at boot, each doubling +6 dB); no argument: show it
def main():
    if len(sys.argv) > 2 or (len(sys.argv) == 2 and not (sys.argv[1].isdigit() and int(sys.argv[1]) <= 255)):
        sys.exit("usage: python3 mic_gain.py [0..255]")

    client = DeviceClient()
    gain = client.set_mic_gain(int(sys.argv[1]) if len(sys.argv) == 2 else None)
    print(f"Mic gain = {gain}")
    client.close()


if __name__ == "__main__":
    main()
