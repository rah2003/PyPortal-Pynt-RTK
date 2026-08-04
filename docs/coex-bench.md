# WiFi + BLE Coexistence Bench

Working doc for branch `coex/nina-fw-airlift`. Goal: prove simultaneous
WiFi + BLE on AirLift hardware with custom module firmware, de-risked on
spare boards before the assembled Pynt. Companion to
`firmware/nina-fw-airlift/AIRLIFT.md` (module firmware) and the
`VENDORED.md` notes in `lib/`.

> **STATUS 2026-07-27.** Spikes A and B **PASSED** on the Metro
> (sections 4–5, bench notes below). Spike C (Feather Wing soak) is
> **SUPERSEDED** by owner decision — see section 6 — and its
> SD-contention question folds into the Pynt integration soak. The
> Pynt may now be flashed, **only** via the section 7 sequence:
> backup first, rollback per section 8. The shipping `pynt-rover` env
> was never modified and remains the instant fallback.

Order of operations:

1. ~~Build the module firmware (section 2)~~ — **done 2026-07-26**.
2. ~~Metro M4 AirLift Lite: backup, flash, Spike A, Spike B~~ —
   **done 2026-07-26, all gates green**.
3. Pynt integration (section 7): backup → flash → `pynt-rover-coex`
   re-verification → BLE NUS module → combined integration soak.
4. Rollback at any point: section 8.

## 1. Pinned version set

| Piece | Version | Where |
|---|---|---|
| Module firmware | arduino/nina-fw **3.0.1** (`57d12f4`) + MOSI 12→14 | vendored `firmware/nina-fw-airlift/` |
| ESP-IDF (builds it) | **v4.4.8**, installed in WSL2 Ubuntu-22.04 at `/opt/esp-idf-v4.4.8` (Docker `espressif/idf:v4.4.8` equivalent) | this machine / AIRLIFT.md |
| WiFiNINA (host) | **2.1.1** (`272159c`) | `platformio.ini` git-tag pin |
| ArduinoBLE (host) | **2.1.0** (`281377b`) + 2-line gate patch | vendored `lib/ArduinoBLE/` |
| Arduino_SpiNINA (host) | **0.0.2** (`200cc35`), unpatched | vendored `lib/Arduino_SpiNINA/` |
| Adafruit SAMD core | **1.7.16** (PIO `framework-arduino-samd-adafruit 1.10716.0`) | pinned by platform `atmelsam` |
| esptool | **5.3.1** (dash-form commands: `read-flash`, `write-flash`); on the bench machine it lives in a scratch venv, driven via `flash_via_bridge.py` | laptop |

Rollback binaries: per-board `esptool read-flash` dumps (primary) and
stock `NINA_W102-1.7.x.bin` from
<https://github.com/adafruit/nina-fw/releases> (secondary).

## 2. Build the module firmware (no hardware needed)

**Done 2026-07-26** on this machine via WSL2 Ubuntu-22.04 + ESP-IDF
v4.4.8 (setup + exact commands: `firmware/nina-fw-airlift/AIRLIFT.md`).
Output already sits at
`firmware/nina-fw-airlift/NINA_W102-3.0.1-airlift.bin` — exactly
2,097,152 bytes, `0xE9` image magic at 0x1000 and 0x30000, "3.0.1"
version string embedded, committed on the branch. One image for all
boards, flashed at offset 0x0.

To rebuild from scratch: WSL path in AIRLIFT.md (verified), or Docker
`espressif/idf:v4.4.8` (equivalent, untested here). Either way the
flashable file is combine.py's **`_ALL.bin`** output.

## 3. Flashing procedure (any board)

The ESP32's serial bootloader is mask ROM — an interrupted or bad flash
is **always** recoverable by redoing this section. Reflash-risk, not
brick-risk.

1. `pio run -e passthrough-<board> -t upload` — the sketch straps the
   ESP32 into its ROM loader (GPIO0 low through a reset pulse) and
   bridges USB↔NINA UART at a fixed 115200.
2. Note the board's COM port (Device Manager, or `pio device list`).
3. **Backup first** (~3 min at 115200), then write. On Windows the stock
   esptool CLI does NOT work through the SAMD bridge — esptool forces
   DTR/RTS low and the CDC core then drops device→host bytes, and
   compressed writes die mid-stream (bench notes, 2026-07-26). Use the
   wrapper, from `firmware/nina-fw-airlift/`:

   ```bash
   python flash_via_bridge.py COMx read-flash 0 0x200000 <board>-stock-backup.bin
   ```

   ```bash
   python flash_via_bridge.py COMx write-flash --no-compress 0 NINA_W102-3.0.1-airlift.bin
   ```

   (On a non-Windows host the plain CLI form with
   `--before no_reset --after no_reset --baud 115200` may work as-is;
   the wrapper is always safe.)

