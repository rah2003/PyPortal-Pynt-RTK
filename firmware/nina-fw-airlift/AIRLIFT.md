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

The 3.0.1 README's Docker path is the reproducible one (its manual
instructions still name the ancient IDF v3.3.1 toolchain; the Docker image
tag is authoritative for 3.0.x): **ESP-IDF v4.4.8** via `espressif/idf:v4.4.8`.

From this directory (Docker Desktop, Linux containers):

```bash
docker run --rm -v "$PWD":/data espressif/idf:v4.4.8 sh -c 'cd /data && make && python combine.py NINA_W102-3.0.1-airlift.bin'
```

`make` produces the bootloader/partition/app pieces plus the cert bundle;
`combine.py` packs them (bootloader @0x1000, partitions @0x8000, phy
@0x9000, certs @0xA000, app @0x30000, spiffs @0x1B0000) into one 2 MB
image **flashed at offset 0x0**. Flashing procedure, pass gates and
recovery: `docs/coex-bench.md`.

PowerShell note: replace `"$PWD"` with `${PWD}` (or the absolute path).

## Rollback / recovery

- **Primary**: before the first write on each board, dump the stock
  firmware — `esptool read_flash 0 0x200000 <board>-stock-backup.bin` —
  and keep it next to the built image. Restoring it is the exact inverse
  `write_flash 0` command.
- **Secondary**: stock Adafruit AirLift firmware binaries (`NINA_W102-1.7.x.bin`)
  are published at <https://github.com/adafruit/nina-fw/releases>; they
  also flash at offset 0x0.

The ESP32's ROM serial bootloader is mask ROM — a bad or interrupted flash
is always recoverable by re-entering the passthrough and reflashing. This
is a reflash-recoverable part, not a brickable one.
