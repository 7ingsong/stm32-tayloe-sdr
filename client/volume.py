import sys
from iqlib import DeviceClient

# On-board receiver volume: python3 volume.py 0..255 (0 = mute, 16 at boot, each doubling +6 dB); no argument: show it
def main():
    if len(sys.argv) > 2 or (len(sys.argv) == 2 and not (sys.argv[1].isdigit() and int(sys.argv[1]) <= 255)):
        sys.exit("usage: python3 volume.py [0..255]")

    client = DeviceClient()
    volume = client.set_volume(int(sys.argv[1]) if len(sys.argv) == 2 else None)
    print(f"Volume = {volume}")
    client.close()


if __name__ == "__main__":
    main()
