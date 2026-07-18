# Bench Bring-Up Log

Working copy for the bench pass — fill in as you go, alongside
`checklists.md` (source of truth for *what* to check) and `wiring.md` /
`power.md` / `ucenter-config.md` (source of truth for *how*).

Date started: ____________  Date completed: ____________

---

## 1. simpleRTK2B Lite (ZED-F9P) — u-center re-verification

- [ ] Physical inspection: JST-GH intact, SMA snug, no bent XBee pins
- [ ] JST pin 1 located and marked on the pigtail
- [ ] Top XBee socket confirmed empty
- [ ] HC977 antenna mounted (hand-tight + 1/8 turn), sky view for fix tests
- [ ] u-center connected via XBee-to-USB adapter, **pigtail unplugged**
- [ ] Connection baud used: `_________` (115200 Feather-config / 38400 factory-default)
- [ ] UBX-MON-VER firmware recorded: `_______________` (want HPG 1.32+)
- [ ] UART1 keys verified/set (baud 115200, UBX+RTCM3X in, UBX+NMEA out, RTCM3X out = 0)
- [ ] RAWX / SFRBX / TIM_TM2 messages on, UART1
- [ ] NMEA set on UART1: GGA/RMC/GSA/GST/GSV = 1, VTG per preference, HIGHPREC = 1
- [ ] Rate 1 Hz (MEAS 1000ms, NAV 1), dynamics = portable
- [ ] Elevation mask = 12°, constellations GPS+GLO+GAL+BDS, SBAS/QZSS off
- [ ] UART2 left untouched
- [ ] Saved to Flash (RAM+BBR+Flash)
- [ ] Power-cycled, reconnected at 115200, VALGET spot-check from Flash layer passed
- [ ] Packet console clean (only RAWX+SFRBX+NMEA @ 1Hz)
- [ ] USB adapter unplugged when done

Notes / anomalies:



---

## 2. PyPortal Pynt — bring-up sketch (`pio run -e pynt-bringup -t upload`)

- [ ] Bootloader confirmed (double-tap reset → `PORTALBOOT`); version recorded: `_______________`
- [ ] Bring-up firmware flashed, serial monitor open at 115200

**Power (multimeter, USB-powered, nothing wired to the Lite yet)**
- [x] P1 — D3 JST power pin voltage: `5` V
- [x] P1 — D4 JST power pin voltage: `5` V
- [x] P2 — I2C STEMMA 5V pin voltage: `5` V (want ~VBUS/5V) — confirmed

**`t` — TFT**
- [x] Three fills clean, no tearing; fill time recorded: `_______` ms (not recorded)
- [x] rotation 0 "UP^" points to: `USB connector side`
- [x] rotation 2 "UP^" points to: `JST connectors side`
- [x] **Chosen rotation for the build:** `2` (JST connectors up — matches planned cable exit)

**`p` — Touch**
- [x] Corner top-right: x=169 y=0 z=148
- [x] Corner top-left: x=175 y=0 z=108
- [x] Corner bottom-right: x=855 y=0 z=125
- [x] Corner bottom-left: x=820 y=0 z=980
- [ ] **ANOMALY (reproduced on retest, not noise)**: all four readings show y=0 — see note below; do not use as calibration constants until resolved

**`w` / `W` — AirLift**
- [x] NINA firmware version: `_______________` (not recorded)
- [x] Hotspot SSID visible in scan
- [x] WiFi join succeeded
- [x] TCP echo test — connected, echo confirmed successful

**`s` — microSD**
- [x] Card formatted FAT32, init OK at SD_CS
- [ ] Latency histogram (bucket counts not recorded — only worst-case captured)
- [x] Worst single write: `93.6` ms

**`l` / `x` — D3/D4 label-swap**
- [x] Signal pins jumpered, loopback (`l`) = PASS
- [x] TX-identify (`x`) run (re-confirmed on retest): **physical socket D4 = steady 3.3V, physical socket D3 = 0V**
- [x] **Result: silkscreen IS swapped, as the forum reports warned.** UART TX
      idles high (3.3V) between the once-per-second byte writes — the pulse
      itself is a microsecond-scale burst a multimeter can't catch, so a
      steady 3.3V reading on the driven line is the *expected*, correct
      signature of TX, not a measurement failure. Firmware's `GNSS_TX_PIN`
      (Arduino pin 3) is physically brought out on the socket silkscreened
      **D4**; `GNSS_RX_PIN` (Arduino pin 4) is on the socket silkscreened
      **D3**.
