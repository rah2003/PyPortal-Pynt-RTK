"""esptool wrapper for the SAMD passthrough bridge.

esptool on Windows forces DTR/RTS low before opening the port
(loader.py "avoid unwanted chip reset" workaround), but the Adafruit
SAMD USB-CDC core drops device->host bytes unless the host asserts
DTR/RTS — so stock esptool never sees the ROM loader's replies.
Here we open the port ourselves with both lines asserted and hand the
pre-connected loader to esptool.main(esp=...).

Usage: flash_via_bridge.py COM9 read-flash 0 0x200000 backup.bin
       flash_via_bridge.py COM9 write-flash 0 firmware.bin
"""
import sys
import serial
import esptool
import esptool.loader
from esptool.targets import ESP32ROM

# This host has stalled USB CDC transfers before (bench note 2026-07-19:
# Windows buffered CDC traffic and delivered it in one burst). esptool's
# 3 s per-read timeout aborts the whole flash on any such hiccup — give
# it room instead.
esptool.loader.DEFAULT_TIMEOUT = 30

port_name, cmd_args = sys.argv[1], sys.argv[2:]

ser = serial.Serial()
ser.port = port_name
ser.baudrate = 115200
ser.dtr = True
ser.rts = True
ser.open()

esp = ESP32ROM(ser, baud=115200)
esp.connect("no-reset")
print(f"Connected via bridge: {esp.get_chip_description()}")

# no-reset-stub: leave the stub running; the plain no-reset teardown does a
# stub->ROM soft reset that has failed flaky here, and the next sketch's own
# boot hard-resets the module anyway.
esptool.main(["--baud", "115200", "--after", "no-reset-stub"] + cmd_args, esp=esp)