4. After flashing, upload the next firmware env — its own boot resets
   the module into the new NINA firmware.

Per-board passthrough envs: `passthrough-metro`, `passthrough-feather`
(Wing pins via build flags), `passthrough-pynt` (used by the section 7
sequence).

## 4. Spike A — upstream WiFi stack alone (Metro) — PASSED

Prereqs: custom firmware flashed (section 3);
`firmware/spike/secrets.h` created from `secrets.example.h`; hotspot up.

```
pio run -e metro-spike-wifi -t upload && pio device monitor -b 115200
```

Pass gates — all four, else stop and debug before any BLE work:

- [x] No `AirLift not responding` (SPI link alive through the new firmware
      → the MOSI 12→14 patch is right)
- [x] `NINA firmware: 3.0.1` (stock would say 1.7.x)
- [x] Scan lists the hotspot SSID
- [x] Join succeeds; IP + RSSI keep printing at 5 s cadence, no drops
      while parked next to the phone

**All four passed 2026-07-26** — see bench notes below.

## 5. Spike B — WiFi + BLE simultaneously (Metro) — PASSED

```
pio run -e metro-spike-coex -t upload && pio device monitor -b 115200
```

Then, at the same time:

- Laptop on the hotspot: `nc <metro-ip> 10110` (or PuTTY raw) — expect
  the 1 Hz `COEX up=...` line; typed bytes echo back.
- Phone: nRF Connect / LightBlue → connect to **PyntRTK-coex** →
  subscribe to characteristic `6E400003-...` (NUS TX) — expect the same
  1 Hz line as notifications.

Pass gates:

- [x] TCP stream and BLE notifications run **concurrently** ≥ 15 min
- [x] `up=` counter never restarts (no watchdog/brownout resets)
- [x] `drops=0`, or any WiFi drop re-joins by itself
- [x] BLE disconnect/reconnect mid-run doesn't disturb the TCP side
- [x] Serial line stays clean (no SPI timeout spew)

**All five passed 2026-07-26** — see bench notes below.

## 6. Spike C — SUPERSEDED (folded into the section 7 soak)

**Owner decision 2026-07-27: not run standalone.** No SD breakout on
hand and no soldering the Wing's GPIO0 jumper; the shared-SPI
contention check moves into the Pynt integration soak (section 7.5).

Why this is acceptable risk rather than a skipped gate:

- **SD + AirLift WiFi contention on the Pynt is already proven** on the
  stock stack: the 1-hour Phase 2/3 rover soak passed with RAWX logging
  + NTRIP + TCP live (convbin RINEX verification, zero gaps), and the
  SPI-contention timing is documented in `docs/hardware/platform.md`
  and the bringup log (93.6 ms worst write, WiFi-only).
- **What coex adds is only BLE HCI multiplexed onto the same SPI
  command channel** — and exactly that multiplexing ran clean for
  34 minutes on real AirLift hardware in Spike B (zero SPI errors,
  zero resets). The Feather rig would have re-proven the combination
  on *different* silicon than the target; the Pynt soak proves it on
  the actual unit.
- **The Pynt is reflash-recoverable, not brickable** (mask-ROM
  bootloader, section 3), and the section 7 sequence takes a full
  stock-firmware backup before writing anything, with the section 8
  rollback returning to the known-good `pynt-rover` state in minutes.

The original standalone procedure is preserved in **Appendix A.1** for
anyone who later has the Wing on the bench and a soldering iron for its
GPIO0 jumper. The `feather-soak-coex` and `passthrough-feather` envs
remain in `platformio.ini` and compiling.

