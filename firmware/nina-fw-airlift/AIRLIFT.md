# nina-fw 3.0.1-airlift — vendored ESP32 module firmware

Vendored copy of **arduino/nina-fw, tag `3.0.1`** (commit `57d12f460aceb0d4f81102a7186ddfdbebe0bd5e`), the firmware that runs *on the AirLift's ESP32 (NINA-W102)*, not on the SAMD host.

Why this exists: nina-fw 3.0.x is where Arduino made WiFi + BLE
**simultaneous** — no boot-time WiFi-or-BLE strap, BLE controller in VHCI
mode, BLE HCI multiplexed onto the same SPI command server WiFi uses, and
ESP-IDF software coexistence on (`CONFIG_SW_COEXIST_ENABLE=y` /
`CONFIG_ESP32_WIFI_SW_COEXIST_ENABLE=y` in `sdkconfig`). Adafruit's own
nina-fw fork never adopted any of that (see `docs/QUESTIONS.md` Q18).

## The one source change

`arduino/libraries/SPIS/src/SPIS.cpp`, last line: SPI slave **MOSI 12 → 14**.
Adafruit AirLift boards route host-MOSI to ESP32 GPIO14 (GPIO12 is the
MTDI flash-voltage strap on their layout); MISO 23, SCK 18, CS 5 and
READY/BUSY 33 are identical to upstream. The image carries no other
host-board knowledge, so **one .bin serves the Metro M4 AirLift Lite, the
AirLift FeatherWing, and the PyPortal Pynt**.

## Building the monolithic .bin

Toolchain: **ESP-IDF v4.4.8** (the 3.0.1 README's manual instructions
still name the ancient IDF v3.3.1; its Docker tag `espressif/idf:v4.4.8`
is authoritative for 3.0.x). The build needs Linux — this is the legacy
GNU-make IDF build, which was never supported on native Windows.

**Verified path (2026-07-26, WSL2 Ubuntu-22.04, this repo's tree):**
one-time setup as root inside WSL —

```bash
apt-get install -y git wget flex bison gperf python3 python3-pip python3-setuptools python3-venv cmake ninja-build ccache libffi-dev libssl-dev dfu-util libusb-1.0-0 build-essential
git clone --depth 1 --branch v4.4.8 --recursive --shallow-submodules https://github.com/espressif/esp-idf.git /opt/esp-idf-v4.4.8
/opt/esp-idf-v4.4.8/install.sh esp32
```

then per build (copy the tree onto the Linux filesystem — building
straight on `/mnt/c` is slow):

```bash
cp -r /mnt/c/Projects/PyPortal-Pynt-RTK/firmware/nina-fw-airlift /root/nina-build
cd /root/nina-build && . /opt/esp-idf-v4.4.8/export.sh
RELEASE=1 make -j$(nproc)
python combine.py NINA_W102-3.0.1-airlift
cp NINA_W102-3.0.1-airlift_ALL.bin /mnt/c/Projects/PyPortal-Pynt-RTK/firmware/nina-fw-airlift/NINA_W102-3.0.1-airlift.bin
```

Docker alternative (same toolchain, untested here): from this directory,
`docker run --rm -v "$PWD":/data espressif/idf:v4.4.8 sh -c 'cd /data && RELEASE=1 make && python combine.py NINA_W102-3.0.1-airlift'`.

`combine.py <base>` writes four files; the one to flash is
**`<base>_ALL.bin`** — the full padded 2 MB image (bootloader @0x1000,
partitions @0x8000, phy @0x9000, certs @0xA000, app @0x30000, spiffs
@0x1B0000), **flashed at offset 0x0**. It also overwrites the old
firmware's spiffs/nvs regions, which is what you want when converting
from Adafruit firmware. (`<base>.bin`/`_BOOT_APP.bin` truncate after the
app; `_APP.bin` is the app alone for 0x30000 updates.) Sanity checks on
a good image: size exactly 2,097,152 bytes; byte `0xE9` at 0x1000 and
0x30000. Flashing procedure, pass gates and recovery:
`docs/coex-bench.md`.

## Rollback / recovery

- **Primary**: before the first write on each board, dump the stock
  firmware — `esptool read-flash 0 0x200000 <board>-stock-backup.bin` —
  and keep it next to the built image. Restoring it is the exact inverse
  `write-flash 0` command.
- **Secondary**: stock Adafruit AirLift firmware binaries (`NINA_W102-1.7.x.bin`)
  are published at <https://github.com/adafruit/nina-fw/releases>; they
  also flash at offset 0x0.

The ESP32's ROM serial bootloader is mask ROM — a bad or interrupted flash
is always recoverable by re-entering the passthrough and reflashing. This
is a reflash-recoverable part, not a brickable one.
