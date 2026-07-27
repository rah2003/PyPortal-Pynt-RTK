# ArduinoBLE 2.1.0 — vendored, 2-line AirLift patch

Source: **arduino-libraries/ArduinoBLE, tag `2.1.0`**
(commit `281377b3588814e4c174c08ec711e10e35b1c9f9`), `src/` +
metadata only (examples/docs dropped).

Why vendored: upstream gates its NINA **SPI** HCI transport
(`HCINinaSpiTransport.cpp`, the nina-fw 3.x path) on exactly four Arduino
board macros — `ARDUINO_AVR_UNO_WIFI_REV2`, `ARDUINO_SAMD_MKRWIFI1010`,
`ARDUINO_SAMD_NANO_33_IOT`, `ARDUINO_NANO_RP2040_CONNECT`. Adafruit
AirLift boards define none of them, and with no transport compiled in the
global `HCITransport` symbol doesn't exist → link failure. The protocol
itself is board-agnostic once nina-fw >= 3.0.0 is on the module.

Patch (marked `AIRLIFT PATCH` in-source): `|| defined(BLE_NINA_SPI_TRANSPORT)`
appended to the gate in

- `src/utility/HCINinaSpiTransport.cpp` (transport + `HCITransport` instantiation)
- `src/local/BLELocalDevice.cpp` (the matching error-message block only)

Enable with `-DBLE_NINA_SPI_TRANSPORT` (set by the coex envs in
`platformio.ini`). Without the define the library compiles exactly as
upstream. Depends on `lib/Arduino_SpiNINA` for the shared SPI link.