- [x] **Sockets tape-labeled**: physical **D4 socket → TX**, physical **D3 socket → RX** (opposite of silkscreen). Use these tape labels, not the board printing, when wiring the Lite (`wiring.md` wire #2 goes to the tape-labeled TX socket, wire #3 from the tape-labeled RX socket).

**`r` — Free RAM**
- [x] Free-RAM low-water mark after all tests: `186008` bytes

Notes / anomalies:

- Touch (`p`): all four corners returned y=0, reproduced on a second run
  (not sampling noise). Traced into the Adafruit TouchScreen library
  (`getPoint()` in `TouchScreen.cpp`): with this constructor order
  (`TouchScreen ts(TOUCH_XL, TOUCH_YU, TOUCH_XR, TOUCH_YD, 300)`), X is read
  via `analogRead(TOUCH_YU)` (pin 19) and **Y is read via
  `analogRead(TOUCH_XR)` (pin 20 / PB08)**. Checked the pyportal_m4
  `variant.cpp` pin table directly — pin 20 is a genuine ADC-capable pin
  (`ADC_Channel2`, `PIN_ATTR_ANALOG`), so this is not a bad pin assignment
  in `pins.h`. X varies correctly across corners while Y pins to one rail
  every time, which is the classic signature of a **floating/disconnected
  analog input**, not a code or pin-mapping bug. Suspect the touch
  overlay's FPC/ribbon connector isn't fully seated, or a cold joint /
  broken trace on the lead that reaches pin 20 (PB08). **Next step is
  physical inspection** — reseat the touch panel's connector and check
  continuity from Arduino pin 20 to the touch overlay's X- lead — not a
  firmware change.
- TX-identify (`x`): re-confirmed on retest, same result both times — this
  is the correct, expected signature of TX (UART idles high; the
  once-per-second pulse is microseconds long and invisible to a
  multimeter). Silkscreen swap confirmed real; tape labels applied (see
  above). Not an anomaly — closed.



---

## 3. Integration gates (stop at any failure)

1. [ ] Section 1 + Section 2 both fully healthy
2. [ ] **Power-only union** (wires 1 + 4): Lite power LED on, Pynt unaffected
   - [ ] P3 — voltage at Lite JST pin 1 under load (WiFi active, backlight full): `_______` V (must stay ≥ 4.5 V)
   - [ ] P4 — 30-min soak, no brownout resets on either board
3. [ ] **Data union** (wires 2 + 3 added): `n` test shows live NMEA in the Pynt console at 115200
4. [ ] **1-hour full-stack soak**: NTRIP + logging + backlight running together, no resets, SD file confirmed growing

Notes / anomalies:



---

## Open items carried forward

(anything discovered during bring-up that needs a QUESTIONS.md entry or a doc update)

- **Touch Y-axis stuck at 0** (see note in section 2). Initial suspicion
  (library senses Y via `analogRead(TOUCH_XR)`, pin 20/PB08) was corrected
  by the new `y` raw-pin diagnostic test (`main_bringup.cpp`,
  `testTouchRawDump()`), which drives each axis and reads both
  opposite-axis electrodes at once instead of trusting the library's
  single-sense-pin math.
  **Result (two live captures, unpressed vs pressed):**
  unpressed `YU≈717 YD≈7 | XL≈459 XR≈450`; pressed
  `YU≈140 YD≈2-8 | XL≈1023 XR≈1017-1023`. YU tracks touch correctly
  (matches the already-good X readings). **YD stays flat near 0 in both
  states, and — the key signal — both XL *and* XR jump to ~1023 together
  the instant of touch instead of settling at a graded value.** Both sense
  pins pinning high together under touch (not just one) means the fault is
  on the **drive/ground side, not a sense pin**: Y-phase drives YU=HIGH,
  YD=LOW to form the gradient, and if YD isn't making a good low-reference
  connection, any touch just pulls toward the healthy HIGH (YU) side
  instead of a real graded voltage — exactly this signature.
  **ROOT CAUSE FOUND — not a hardware fault at all.** Pulled Adafruit's
  official "Adafruit PyPortal Pynt Pinout.pdf"
  (github.com/adafruit/Adafruit-PyPortal-PCB) and cross-checked against
  `pins.h`. The chip ports were right (PB00/PB01/PA06/PB08) but every
  touch pin's **Arduino number was off by exactly one**:

  | Signal | Official pin | `pins.h` had |
  |---|---|---|
  | TOUCH_YD (PB00) | 18 | 17 |
  | TOUCH_XL (PB01) | 19 | 18 |
  | TOUCH_YU (PA06) | 20 | 19 |
  | TOUCH_XR (PB08) | 21 | 20 |

  So `TOUCH_YD` was actually driving whatever's really on pin 17 (not the
  touch panel), which explains everything: driving "YD" LOW never
  established a real ground reference for the Y-axis gradient, so any
  touch just pulled the sense pins toward the healthy HIGH side — exactly
  the both-pins-jump-to-1023 signature the `y` test caught. "X" looked
  fine because pins.h's `TOUCH_YU` (19) is really the panel's XL
  electrode — still a genuine touch electrode, so it happened to produce
  plausible-looking numbers by coincidence.
  **Fix applied 2026-07-17:** both `firmware/pynt/bringup/pins.h` and
  `firmware/pynt/rover/pins.h` corrected to 18/19/20/21. Both environments
  rebuild clean. **Status: fix applied, not yet re-verified on hardware —
  re-flash `pynt-bringup` and re-run `p`/`y` to confirm real 4-corner
  values before closing this out.**