**Update 2026-07-27 — optional no-soldering path:** Spike C is
runnable after all, with a hand-wired **HUZZAH32** standing in for the
Wing (same ESP32 silicon family, same `NINA_W102-3.0.1-airlift.bin`,
flashed trivially over the HUZZAH32's own USB). Procedure, wiring
table, and a **do-NOT-stack** warning: **Appendix A.2**, env
`feather-soak-coex-huzzah`.

**Update 2 (same day): RUN AND PASSED** — 72-minute soak on the A.2
rig, all gates green (bench note below). Spike C is no longer a gap;
the section 7.5 integration soak now re-proves the same combination on
the target unit rather than proving it first.

## 7. Pynt integration (unlocked 2026-07-27)

Run the steps in order; stop at any failure and consult section 8.

### 7.1 Back up the Pynt's stock NINA firmware

```
pio run -e passthrough-pynt -t upload
```

then, from `firmware/nina-fw-airlift/` (note the Pynt's COM port; the
wrapper is required on Windows — see section 3):

```bash
python flash_via_bridge.py COMx read-flash 0 0x200000 pynt-stock-backup.bin
```

Sanity-check the dump before proceeding: 2,097,152 bytes, `0xE9` at
offset 0x1000. **Keep this file safe** — it is the primary rollback
(gitignored; park a copy off-machine).

### 7.2 Flash the custom firmware

```bash
python flash_via_bridge.py COMx write-flash --no-compress 0 NINA_W102-3.0.1-airlift.bin
```

Wait for `Hash of data verified` (~3 min uncompressed).

### 7.3 Switch to the coex env and re-verify WiFi-only behavior

```
pio run -e pynt-rover-coex -t upload
```

`pynt-rover-coex` builds the same rover source against the upstream
stack — WiFiNINA 2.1.1 + patched ArduinoBLE 2.1.0 + vendored
Arduino_SpiNINA, selected by `-DCOEX_UPSTREAM_NINA` (compiles out the
Adafruit-fork `WiFi.setPins()`; pins come from the pyportal_m4 variant
macros) and `-DBLE_NINA_SPI_TRANSPORT` (enables ArduinoBLE's SPI-HCI
transport). The shipping `pynt-rover` env is untouched.

Short attended bench (not the soak yet) — re-verify the behaviors that
were characterized against the Adafruit 1.x fork:

- [x] Boot clean: F9P detected, SD up, WiFi joins, NTRIP connects,
      RTK fix arrives, display sane
- [ ] `WiFi.begin()` stall window: RAWX timeline across a forced
      rejoin (hotspot off/on) shows only the bounded gap the 4096-byte
      SERCOM ring was sized for
- [x] **Silent-TCP-client accept — RESOLVED from source 2026-07-27,
      verify on the Pynt:** `server.available()` is data-gated on every
      nina-fw (the gate lives in the host's `availServer(accept=false)`
      call); upstream's new `server.accept()` API takes nina-fw 3.x's
      true-accept path. `tcp_nmea.cpp` now uses `accept()` under
      `COEX_UPSTREAM_NINA` (verified live on the soak rig). Bench
      check: a listen-only client (u-center / `nc` without typing)
      connects to :10110 and streams immediately. Details in Q19.
- [x] Serial `status` healthy: worst-loop time and free RAM in line
      with the stock-stack numbers

### 7.4 Add the BLE NUS rover module — WRITTEN 2026-07-27, needs bench

Implemented as `firmware/pynt/rover/ble_nus.{h,cpp}` — the seventh
polled superloop module, `FEATURE_BLE`-gated (default 0; only
`pynt-rover-coex` sets `-DFEATURE_BLE=1`). Spike B's proven service,
verbatim where it matters: same NUS UUIDs, one-sentence-per-notification
framing (120-byte characteristic, sentences never split), advertising as
**PyntRTK-rover**. NMEA reaches it through a tee in `tcp_nmea.cpp`'s
existing drain — BLE runs ALONGSIDE the TCP server (QField on TCP, iOS
SW Maps on BLE), including when WiFi is down (the future radio
scenario). Discipline: the tee only queues into an 8-line drop-oldest
ring; all SPI work happens in `bleNusPoll()` after gnss/ntrip/tcp, max
3 notifications per pass, zero bus traffic with no subscriber;
`BLE.begin()` failure degrades to WiFi-only with a log line. Status
surfaces as the POS page's ninth line (`BLE off/advert/conn/stream`)
and in the serial `status` report (state + linesTx + drops).

Verified compiling: `pynt-rover` byte-identical footprint
(140184 B RAM / 120044 B flash, unchanged); `pynt-rover-coex`
142868 B RAM (54.5 %) / 149772 B flash (+~2.7 KB / +~29 KB for
ArduinoBLE + module).

Bench items for this module (fold into 7.3/7.5):

- [ ] `BLE.begin()` succeeds after `ntripInit()` on the Pynt (Spike B
      proved the ordering on the Metro; confirm on this unit)
- [ ] iOS SW Maps discovers **PyntRTK-rover**, connects as a BLE GNSS
      instrument, and gets a position — the actual Q19 payoff
- [ ] Full NMEA set at 1 Hz (~8–12 sentences/epoch) drains within the
      3-per-pass cap with `bleDrops` staying ~0 while subscribed (the
      loop runs kHz-fast, so an epoch's burst should clear in
      milliseconds — verify, don't assume)
- [ ] Worst-loop time with a subscriber attached stays in line with the
      stock numbers (each notify is one bounded SPI command)

### 7.5 Integration soak — the combined gate (replaces Spike C)

Everything at once, ≥ 1 hour: **SD RAWX logging** (rover cadence:
bounded 512 B writes per loop pass, 8 s sync) + **NTRIP corrections
over WiFi** (live caster, RTK fix held) + **TCP NMEA** (QField or a
logging `nc` client) + **BLE NUS to a phone** (SW Maps on iOS if it
accepts the stream, else nRF Connect logging NUS TX).

Pass metrics — all of them, measured, recorded in the bench notes:

- [x] **≥ 60 min**, uptime monotonic — zero watchdog/brownout resets
- [x] **SD integrity:** PASS 2026-08-03 — 8.36 h battery-powered run
      (`r_060336.ubx`, 86.76 MB): 721,448 frames, **0 bad checksums,
      0 resyncs, 30,099 RAWX epochs = exactly 1 Hz × duration (zero
      gaps)**, size consistent with the ~2.9 kB/s RAWX+SFRBX rate
- [x] **NTRIP:** zero correction-stale teardowns while the hotspot is
      up; any genuine hotspot handoff recovers unaided; correction age
      < 10 s steady-state; RTK fix (or float, per sky view) held
- [x] **TCP:** client stays connected the full soak, zero dropped
      connections; silent-accept behavior as verified in 7.3
- [x] **BLE:** stays connected/subscribed throughout (operator-initiated
      bounces allowed, must reconnect and must not perturb TCP/NTRIP —
      the Spike B gate, now under load); notify latency bounded — 1 Hz
      NMEA lines arrive within ~2 s, no multi-second stalls
- [ ] **Memory/loop stability:** free RAM flat across the hour (no
      monotonic decline = no leak) and worst-loop time bounded, both
      via the serial `status` report at start / mid / end
- [ ] **Worst SD write latency recorded:** `______ ms` (WiFi-only
      baseline was 93.6 ms; expect the same order with BLE HCI added
      to the bus)

All green ⇒ the coexistence stack is proven on the target unit; Phase 5
(SW Maps polish, UART2 radio scenario) proceeds on this branch. Any
red ⇒ section 8, analyze from the logs, re-approach.

## 8. Rollback (any time, in minutes)

The two halves are independent — module firmware and host env — and
both revert cleanly; the shipping env was never modified on this
branch.

1. **Module firmware back to stock:** `pio run -e passthrough-pynt -t
   upload`, then from `firmware/nina-fw-airlift/`:

   ```bash
   python flash_via_bridge.py COMx write-flash --no-compress 0 pynt-stock-backup.bin
   ```

   (No backup at hand? Secondary: the stock `NINA_W102-1.7.x.bin` from
   <https://github.com/adafruit/nina-fw/releases>, same command.)

2. **Host back to the shipping stack:**

   ```
   pio run -e pynt-rover -t upload
   ```

   `pynt-rover` still builds against the pinned Adafruit WiFiNINA fork
   exactly as it passed the Phase 2/3 soak — byte-identical env, never
   touched by the coex work.

That pair restores the unit to its last known-good field state.

## Bench notes / results

(append dated notes here as sections close, bringup-log style)

### 2026-08-03 — SD forensics: battery run 8.36 h GAP-FREE; freeze times confirmed

UBX frame-walk over the card (`tools/ubx_walk.py` — walks
sync/len/checksum, extracts RAWX week+tow):

- **`r_060336.ubx` (the zero-observation battery run): 86.76 MB, 100 %
  valid — 721,448 frames, 0 bad checksums, 0 resyncs, 30,099 RAWX
  epochs over 8.36 h = exactly 1 Hz, ZERO epoch gaps** (including
  through the log-start BUFH spike). Ran 06:03:37Z → 14:25:16Z
  (22:03 → 06:25 local); the end is battery depletion, not firmware.
  §7.5 SD-integrity gate: PASS.
- The four 32,768 KB files from the freeze session are NOT a size cap:
  `kPreAllocBytes` = 32 MiB and `truncate()` only runs in `closeFile()`,
  so any crash/power-yank leaves the directory entry at the full
  pre-allocation with stale-cluster tail after the real data. Each
  freeze file holds valid frames up to the freeze (one truncated frame
  at the cut, then tail): r_011333 → last epoch 01:31:31Z (lockup 1,
  serial died 01:31:29Z), r_014443 → 02:05:34Z (lockup 2), r_025155 →
  02:57:37Z (lockup 3), r_031248 → 03:46:19Z (the 37-min freeze).
  The .ubx record corroborates the freeze timeline to the second.
- Operational note: a `.ubx` whose size is exactly 33,554,432 B is an
  unclosed session — parsers must stop at the last checksum-valid
  frame (convbin does; naive size-based tools will read garbage tail).

### 2026-08-02 — §7.5 re-run w/ open sky: RTK FIXED reached; 8 freezes; INTERIM

Goal was closing the 7-28 float-only item with a fix held under the full
concurrent load. Two headline results, one good, one bad:

**RTK FIXED confirmed on the target.** `carr=2` with 31–32 SV within
minutes of every boot (open-sky antenna), GGA quality 4 on the TCP
stream, held until each freeze. The 7-28 "re-attempt fixed with open
sky" item is closed.

**Eight hard freezes, 7–37 min uptime each** (build Jul 31 18:25,
`pynt-rover-coex` + `ENABLE_WEB_CONFIG`). Signature every time:
superloop dead (TFT frozen on last frame, serial menu dead, TCP stream
stops mid-flow with NINA sockets left half-open, BLE drops), USB still
enumerated — consistent with an unbounded SpiDrv-style wait, not a
hard fault. No warning in any counter beforehand (worst-loop 93–180 ms,
RAM n/a, bleDrops growing slowly ~0.6%-class as in the 7-28 soak).

Elimination ladder (each variable exonerated by a freeze without it):
serial capture attached → froze without it; SD logging on → froze with
it off; BLE streaming → froze mid-connect AND with BLE idle; PC-USB
power → froze on a wall brick; web GUI serving → froze with
`webenable=0`. The one condition present at ALL eight freezes and
absent from the known-good baseline (a 24 h clean run, same build,
other host, one TCP consumer, no automated probes): **this bench's
observation harness — a 2nd concurrent TCP client on :10110 plus a
5-min `GET /api/status` probe** (note the SPA itself polls /api/status
at 1 Hz while the page is open — a phone with the page up counts as
web load). Zero-observation rerun (24h-config reproduction, battery
power, SW Maps + QField + SD logging, started 20:35): **clean until
the battery depleted — no freeze**; confirmed alive at 22:30 (≥2 h,
&gt;3× the worst time-to-freeze; exact end time recoverable from the
.ubx last epoch) vs 8/8 freezes ≤37 min with the harness attached. The firmware is stable in field configuration;
remaining pinpoint = one-variable reintroduction (2nd TCP client only,
no probes) to split multi-client tcp_nmea vs the probe path. Freeze
evidence parked in session scratchpad (`soak_*.lockup1-4.log` +
per-run sets).

**Action items out of this session (owner-endorsed):**

1. **Add the SAMD51 WDT** — arm at boot, kick once per superloop pass.
   Every freeze tonight needed a finger on RESET; a field unit must
   self-recover. Use the early-warning interrupt to stamp a breadcrumb
   (which module the pass died in) to flash/SD before the reset fires —
   that breadcrumb would have named tonight's culprit on freeze #1.
   Treat as a Phase 5 prerequisite.
2. **Log-start buffer spike:** BUFH hit 24,376 B of the 32,768 B GNSS
   file buffer at `log on` (day-dir create + file alloc stall on the
   32 GB card; boot free-space scan on this card is ~2:20 for the same
   reason). ~8 KiB of headroom left — an SD latency spike landing on a
   log-start drops RAWX frames silently. Pre-create the day directory
   at boot (or open the file eagerly / enlarge the buffer).
3. **Untested remote-reset path:** the 1200-baud touch runs from USB
   interrupt context and the milder freezes kept interrupts alive —
   try it on the next USB-attached freeze before finger-reset; if it
   works it's a bench recovery tool (wall-powered units still need
   the WDT).
4. Host bench gotchas re-confirmed on this PC: USB CDC stall recurred
   twice mid-session (both directions; close/reopen restores flow —
   the capture script now self-heals on 75 s RX silence), and NINA
   keeps frozen-MCU sockets alive with no FIN/RST — TCP clients must
   treat "connected but silent >60 s" as dead.

### 2026-07-28 — Section 7 run: Pynt flashed, §7.3 green, §7.5 soak PASS

**7.1/7.2.** Stock backup `pynt-stock-backup.bin` taken via
`passthrough-pynt` + `flash_via_bridge.py` (2,097,152 B, 0xE9 @0x1000;
embedded version string shows the Pynt shipped with Adafruit nina-fw
**1.6.1** — the bringup-log blank, finally filled). Custom
`NINA_W102-3.0.1-airlift.bin` written `--no-compress`, hash verified.

**7.3.** `pynt-rover-coex` boot: SD ok, F9P detected, WiFi joined,
TCP bound, **BLE advertising as PyntRTK-rover**, RAWX logging.
Silent-TCP-accept: **PASS on target** — pure listener received 106
NMEA sentences in 10 s without sending a byte (`server.accept()`
path). Two operational notes: the Pynt's SD had **no /config.txt**
(credentials re-entered via serial menu, persisted), and first boot
stalls ~20 s in the 32 GB card's free-space scan before the loop
starts — benign, don't mistake it for a hang.

**7.5 integration soak — 75 min, RAWX + NTRIP + TCP + BLE NUS all
concurrent:**

- **NTRIP:** connected once, **zero reconnects/teardowns** the whole
  run; corrections applied continuously (GGA quality field: 994 s
  DGPS → 3,506 s **FLOAT**; RTK-fixed not reached — bench antenna sky
  view, gate wording "fix or float per sky view" satisfied; re-attempt
  fixed with open sky).
- **TCP:** 50,304 NMEA lines to a silent listener, **zero reconnects,
  max inter-line gap 1.0 s**.
- **BLE NUS:** three streaming sessions ≈ 58 min total with two
  bounces, every reconnect clean, **37,628 sentences notified**;
  `bleDrops=224` (~0.6 %, drop-oldest at epoch bursts coinciding with
  SD stalls — §7.4's "verify don't assume" item answered: not zero,
  functionally invisible to a live-position consumer; bump
  kQueueLines/kMaxNotifyPerPass if it ever matters).
- **SD:** same file all run, 5,045,080 B written (matches the ~1.1
  kB/s RAWX rate × 4,500 s); post-soak convbin parse = optional
  owner step.
- **Loop health:** 75/75 one-per-minute worst-loop reports (no
  resets); median worst-pass ≈ 132 ms, max 512 ms (SD latency spikes,
  bounded, no growth trend; 32 KiB SERCOM ring ≈ 13 s of margin).

**Simultaneous WiFi + BLE is proven on the assembled Pynt.** Q18's
original constraint is dead on this branch.

**Owner confirmation (same day): the soak's BLE client was iOS
SW Maps** connected to `PyntRTK-rover` as a Bluetooth GNSS instrument
— the Q19 payoff, delivered. Q19 closed with both phone paths live
(QField/TCP + SW Maps/BLE).

### 2026-07-26 — Metro flashed (section 3) + Spike A PASS (section 4)

Metro M4 AirLift Lite, app port **COM9** (bootloader enumerates
separately as COM7).

**Flashing.** The section 3 esptool commands failed as written on this
host, twice over, both host-side — the module and procedure are fine:

1. *CLI can't connect through the bridge.* esptool on Windows forces
   DTR+RTS low before opening the port (`loader.py` "avoid unwanted
   chip reset"); the Adafruit SAMD CDC core discards device→host bytes
   while neither line is asserted, so the ROM loader's sync replies
   never reach esptool ("No serial data received"). Proven by raw
   pyserial probe with DTR/RTS asserted: loader answers instantly.
   Fix: `flash_via_bridge.py` (parked next to the bin) — opens the port
   itself, then `esptool.main(esp=...)`.
2. *Compressed write dies deterministically.* `write-flash` (default
   deflate) aborted at exactly compressed offset 294912 (block 19) on
   two runs, even with 30 s timeouts — content-dependent transport
   failure somewhere in the host→device path. `--no-compress` wrote all
   2,097,152 bytes clean, ~188 s, `Hash of data verified`.

Backup `metro-stock-backup.bin` taken first (2,097,152 B, 0xE9 magic at
0x1000, ~192 s read). esptool 5.3.1 in a scratch venv (NOT on PATH on
this machine, contrary to the section 1 table).

**Spike A.** `metro-spike-wifi` + 240 s scripted capture
(`spikeA-metro-2026-07-26.log` next to the bin):

- `NINA firmware: 3.0.1` at t=1.0 s; no SPI errors at any point.
- Scan listed hotspot `Airport` (−82 dBm) — plus a scan-print cosmetic
  quirk: the neighboring printer's long SSID ran into the next entry on
  one line (`...9120eAirport`), worth re-checking string termination in
  the scan path if it ever matters; per-entry RSSI was sane.
- Join → IP 192.168.0.168 at t=9.6 s.
- 47/47 status lines at 5 s cadence over 240 s, `drops=0`, RSSI
  −73…−87 dBm, uptime counter monotonic (no resets).

Spike B (`metro-spike-coex`) is next; it needs the phone running nRF
Connect / LightBlue plus a laptop `nc` client on the hotspot, so it
wasn't run unattended.

### 2026-07-26 — Spike B PASS (section 5): WiFi + BLE simultaneous

34-minute attended soak, same Metro/COM9. Logs parked next to the bin
(`spikeB-metro-serial-2026-07-26.log`, `spikeB-metro-tcp-2026-07-26.log`).
Serial side: scripted capture from boot; TCP side: scripted client from
this PC (wired side of the Airport LAN), 1 Hz stream logged + a ping
sent every 60 s to prove two-way echo. BLE side: phone on nRF Connect,
subscribed to NUS TX on **PyntRTK-coex**.

- Boot: NINA 3.0.1, WiFi join at t=4.9 s (IP 192.168.0.168), TCP server
  bound, `BLE.begin()` OK and advertising at t=5.3 s — WiFi + BLE
  co-resident on the one SPI link, no mode switch.
- `up=` monotonic 5 s → 2075 s, zero non-monotonic steps (no watchdog /
  brownout resets). `drops=0` the whole run. RSSI −72…−84 dBm.
- TCP: connected at up≈32 s, **zero disconnects over 33 min**, 2002
  1 Hz lines received, **34/34 pings echoed**.
- BLE: connected up=177 s → 1239 s (17.7 min continuous, concurrent
  with TCP — gate met on that stretch alone), user-initiated bounce,
  reconnected up=1291 s → end. The bounce didn't perturb TCP at all
  (echo + stream continuous through the window).
- Serial: only the boot banner and expected events — zero SPI timeout
  spew across 2071 consecutive status lines.
- ~~**Bonus finding:** the TCP client was accepted **without sending a
  byte** — upstream nina-fw 3.0.1's `server.available()` is NOT
  data-gated.~~ **CORRECTED 2026-07-27:** misread — the Spike B test
  client pinged on connect, which is what satisfied the (still
  data-gated) `available()` path. The real mechanism, found from
  source during the A.2 soak bring-up: `server.available()` sends
  `accept=false` and is data-gated on ALL nina-fw versions; nina-fw
  3.x's true accept is reached only via upstream's new
  `server.accept()` API. See Q19 and the 7.3 checklist item.

### 2026-07-27 — Decision: Spike C superseded, Pynt unlocked

Owner call (no SD breakout on hand, no soldering the Wing's GPIO0
jumper): the standalone Feather soak is dropped and its SD-vs-radio
contention check folds into the section 7.5 integration soak on the
Pynt itself. Rationale recorded in section 6; original procedure
preserved in Appendix A. Sections 7 (step-by-step unlock) and 8
(rollback) rewritten accordingly.

### 2026-07-27 — Spike C PASS via the A.2 HUZZAH32 rig (72-min soak)

Rig: Feather M0 Adalogger (COM4) + hand-wired HUZZAH32 (COM3) running
`NINA_W102-3.0.1-airlift.bin` (flashed over its own USB after a full
4 MB stock backup, `huzzah32-stock-backup.bin`). Bring-up needed the
`feather-wire-probe` twice (miswired CS, then a disturbed READY wire)
and surfaced the `server.accept()` finding (separate bench note).
Monitors: scripted serial capture + a pure-listener TCP client from
this PC; BLE via phone (nRF Connect on **PyntRTK-soak**).

All A.1 gates green over **72 min** (final: `writes=4369 up=4376s`):

- **No resets:** writes counter monotonic across all 144 reports;
  1 write/s ≈ uptime throughout.
- **SD:** `sdErrors=0`; **worst write 109.7 ms** (vs 93.6 ms Pynt
  WiFi-only baseline — same order, one U1-class latency spike; well
  inside the platform.md buffer margins).
- **WiFi:** `drops=0` for the entire run; RTCM-sized TCP traffic
  unaffected.
- **TCP:** one connection held 70+ min — **4262 lines at 1 Hz, zero
  reconnects, max inter-line gap 1.6 s**; accepted as a silent
  listener via `server.accept()` within one loop pass.
- **BLE:** five connect sessions totaling ≈48 min (longest 20.5 min
  continuous), interleaved with deliberate bounces — every disconnect
  re-advertised and re-accepted, TCP/SD/WiFi undisturbed through all
  of them.
- `soak.bin` post-read on a card reader (expected ≈ 2.24 MB =
  4369 × 512 B): left to the owner, optional — the CRC-stamped records
  and `sdErrors=0` already cover the integrity gate's intent.

**Consequence: the shared-SPI contention question is answered on spare
hardware after all — SD + WiFi + BLE coexist cleanly on one bus with
the custom firmware and upstream stack. Section 7 (Pynt) proceeds with
one less unknown.**

## Appendix A — standalone Spike C procedures

### A.1 — original AirLift FeatherWing procedure

<details>
<summary>Superseded 2026-07-27 — kept for a future bench that has the
AirLift FeatherWing with its GPIO0 jumper soldered closed and a spare
SD-equipped host. The <code>feather-soak-coex</code> and
<code>passthrough-feather</code> envs still build.</summary>

This was the Pynt's dress rehearsal: SD writes + WiFi TCP + BLE notify
contending on one SPI bus for an hour, on spare silicon.

Hardware prep:

- [ ] **Close the Wing's GPIO0 solder jumper.** Needed twice over: the
      passthrough uses GPIO0 (pin 10) to enter the ROM loader, and
      WiFiNINA 2.x polls GPIO0 as the module's data-ready IRQ during
      normal operation (stock Adafruit stack never did — this is new).
- [ ] Wing stacked on the Adalogger (Wing CS/BUSY/RESET/GPIO0 =
      pins 13/11/12/10; onboard SD CS = 4).
- [ ] FAT32 microSD in the Adalogger slot (a spare — `soak.bin` gets
      truncated each boot).
- [ ] Flash the **Wing's** ESP32: `passthrough-feather` + section 3
      commands (own backup file: `wing-stock-backup.bin`).

Run:

```
pio run -e feather-soak-coex -t upload && pio device monitor -b 115200
```

Connect a `nc` client and a BLE subscriber (device name **PyntRTK-soak**)
as in Spike B, then leave it for ≥ 60 min.

Pass gates:

- [ ] ≥ 60 min, `up=` never restarts
- [ ] `sdErrors=0`, writes ≈ uptime seconds (one 512 B record/s + sync
      every 8th — the rover's cadence)
- [ ] Worst SD write latency recorded here: `______ ms` (Pynt bench saw
      93.6 ms WiFi-only; expect same order under coex)
- [ ] WiFi drops recover unaided; BLE stays subscribable throughout
- [ ] `soak.bin` readable afterwards, size ≈ writes × 512

</details>

### A.2 — HUZZAH32 variant (no soldering) — env `feather-soak-coex-huzzah`

A HUZZAH32 Feather running `NINA_W102-3.0.1-airlift.bin` stands in for
the AirLift Wing as the nina co-processor. Same ESP32 silicon family as
the NINA-W102; nina-fw doesn't care about the carrier. Bonus: it
revalidates the MOSI-14 patch on a second board, and the HUZZAH32's own
USB console shows nina-fw boot output.

**⚠ Do NOT physically stack the two Feathers.** Feather stacking ties
same-POSITION header pins together, but this rig's map is deliberately
scrambled (the module's raw GPIOs vs. the host's SPI header + 13/11/12).
A naive M0-Adalogger-on-HUZZAH32 stack mis-connects every single line:

| Stacked position | What actually connects | Why it's wrong |
|---|---|---|
| SCK ↔ "SCK" | M0 SPI clock → **GPIO5 = nina CS** | clock hammered into chip-select |
| MOSI ↔ "MOSI" | M0 data-out → **GPIO18 = nina SCK** | data into clock |
| MISO ↔ "MISO" | M0 data-in ← GPIO19 | not nina MISO at all — the real one (GPIO23) lands on the M0's **SDA** pin |
| D13 ↔ "13" | host CS → GPIO13 | unused by nina-fw; the module is never selected |
| D11 ↔ "27" | host BUSY ← GPIO27 | unused; the real BUSY (GPIO33) lands on M0 **D10** |
| D12 ↔ "12" | host RESET → **GPIO12 = MTDI flash-voltage strap** | worst one: a high level here during ESP32 boot straps the flash to 1.8 V — the module may not boot at all |
| RST ↔ "RST" | M0 reset rail ↔ ESP32 EN | resets tangle: the host's reset pulses (and auto-reset) yank the ESP32, and SpiDrv's reset pin (D12) isn't on EN anyway |
| D5 ↔ "14" | M0 D5 ← nina MOSI (GPIO14) | the patched MOSI ends up on an unused host pin |
| USB ↔ USB, BAT ↔ BAT | **both boards' 5 V USB rails hard-paralleled** | with each board on its own USB (which this procedure wants), current flows between the two host ports — never tie them |

**Instead: keep both boards on their headers, unseated, and wire
point-to-point with jumpers** (female-to-female onto the header pins is
fine; keep them short — the SPI runs at 8 MHz). Common ground is
mandatory; each board runs from its **own USB** (M0 = host + serial
monitor, HUZZAH32 = flashing + nina console).

| Function | M0 host pin (env `-D`s) | HUZZAH32 silkscreen (actual GPIO) |
|---|---|---|
| CS | D13 | **"SCK"** (GPIO5) |
| BUSY/READY | D11 | **"33"** (GPIO33) |
| RESET | D12 | **"RST"** (EN) |
| SCK | SCK (SPI header) | **"MOSI"** (GPIO18) |
| MOSI | MOSI (SPI header) | **"14"** (GPIO14 — the patched pin) |
| MISO | MISO (SPI header) | **"SDA"** (GPIO23) |
| GND | GND | GND |
| data-ready IRQ | D10 — **leave unwired** | (GPIO0 is not on the HUZZAH32 headers) |

The missing GPIO0 is the one functional difference: nina-fw drives it
as a data-ready IRQ and WiFiNINA 2.x polls it in `SpiDrv::available()`.
The `feather-soak-coex-huzzah` env sets `-DSOAK_FORCE_GPIOIRQ_HIGH`,
which pulls host pin 10 high (INPUT_PULLUP) right after driver init —
`available()` then always falls through to a real SPI query:
functionally correct, slightly chattier on the bus. Consequence: this
variant does NOT exercise the IRQ-gated accept path — but the Metro
(GPIO0 board-wired) already did, and the Pynt has it wired too.

**Flashing the HUZZAH32 — no passthrough, no bridge:** it has its own
USB-serial with auto-program. Plug it in alone and:

```bash
esptool --port COMx write-flash 0 NINA_W102-3.0.1-airlift.bin
```

(Stock CLI, compression, default reset handling — none of the
SAMD-bridge workarounds apply. The 2 MB image on the WROOM32's 4 MB
flash at offset 0 is fine.) Back up its stock MicroPython/whatever
first with `read-flash` if you care about it.

**Run:** wire per the table, then

```
pio run -e feather-soak-coex-huzzah -t upload && pio device monitor -b 115200
```

and follow A.1's soak procedure and pass gates unchanged (same sketch,
same gates — only the IRQ define differs).
