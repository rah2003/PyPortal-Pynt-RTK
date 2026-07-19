# Power Tree — decision doc (QUESTIONS.md Q8)

## Loads (measured/vendor figures)

| Load | Current | Source |
|---|---|---|
| SAMD51 @ 120 MHz | ~65 mA | typical M4 figure — measure at bring-up |
| TFT backlight (2.4") | ~40–80 mA | measure; PWM-dimmable (auto-dim is a Phase 4 feature) |
| ESP32 AirLift | ~250 mA peak (WiFi TX bursts), ~80 mA active | [Adafruit AirLift docs](https://learn.adafruit.com/adafruit-airlift-breakout) |
| ZED-F9P (Lite) | ~130 mA | ArduSimple/u-blox |
| HC977 antenna | ~21 mA typ (35 dB variant) | [Calian datasheet](https://www.tallysman.com/product/hc977-triple-band-helical-antenna-with-l-band/) |
| microSD write bursts | ~50–100 mA momentary | typical SDHC |

**System total ≈ 500–600 mA peak, ~350–450 mA typical.** Any single USB
bank port rated 1 A carries this with margin, and it is far above the
~50–100 mA auto-off threshold of picky banks — no keep-alive worries.

## Candidate shapes

### A. Single bank port, Lite fed through the Pynt (adopted — checks passed)

```
USB battery bank (1 port, ≥1 A)
 └── USB cable ──► Pynt micro-USB (5V VBUS)
                     ├── Pynt 3V3 reg ──► SAMD51 + TFT + AirLift + microSD
                     └── D4 JST VCC pin ──► Lite JST pin 1 (5V_IN)
                                              └── Lite's own 3V3 reg ──► F9P + antenna
Ground: D4 JST GND pin ──► Lite JST pin 6 (wired, not assumed)
```

One cable in the field, one thing to charge. The Lite regulates its own
3.3 V, so the ~150 mA GNSS load never touches the Pynt's 3.3 V regulator —
it only transits the Pynt's **5 V/VBUS copper**.

**Power/ground now come off the D4 JST connector itself** (not the I2C
STEMMA port as originally planned) — P1 below confirmed D3/D4 VCC reads
the same 5V as the STEMMA port, and D4 already carries a signal wire to
the Lite, so routing power off the same connector means the STEMMA port
stays fully unpopulated (this project has no I2C use). See `wiring.md`
for the full wire list.

**Blocking checks (completed at bring-up, 2026-07-17 —
`docs/hardware/bringup-log.md`):**

- [x] **P1 — D3/D4 JST power-pin voltage.** Measured 5V on both D3 and D4
      VCC pins, board powered from USB. This is what unlocked shape A's
      final form above — power sourced from D4 directly instead of a
      separate STEMMA connection.
- [x] **P2 — I2C STEMMA port 5 V confirmed.** Also measured 5V (jumper
      untouched, defaults intact) — but the port ended up unused per the
      D3/D4 decision above.
- [ ] **P3 — voltage drop under load.** Not yet done — do this at the
      power-only integration gate (Lite + antenna running, WiFi active,
      backlight full): measure at Lite JST pin 1, must stay ≥ 4.5 V.
- [ ] **P4 — 30-minute soak.** Not yet done — no brownout resets (SAMD51
      or F9P), no warm connectors.

### B. Two-feed fallback (zero doubt, more cabling)

```
USB battery bank (2 ports)
 ├── Port 1 ──► Pynt micro-USB
 └── Port 2 ──► USB breakout/pigtail ──► Lite JST pin 1 (5V_IN) + pin 6 (GND)
Common ground: REQUIRED — Lite pin 6 to Pynt GND (UART reference).
```

Adopt if any of P2–P4 fails. Port 2's ~150 mA load is above most banks'
auto-off floor but nearer to it — soak-test the actual bank (the Feather
project's bank-auto-off caution applies).

## Rules

1. **Never** feed the Lite from a Pynt 3.3 V pin (P-rail budget) or into
   the XBee socket VCC (it's an output).
2. The Pynt has **no battery charger** — there is no LiPo UPS story here.
   If the bank dies, everything dies together. Firmware treats "GNSS
   stream stopped" and undervoltage as flush-and-close triggers for the
   SD log regardless.
3. u-center USB adapter power is fine simultaneously with other supplies
   (ArduSimple-sanctioned) — the simultaneous-**data** prohibition in
   `wiring.md` is what matters.

## Smoke-test order (stop at any failure)

1. Bank + Pynt alone: boots, TFT lights, WiFi joins, SD mounts.
2. u-center pass on the Lite via its USB adapter, Pynt disconnected
   (`ucenter-config.md`) — config verified, adapter unplugged.
3. Bank + Pynt + Lite, **power wires only** (1 and 4): Lite power LED on,
   Pynt unaffected, run checks P2–P3.
4. Add data wires 2 and 3: NMEA visible in the Pynt's serial console.
5. Check P4 soak, then full-stack field-sim soak: NTRIP + logging +
   backlight 1 h; SD file grows; no resets.
