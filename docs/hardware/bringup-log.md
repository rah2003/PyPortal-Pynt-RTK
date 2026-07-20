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
- [x] UBX-MON-VER recorded 2026-07-19 — **HPG 1.51, protver 27.50** —
      without u-center: the rover firmware now queries MON-VER over UART1
      at boot and prints it (`[gnss] ZED-F9P fw HPG 1.51 protver 27.50`).
      u-center can't connect while the Pynt drives UART1 (its USB adapter
      bridges the same port — README constraint 5), so this is the
      go-forward way to read the version.
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
3. [x] **Data union** (2026-07-19): `n` test shows live NMEA (`$GBGSV`,
       `$GNGLL`, `$GNGST`, etc.) in the Pynt console at 115200,
       interleaved with unreadable binary — **that binary is
       UBX-RXM-RAWX/SFRBX rendering as raw bytes in a text terminal,
       exactly as expected** since both are configured on for logging
       (section 1). Confirms both the NMEA and UBX binary paths are live,
       not a data-corruption signal.
4. [x] **1-hour full-stack soak**: NTRIP + logging + backlight running
       together, no resets, SD file confirmed growing — **PASS 2026-07-19**
       (66 min captured, details below)

**Section 3 status: all gates CLOSED (gate 4 soak PASS 2026-07-19).**

### Gate 4 runbook — 1-hour full-stack soak (`pynt-rover`)

**Software pre-flight (done 2026-07-19, no board attached):** bench touch
calibration + rotation 2 applied to `ui.cpp` from the section-2 'p'
values; both envs rebuild clean (rover RAM 9.7 % / flash 11.1 %). The two
remaining README bench items (`server.available()` semantics, bounded
`WiFi.begin()` stall) are runtime observations with mitigations already
in code — watch for them below, nothing to change beforehand.

**Setup (in order):**
- [x] u-center USB adapter physically unplugged from the Lite (wiring.md
      data-contention rule); wires 1-4 connected per the tape labels,
      antenna sky view, iPhone hotspot up
- [x] Credentials: `secrets.h` created from the example (wifi1/pass1 +
      caster user/password); macro names verified against settings.cpp
- [x] Flash + monitor: flashed over COM5; session captured by a pyserial
      logger with elapsed-time stamps + `status` injected every 10 min

**Watch during the hour (`status` every ~10 min):**
- [x] `ntrip=` reached connected at 00:20 and never dropped (zero
      `[ntrip]` error lines across the 66-min run)
- [x] `bytes=` grew monotonically 0.26 → 11.37 MB; `sd=ok` throughout,
      zero SD errors
