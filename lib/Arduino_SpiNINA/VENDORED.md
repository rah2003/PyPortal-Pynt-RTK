# Arduino_SpiNINA 0.0.2 — vendored, unpatched

Source: **arduino-libraries/Arduino_SpiNINA, tag `0.0.2`**
(commit `200cc3594b0d80a64071d81efaf9d09a729af581`), unmodified.

The shared SPI transport that upstream WiFiNINA 2.x and ArduinoBLE 2.x
both drive (it owns `spi_drv.h`, which WiFiNINA's `wifi_drv.cpp`
includes). Vendored rather than registry-pinned because it is pre-1.0 and
thinly tested — this exact tree is what the coex work was verified
against.

Facts this project relies on:

- Pins come from **variant macros** at compile time: `SPIWIFI`,
  `SPIWIFI_SS`, `SPIWIFI_ACK`, `SPIWIFI_RESET`, `NINA_GPIO0`
  (`NINA_GPIOIRQ` defaults to `NINA_GPIO0`). `pyportal_m4` (8/5/7/6) and
  `metro_m4_airlift` (36/37/38/35) define the full set in Adafruit SAMD
  core 1.7.16; `feather_m0` defines none → the FeatherWing envs pass
  `-D` values (13/11/12/10).
- There is **no runtime `setPins()`** in this stack.
- `SpiDrv::available()` polls `NINA_GPIOIRQ` as the module's data-ready
  IRQ (nina-fw drives ESP32 GPIO0 as that output), and WiFiNINA's server
  paths call it — GPIO0 must be physically wired, hence the FeatherWing
  GPIO0 solder-jumper requirement in `docs/coex-bench.md`.
