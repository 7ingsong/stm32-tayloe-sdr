import sys
from iqlib import DeviceClient

USAGE = """usage: python3 spectrum.py [rise N] [fall N] [smooth 0|1]
  rise N    0..7, how fast a bin follows a rising level (0 = jump at once, higher = slower)
  fall N    0..7, how fast it falls back (higher = calmer noise)
  smooth    1 = average neighbouring bins (smoother line), 0 = sharper narrow signals
  no arguments: show the current values"""


def main():
    args = sys.argv[1:]
    if len(args) % 2:
        sys.exit(USAGE)
    params = {}
    for name, value in zip(args[::2], args[1::2]):
        limit = 1 if name == "smooth" else 7
        if name not in ("rise", "fall", "smooth") or not value.isdigit() or int(value) > limit:
            sys.exit(USAGE)
        params[name] = int(value)

    client = DeviceClient()
    rise, fall, smooth = client.set_spectrum(**params)
    print(f"rise {rise}, fall {fall}, smooth {int(smooth)}")
    client.close()


if __name__ == "__main__":
    main()