- [x] `[main] worst loop` bounded: steady-state 32-50 ms; isolated ~1.1 s
      spikes (17:51, 23:51, 24:50 — SD/NINA ops, absorbed by design);
      first-minute 10.9 s = the blocking `WiFi.begin()` window. ⚠ That
      join measured **10.9 s, not the ~4 s the 4096 B SERCOM ring was
      sized for** — harmless at boot (logging hadn't started), but a
      mid-session WiFi rejoin would gap the RAWX stream. Carried forward.
- [x] TCP client streamed NMEA (laptop test client). **Two findings**,
      both recorded in QUESTIONS.md Q19: nina-fw accept is data-gated
      (a listen-only client sits unaccepted until it sends ≥1 byte —
      confirmed live), and the dedup in `tcp_nmea.cpp` compared clients
      via operator-bool (fork has no operator==), so only the first
      client could ever be accepted — fixed and flashed post-soak.
- [x] Touch: LOG / PAGE / PWR buttons all functioned correctly
      (2026-07-19, owner-verified) — **bench 4-corner calibration +
      inverted-axis mapping confirmed good on hardware**
- [x] No resets: **zero boot banners** in the log, per-minute reports
      continuous, `bytes=` never restarted

**Record at the end:**
- [x] Duration: `66` min captured; resets: `0`
- [x] Final `.ubx` size: `11,365,704` B (~10.3 MB/h — the runbook's
      1.5-2.5 MB/h sanity guess was far low for 32-SV multi-band RAWX;
      still only ~0.25 GB/day against a 32 GB card, no concern)
- [x] RTK fix achieved: `yes` — carr=2 by the 02:01 status and held to
      66:04 (TTFF < 2 min from cold boot including hotspot join)
- [x] Worst loop over the hour: `10.9 s` (boot WiFi join); `1.1 s`
      steady-state peak. `BUFH` not captured — `status` doesn't print
      it (SYS TFT page only); read it on-screen next bench sit
- [x] Post-soak: `.ubx` parse **verified 2026-07-19** — the original
      soak file was lost to the SD corruption (unsafe eject), so a fresh
      4-min capture (`r_054749.ubx`, 823 KB, closed cleanly via
      `log off`) was played back in u-center: RAWX 1688-1720 B at 1 Hz
      (~30 SV multi-band) + SFRBX bursts, all well-formed. The clean
      close → clean read also supports the unsafe-eject theory for the
      earlier corruption.

**Base-mode spot check (Phase 3 close-out, after the rover soak passes):**
- [x] **PASS 2026-07-19** — `mode=base` via serial menu applied live
      (`[gnss] applying mode: base` within 1 s); survey-in ran a
      textbook convergence, 9.16 m → 1.46 m mean accuracy, **valid at
      exactly dur=300 s** (the svindur/svinacc defaults); `mode=rover`
      switched back live and re-applied cleanly. UART2/XBee socket
      stayed empty throughout (config-only rule).
      One gap found: the **`b_` log prefix did not appear** — the
      prefix is chosen only at file open and nothing rotated the log on
      a live mode switch, so the boot-time `r_` file kept collecting.
      Fixed same day (serial_menu.cpp cycles the logger on a `mode=`
      change) and flashed; rotation itself not yet re-verified on
      hardware — one-minute check next bench sit: `mode=base`, confirm
      `[sd] logging to /YYYYMMDD/b_*.ubx`, `mode=rover`.

Notes / anomalies:

- Wiring was done as a single four-wire pass rather than the
  power-only-then-data sequencing the gate list originally implied.
  Retroactively confirmed both P3/P4 (power) and the data path are
  healthy, so no re-work needed — just noting the actual order for
  anyone reading this log later.
- **First soak attempt aborted at ~2 min (2026-07-19):** NTRIP TCP
  connect failed repeatedly — root cause was not the firmware but DNS:
  the inherited caster hostname `acorn-gnss.net` no longer has an A
  record at the apex (provider change since the Metro era); only
  `www.acorn-gnss.net` resolves (verified listening on :2101 from the
  bench PC before switching). Default fixed in `settings.h`, README +
  QUESTIONS.md Q6 note updated, reflashed, soak restarted clean.
- **Host-side USB stall, minutes ~30 → 55:14:** no serial delivered to
  the logging PC for ~25 min, then everything arrived in one burst (all
  lines stamped 55:14) and the three queued `status` commands were
  answered on arrival. The device itself never blinked: per-minute
  worst-loop reports all present with normal values, SD bytes grew
  continuously, NTRIP never reconnected, fix unchanged. Attributed to
  Windows USB suspend/driver buffering, not firmware. Post-recovery
  statuses (62:01, 66:04) arrived on schedule.
- **Soak binary note:** the hour ran the pre-dedup-fix `tcp_nmea.cpp`
  (the operator== bug was found mid-soak); the fixed binary was flashed
  immediately after the soak ended, before the base-mode spot check.



---

## Open items carried forward (2026-07-19, post-soak)

- ~~**iOS phone app** (QUESTIONS.md Q19)~~ **CLOSED 2026-07-19: QField
  connected and streamed on the first try** — it sends on connect, so
  the data-gated accept is satisfied. QField is the v1 phone app.
- ~~**`WiFi.begin()` blocks ~10.9 s**~~ **CLOSED 2026-07-19:
  SERIAL_BUFFER_SIZE raised 4096 → 32768 (~13 s of F9P output) — a
  mid-session rejoin no longer gaps the RAWX stream.
- ~~**Touch buttons unverified**~~ **CLOSED 2026-07-19: all three
  buttons function correctly** (owner-verified). **BUFH read
  2026-07-19: typically 9,000-14,000 B** against the 32,768 B SparkFun
  file buffer — ~43 % peak occupancy, comfortably inside the
  platform.md contention budget. Closed.
- **RINEX conversion verified (convbin, demo5 v2.5.1)**: the 823 KB
  capture converted to RINEX 3.04 with **304 obs epochs in a 304 s
  span — zero dropped epochs at 1 Hz** — dual-frequency on all four
  constellations (G L1C/L2X, R L1/L2, E E1/E5b, C B1I/B2I) + 38 nav
  messages, with NTRIP + TCP server running concurrently during the
  capture. The logging pipeline is post-processing-grade.
- **SD card read back corrupted on the PC (2026-07-19)** after the
  first full bench day; reformatted (keep FAT32) and lost the soak
  `.ubx`. Most likely cause: card pulled while the device was powered
  with a log file open — logging auto-starts whenever GNSS time is
  valid, and the logger syncs on an 8 s cadence, so a pull can land
  mid-write and damage the FAT. **Safe-eject procedure: PWR two-tap →
  wait for "SAFE TO POWER OFF" → cut power → then pull the card.**
  (LOG-button off also closes the file, but shutdown is the guarantee.)
  Watch for recurrence with the procedure followed — if it corrupts
  again on a clean shutdown, suspect the card itself (PNY U1) and
  retest with a fresh card before blaming the SPI path. **Follow-up
  same day: post-reformat 4-min capture closed with `log off` read back
  perfectly in u-center — consistent with unsafe eject, not a bad card.**
- ~~**`b_` prefix rotation fix flashed but not re-verified**~~ **CLOSED
  2026-07-19**: re-check session confirmed `mode=base` closes the `r_`
  file and opens `b_045917.ubx`, and `mode=rover` rotates back. Same
  session verified the MON-VER boot report (HPG 1.51 / 27.50) and the
  new `ip=` status + SYS-page IP:port display, and exercised the NTRIP
  retry path live (5 rejects at boot → connected → stable).
- **Post-soak `.ubx` sanity**: eject the card, open
  `/20260720/r_031723.ubx` (10.8 MB soak log) on the laptop — RTKLIB or
  u-center should parse RAWX/SFRBX cleanly.

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
