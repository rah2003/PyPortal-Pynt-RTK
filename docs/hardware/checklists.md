# Per-Board Bring-Up Checklists

Work through these before any inter-board wiring. Sources cited so items
can be re-verified against current vendor docs.

## simpleRTK2B Lite (ZED-F9P) — moved from the RTK-Feather rig

- [ ] Physical inspection after de-rigging from the Feather stack: JST-GH
      connector intact, SMA snug, no bent XBee-header pins.
- [ ] JST **pin 1 marking** located (white square / printed "1"); pigtail
      pin-1 end tagged. Wire colors untrusted
      ([Pixhawk cable notes](https://www.ardusimple.com/product/pixhawk-cable-set/)).
- [ ] **Top XBee socket empty** and kept empty (UART2 = future radio;
      socket VCC is a 3.3 V output — never feed power in).
- [ ] HC977 antenna on SMA: hand-tight + ⅛ turn, sky view for fix tests.
      No ground plane needed (helical).
- [ ] u-center **re-verification** pass done (`ucenter-config.md`),
      config survives power cycle, adapter unplugged afterward.

## PyPortal Pynt

- [ ] Board boots on USB; CircuitPython or prior contents noted, then
      Arduino bootloader confirmed working (double-tap reset → PORTALBOOT
      drive). Record bootloader version (UF2 boot screen); update via
      Adafruit's bootloader-updater UF2 if older than the Arduino-core
      minimum.
- [ ] **Power measurements P1 + P2** (`power.md`): D3/D4 JST power-pin
      voltage recorded; I2C STEMMA port pin measures ~VBUS (5 V default
      jumper intact, [pinouts](https://learn.adafruit.com/adafruit-pyportal/pinouts)).
- [ ] TFT smoke test: fill screens, text at `setRotation(0)` and `(2)` —
      pick the portrait rotation whose "up" matches the planned cable
      exit; record it.
- [ ] Touch smoke test: raw X/Y ADC values at the four corners recorded
      (calibration constants for Phase 2).
- [ ] AirLift check: `WiFi.firmwareVersion()` ≥ NINA 1.7.7; hotspot join;
      TCP echo to a laptop on the hotspot network (proves the SW Maps
      path before SW Maps enters the picture).
- [ ] microSD: PNY Elite 32 GB formatted FAT32; SdFat init at
      `SD_CS` (pin 32); **card latency test** — 4 MB in 512 B appends,
      histogram of per-write latency, record worst case (feeds the
      `platform.md` buffer numbers with real data).
- [ ] **D3/D4 label-swap + UART loopback test** (`wiring.md`) — sockets
      tape-labeled from measurement, SERCOM0 recipe compiles and echoes.
- [ ] Free-RAM high-water mark printed by the bring-up sketch recorded.

## Integration gates (in order, from `power.md` smoke-test)

1. Pynt alone healthy (all items above).
2. Lite alone healthy on u-center.
3. Power-only union (wires 1+4) — checks P3/P4.
4. Data union (wires 2+3) — NMEA in the Pynt console at 115200.
5. 1 h full-stack soak: NTRIP + logging + backlight; no resets; SD grows.
