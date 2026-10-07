import sys
from iqlib import DeviceClient

# On-board half-duplex radio: python3 ptt.py on | off (no argument: show the current state)
def main():
    if len(sys.argv) > 2 or (len(sys.argv) == 2 and sys.argv[1] not in ("on", "off")):
        sys.exit("usage: python3 ptt.py [on|off]")

    client = DeviceClient()
    on = None if len(sys.argv) == 1 else sys.argv[1] == "on"
    state = client.set_ptt(on)
    print("TX (mic -> SSB -> DAC)" if state else "RX (SSB -> I2S)")
    client.close()


if __name__ == "__main__":
    main()
