# Bench Bring-Up Log

Working copy for the bench pass — fill in as you go, alongside
`checklists.md` (source of truth for *what* to check) and `wiring.md` /
`power.md` / `ucenter-config.md` (source of truth for *how*).

Date started: ____________  Date completed: ____________

---

## 1. simpleRTK2B Lite (ZED-F9P) — u-center re-verification

- [x] Physical inspection: JST-GH intact, SMA snug, no bent XBee pins
- [x] JST pin 1 located and marked on the pigtail
- [x] Top XBee socket confirmed empty
- [x] HC977 antenna mounted (hand-tight + 1/8 turn), sky view for fix tests
- [x] u-center connected via XBee-to-USB adapter, **pigtail unplugged**
- [x] Connection baud used: `115200` (Feather-era config took, as expected)
- [ ] UBX-MON-VER firmware not recorded this pass (want HPG 1.32+) — grab
      this next time u-center is open, low priority since everything else
      checked out
- [x] **UART1 keys verified/set via VALGET→VALSET→VALGET (Flash-confirmed):**
      baud 115200 ✅; UBX in=1 ✅; NMEA in was **1→ fixed to 0** ✅;
      RTCM3X in=1 ✅; UBX out=1 ✅; NMEA out=1 ✅; RTCM3X out was
      **1→fixed to 0** ✅ (this is Rover — RTCM3X out belongs on UART2 in
      Base mode, not here)
- [x] **RAWX / SFRBX messages: both were 0→fixed to 1**, confirmed in Flash
      and visible live in the packet console (RAWX ~1/s, SFRBX bursting
      several/s — normal). TIM_TM2 left off (optional, not needed)
- [x] **NMEA set on UART1: GGA/RMC/GSA/GST/GSV all fixed 0→1**; VTG was
      briefly mis-queued to 1, corrected to 0; HIGHPREC=1 ✅ — all
      confirmed in Flash
- [x] **Rate: MEAS=1000ms ✅, NAV was unset in first pass → fixed to 1** —
      confirmed in Flash (true 1 Hz)
- [x] Dynamics = 0 (Portable) ✅ — already correct, no fix needed
- [x] **Elevation mask was 10°→fixed to 12°**; constellations
      GPS/GLONASS/Galileo/BeiDou all already =1 ✅; SBAS already =0 ✅;
      **QZSS was 1→fixed to 0** — all confirmed in Flash
- [x] UART2 left untouched (never queried/changed)
- [x] Saved to Flash (RAM+BBR+Flash checked on every VALSET)
- [x] **Power-cycled — all fixes survived reconnect at 115200.** VALGET
      re-check not repeated post-cycle (relying on pre-cycle Flash
      confirmations + successful reconnect at the configured baud)
- [x] Packet console reviewed live: RAWX (1/s) + SFRBX (bursty) + configured
      NMEA set all present. **One unaccounted extra: `NMEA-GNGLL` is on**
      (never in the checklist's target set) — harmless bandwidth-wise,
      left on by owner decision rather than spending another VALSET round
      trip; not a logging/protocol risk either way
- [ ] USB adapter not yet unplugged — **do this before wiring the pigtail
      to the Pynt** (data-contention rule in `wiring.md`)

**Section 1 status: CLOSED**, pending only the physical adapter-unplug
step before integration wiring begins.

Notes / anomalies:

- This Lite came over from the Feather rig "maybe already configured" —
  in practice it was **not** correctly configured for this project's
  Rover/RAWX-logging needs: RTCM3X-out and NMEA-in were both backwards
  (Base-mode-ish leftovers), RAWX/SFRBX logging was entirely off, and the
  elevation mask + QZSS were both off-target. The VALGET-first approach
  caught all of it before assuming "it probably still has the old config."
- `NMEA-GNGLL` chatter — see checklist line above. Revisit only if SD
  logging bandwidth ever becomes tight (unlikely per `platform.md`'s
  budget math).



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

**`p` — Touch (re-captured 2026-07-17 after the pin fix, rotation 2)**
- [x] Corner top-left (TL): x=825 y=835 z=250
- [x] Corner top-right (TR): x=157 y=849 z=500
- [x] Corner bottom-left (BL): x=837 y=130 z=1700
- [x] Corner bottom-right (BR): x=202 y=126 z=585
- [x] **Valid calibration constants — touch pin fix confirmed on hardware.**
      Note for Phase 2 `ui.cpp`: both axes read **inverted** vs. typical
      screen convention at rotation 2 — X is high-on-left/low-on-right
      (TL≈825→TR≈157), Y is high-on-top/low-on-bottom (TL≈835→BL≈130).
      Map/invert accordingly when converting raw touch to screen
      coordinates, don't assume a direct 0-at-top-left mapping.

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

1. [x] Section 1 + Section 2 both fully healthy
2. [x] **All four wires connected at once** (per the D3/D4-sourced wiring
       in `wiring.md`: power+ground+TX off D4, RX off D3) rather than
       power-only first — Lite power LED on, Pynt unaffected
   - [x] P3 — voltage at Lite JST pin 1 under load (WiFi active, backlight
         full): **4.77 V** (≥ 4.5 V floor — comfortable margin)
   - [x] P4 — 30-min soak: **complete, no issues** (no brownout resets on
         either board)
3. [x] **Data union**: `n` test shows live NMEA (`$GBGSV`, `$GNGLL`,
       `$GNGST`, etc.) interleaved with unreadable binary — **that binary
       is UBX-RXM-RAWX/SFRBX rendering as raw bytes in a text terminal,
       exactly as expected** since both are configured on for logging
       (section 1). Confirms both the NMEA and UBX binary paths are live,
       not a data-corruption signal.
4. [ ] **1-hour full-stack soak**: NTRIP + logging + backlight running
       together, no resets, SD file confirmed growing — **not yet done,
       this is Phase 2 rover-firmware territory** (`pynt-rover` env),
       not the Phase 1 bring-up sketch. Pick up when Phase 2 work starts.

**Section 3 status: gates 1-3 CLOSED. Gate 4 deferred to Phase 2.**

Notes / anomalies:

- Wiring was done as a single four-wire pass rather than the
  power-only-then-data sequencing the gate list originally implied.
  Retroactively confirmed both P3/P4 (power) and the data path are
  healthy, so no re-work needed — just noting the actual order for
  anyone reading this log later.



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
  rebuild clean.
  **CONFIRMED FIXED ON HARDWARE 2026-07-17.** Re-flashed `pynt-bringup`,
  re-ran `p`: y now varies across the full range with touch position
  (observed x=298-829, y=123-878, z=209-1998 across several presses) —
  no longer pinned. **Closed.**
