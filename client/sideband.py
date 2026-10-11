import sys
from iqlib import DeviceClient

MODES = {"usb": DeviceClient.SIDEBAND_USB, "lsb": DeviceClient.SIDEBAND_LSB}
NAMES = {DeviceClient.SIDEBAND_USB: "USB", DeviceClient.SIDEBAND_LSB: "LSB"}

# On-board receiver and transmitter sideband: python3 sideband.py usb | lsb (no argument: show the current one)
def main():
    if len(sys.argv) > 2 or (len(sys.argv) == 2 and sys.argv[1] not in MODES):
        sys.exit("usage: python3 sideband.py [usb|lsb]")

    client = DeviceClient()
    sideband = client.set_sideband(MODES[sys.argv[1]] if len(sys.argv) == 2 else None)
    print(NAMES.get(sideband, sideband))
    client.close()


if __name__ == "__main__":
    main()
