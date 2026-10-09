import sys
from iqlib import DeviceClient

MODES = {"off": DeviceClient.RADIO_RX, "on": DeviceClient.RADIO_TX, "duplex": DeviceClient.RADIO_DUPLEX}
NAMES = {DeviceClient.RADIO_RX: "RX (SSB -> I2S)", DeviceClient.RADIO_TX: "TX (mic -> SSB -> DAC)",
         DeviceClient.RADIO_DUPLEX: "DUPLEX (RX on I2S + TX from mic at once)"}

# On-board radio: python3 ptt.py on | off | duplex (no argument: show the current mode)
def main():
    if len(sys.argv) > 2 or (len(sys.argv) == 2 and sys.argv[1] not in MODES):
        sys.exit("usage: python3 ptt.py [on|off|duplex]")

    client = DeviceClient()
    mode = client.set_ptt(MODES[sys.argv[1]] if len(sys.argv) == 2 else None)
    print(NAMES.get(mode, mode))
    client.close()


if __name__ == "__main__":
    main()
