# PyPortal-Pynt-RTK — Single-Board RTK Field Device

Third build in the RTK series, following the Metro ESP32-S3 build
([`rah2003/SWMaps-propertylines`](https://github.com/rah2003/SWMaps-propertylines),
branch `claude/esp32-rtk-bridge-firmware-yokgan`) and the interim two-MCU
RTK-Feather build (local, `C:\Projects\RTK-Feather`). This build collapses
everything onto one board: an Adafruit PyPortal Pynt drives the ArduSimple
simpleRTK2B Lite (u-blox ZED-F9P) over **F9P UART1**, keeping **UART2 free
for a future plug-in radio**.

## Build one yourself

The short path from parts to a working rover. Everything below is the
condensed version — the linked docs carry the full detail and the bench
history behind each step.

### 1. Parts (BOM)

| Part | Source | ≈ Cost |
|---|---|---|
| Adafruit PyPortal Pynt | [adafruit.com #4465](https://www.adafruit.com/product/4465) | $45 |
| ArduSimple simpleRTK2B **Lite** (u-blox ZED-F9P) | [ardusimple.com](https://www.ardusimple.com/product/simplertk2b-lite/) | $180 |
| Calian/Tallysman **HC977** helical antenna (33-HC977-35) + SMA pigtail | [calian.com GNSS](https://www.calian.com/advanced-technologies/gnss/) | $250 |
| microSD card (32 GB, UHS-I U1 or better; PNY Elite used here) | anywhere | $10 |
| JST-GH 4-pin pigtail (Pixhawk-style, for the Lite's UART1) + 3-pin JST-PH leads for D3/D4 | anywhere | $10 |
| USB power bank (2.4 A port) + micro-B cable | anywhere | $20 |

≈ **$500** total (prices drift — check the links). You'll also want an
NTRIP caster subscription with VRS coverage in your area (this build
uses acorn-gnss.net), or your own base station.

### 2. Wire it

Four conductors: Pynt **D3/D4 UART** ↔ Lite **UART1** (via the Pixhawk
JST-GH), plus 5 V + GND from the Pynt's D3/D4 socket to the Lite.
⚠ **Check the label swap first**: Pynt D3/D4 silkscreen is swapped vs.
the classic PyPortal (confirmed real on this unit) — run the loopback
test in [docs/hardware/wiring.md](docs/hardware/wiring.md) and
tape-label the sockets before trusting anything. Antenna on the Lite's
SMA; microSD in the Pynt's slot.

### 3. Flash it

Prereq: [PlatformIO CLI](https://platformio.org/), and the repo cloned.
The Pynt flashes over its micro-B USB (1200-baud touch; if the port
wedges in bootloader, press reset and retry).

| Env | What it is | When you use it |
|---|---|---|
| `pynt-rover-coex` | **The firmware to run** — WiFi + BLE + web GUI | `pio run -e pynt-rover-coex -t upload` |
| `pynt-rover` | WiFi-only fallback for a **stock** AirLift (no BLE/web) | rollback path only |
| `pynt-bringup` | Phase 1 hardware test menu (TFT, touch, UART, SD, WiFi) | first assembly, or debugging hardware |
| `passthrough-*` | esptool bridges for reflashing the AirLift | one-time AirLift step below |

One-time prerequisite for `pynt-rover-coex`: the AirLift co-processor
must run the vendored `NINA_W102-3.0.1-airlift.bin` (WiFi+BLE coex
firmware, committed in `firmware/nina-fw-airlift/`) — back up the stock
firmware first, then flash via the passthrough bridge
([docs/coex-bench.md](docs/coex-bench.md) §3 + §7.2, rollback §8).

### 4. First boot

1. TFT shows the fix-state header (NO GNSS → NO FIX → 3D → FLOAT →
   FIXED) over the POS page. **PAGE** cycles POS → SYS → WEB.
2. Configure over USB serial (115200, type `help`) or by editing
   `/config.txt` on the SD card: WiFi SSID/pass (2 slots), caster
   host/mount/credentials. Settings persist on the SD.
3. The **WEB page** on the TFT shows the web GUI's admin password
   (user `admin`), generated on-device at first web boot. Browse to
   the device IP (on the TFT/serial status) for the dashboard — or
   tap **AP** on the WEB page to provision WiFi from a phone via the
   device's own hotspot at `192.168.4.1`.
4. Phone links: **SW Maps** (iOS) → Bluetooth rover `PyntRTK-rover`;
   **QField/SW Maps (Android)** → TCP NMEA at `<device-ip>:10110`.
   Both run concurrently.

### 5. Field use

- **LOG** button (or web Logging card, or serial `log on`): start/stop
  RAWX `.ubx` logging to SD. Files pre-allocate 32 MiB; a file whose
  size is exactly 32,768 KB is an unclosed session — parse to the last
  valid frame ([tools/ubx_walk.py](tools/ubx_walk.py)).
- **PWR** button, two taps: safe shutdown (closes the log file).
- Rover ↔ base: serial `mode=base` / `mode=rover`, live.
- Read the header: fix state + SIV; CORR line = the receiver's own
  correction age ("lnk" suffix = link-derived fallback); SYS page has
  buffers/clients; C/N0, baseline and jamming indicators are on the
  web dashboard and serial `status` (`corr:`/`rf:` lines).
- Measured on this unit: ≥8.36 h on the field battery bank with RAWX +
  NTRIP + TCP + BLE running (battery-limited, zero data gaps).
- **Positions are NAD83(2011) epoch 2010.00, not WGS84** — that's the
  frame the ACORN VRS corrections are in. Set SW Maps/QField to
  EPSG:6318 (or the matching AK State Plane zone) before overlaying on
  control data: [docs/datum-epoch.md](docs/datum-epoch.md).

## Hardware

| Role | Part |
|---|---|
| MCU + display | Adafruit PyPortal Pynt (#4465) — ATSAMD51J20 @ 120 MHz, 2.4" 320×240 ILI9341 TFT (8-bit parallel), resistive touch, microSD, 8 MB QSPI flash |
| WiFi | ESP32 AirLift co-processor (NINA-W102) on the Pynt, driven over SPI (WiFiNINA) |
| GNSS | ArduSimple simpleRTK2B Lite (u-blox ZED-F9P), UART1 via Pixhawk JST-GH — **moved over from the RTK-Feather rig** |
| Antenna | Calian/Tallysman HC977 helical (33-HC977-35, 35 dB LNA), triple-band + L-band, 3.3 V bias ~21 mA, no ground plane needed |
| Storage | PNY Elite 32 GB microSDHC UHS-I U1 (in the Pynt's slot) |
| Phone | iPhone + **SW Maps over BLE NUS** (Phase 5, verified 2026-07-28) and/or **QField over WiFi TCP** (verified 2026-07-19) — both links run simultaneously |

## Hard constraints (inherited + new)

1. **F9P UART2 / XBee socket is reserved** for a future plug-in radio. No
   firmware function may depend on it. (Base mode may *configure* RTCM3
   output on UART2 — config only, no wiring.)
2. **F9P UART1 is the Pynt's working port** (RTCM3 in; NMEA + UBX out) via
   the Pynt's D3/D4 hardware UART. ⚠ **The Pynt's D3/D4 sockets are
   reportedly silkscreen-swapped** vs. the classic PyPortal — verify with a
   scope/loopback before trusting labels (see `docs/QUESTIONS.md`).
3. ~~The AirLift cannot run WiFi and BLE simultaneously.~~ **Retired
   2026-07-28 (Phase 5):** the AirLift now runs the vendored
   `nina-fw 3.0.1-airlift` module firmware (BLE HCI multiplexed onto the
   SPI link) with the upstream WiFiNINA 2.1.1 + ArduinoBLE 2.1.0 host
   stack — WiFi + BLE simultaneous, proven in a 75-min soak on the
   assembled unit (`docs/coex-bench.md`). The `pynt-rover` env still
   builds the original WiFi-only Adafruit-fork stack, byte-identical,
   as the rollback path; `pynt-rover-coex` is the live configuration.
4. **The ESP32 AirLift and the microSD share the main SPI bus.** NTRIP/TCP
   traffic and RAWX log writes contend — the logging pipeline must tolerate
   SPI stalls (two-stage buffering, ported from the Feather build).
5. **The Lite's USB is an FTDI bridge onto UART1.** Never attach the
   u-center adapter while the Pynt drives UART1 — one driver at a time.

## Architecture (single MCU, cooperative superloop or light RTOS-less tasks)

```
                          ┌──────────── PyPortal Pynt (SAMD51) ────────────┐
 iPhone hotspot ──WiFi──► │ ESP32 AirLift ◄─SPI─► NTRIP client ─┐          │
                          │  (WiFi+BLE coex)                    ▼          │
 QField ◄───TCP/NMEA────► │  TCP server ◄──┬─ NMEA tee ◄─ UART (D3/D4) ◄───┼──► F9P UART1
 SW Maps ◄──BLE NUS─────► │  NUS notify ◄──┘      │       RTCM3 ▲──────────┘   (UART2: radio,
 browser ◄──HTTP :80────► │  web config GUI       │                            untouched)
                          │  UBX extractor ◄──────┘                        │
                          │       ▼                                        │
                          │  microSD (.ubx RAWX, shared SPI)               │
                          │  TFT 320×240 (8-bit parallel) + resistive touch│
                          └────────────────────────────────────────────────┘
```

Module lineage (ported/adapted copies, per the Feather precedent — not
submodules): `ringbuf.h`, `ubx_extractor`, `ntrip_client`, `gnss_config`,
`settings`/serial-menu from RTK-Feather & Metro. The Feather's two-MCU
`status_link` protocol disappears entirely. New modules: TFT/touch UI
(Adafruit_ILI9341 in 8-bit-parallel mode + touch driver), WiFiNINA transport
for NTRIP, TCP NMEA server, SD logger on shared SPI.

## Decisions (2026-07-11)

- **Toolchain:** PlatformIO, Arduino C++ (Adafruit SAMD51 core) — maximizes
  reuse of the proven Metro/Feather modules.
- **v1 scope:** Rover **and** Base mode, with RAWX `.ubx` logging to the
  Pynt's own microSD.
- **Phone link:** ~~SW Maps via WiFi TCP; no BLE ever on this hardware~~
  → since Phase 5: SW Maps over BLE NUS + QField over TCP, concurrently.
- **Touch UI:** status pages (fix quality, correction age, NTRIP state,
  logging, sat count; C/N0 + RF/jamming health live on the web dashboard
  and serial `status` since 2026-08-04) + touch controls (log start/stop,
  rover/base switch, safe shutdown). Credentials/config via USB serial
  menu — no on-screen keyboard in v1. **Portrait orientation (240×320)**
  — enclosure comes after the electronics and will be built around
  portrait mounting.
- **Repo:** public GitHub, `PyPortal-Pynt-RTK`. Secrets via gitignored
  `secrets.h` (`secrets.example.h` committed), same as Feather.
- **Inherited defaults (from Metro via Feather):** caster
  `www.acorn-gnss.net:2101` (2026-07-19: the bare apex hostname the Metro
  build used lost its DNS A record — `www.` is the live caster now),
  mounts `VRS_SouthCentral_RTCM3`/`MS_RTCM3`, WiFi
  via iPhone personal hotspot, elevation mask 12°, constellations
  GPS+GLO+GAL+BDS (SBAS/QZSS off), F9P UART1 @ 115200.

## Repository layout

```
LICENSE                     MIT (vendored libs keep their own licenses)
docs/
  README.md                 Index: which docs to read to build vs. for history
  QUESTIONS.md              Open/answered clarifying questions
  hardware/
    wiring.md               4-wire interconnect, label-swap check, SERCOM recipe
    power.md                Power tree decision (shape A adopted) + checks P1-P4
    platform.md             Bus map, SPI contention math, TFT/touch notes
    ucenter-config.md       F9P re-verification (unit moved from Feather rig)
    checklists.md           Per-board bring-up + integration gates
  coex-bench.md             Phase 5 bench + 2026-08 freeze/boot-loop postmortems
  web-config-spike.md       Web GUI plan + W0-W5 bench results
  reviews/                  2026-08-03 four-angle team review + response plan
firmware/
  pynt/bringup/             Phase 1 serial-menu test suite
  pynt/rover/               Rover firmware (superloop: gnss, ntrip, tcp_nmea,
                            ble_nus*, web_config*, sd_logger, ui, serial_menu,
                            settings; wdt = watchdog + fault breadcrumbs,
                            uart_gnss = per-instance-sized GNSS UART —
                            * = coex env only)
  nina-fw-airlift/          Vendored arduino/nina-fw 3.0.1 + MOSI-14 patch
                            (the AirLift module firmware; AIRLIFT.md)
  spike/                    Spare-board spikes + esptool passthrough bridges
lib/                        Vendored ArduinoBLE 2.1.0 (patched) + Arduino_SpiNINA
tools/webui/                Web GUI source -> gzipped PROGMEM asset pipeline
tools/ubx_walk.py           .ubx frame walker: checksums, RAWX epochs, gap check
platformio.ini              pynt-bringup, pynt-rover (WiFi-only fallback),
                            pynt-rover-coex (live: WiFi+BLE+web), spikes,
                            passthrough-* flash bridges
```

## Phases

- [x] **Phase 0 — Hardware verification pack** (`docs/hardware/`): wiring
      diagram (Pixhawk JST pinout → D3/D4, incl. the label-swap check),
      power tree (**shape A adopted** — single bank, Lite fed from D3/D4
      5 V; P1/P2 measured 2026-07-17, P3/P4 closed by soak evidence — no
      brownout resets across every soak through the 8.36 h battery run),
      u-center re-verification checklist, bring-up checklists,
      platform/SPI-contention analysis — **verify wiring against these
      documents and current vendor docs before powering anything**.
- [x] **Phase 1 — Bring-up sketches**: serial test menu covering TFT +
      portrait rotations, touch raw dump, AirLift version/scan/join + TCP
      echo (the SW Maps path), SD write latency, D3/D4 UART loopback +
      TX-socket identify (the label-swap tests), live NMEA echo, free-RAM
      report. **Bench pass complete** (`docs/hardware/bringup-log.md`,
      2026-07-17): silkscreen swap confirmed real (sockets tape-labeled),
      touch pin off-by-one found and fixed (18/19/20/21), rotation 2
      chosen, corner calibration captured, WiFi/TCP/SD all healthy.
- [x] **Phase 2 — Rover firmware**: written and **compiling clean**
      (2026-07-11: RAM 9.7 % / flash 10.3 %, `pynt-rover` env). NTRIP
      state machine (ported Metro→Feather→here, polled instead of tasked)
      → RTCM3 → F9P UART1; GNSS config via SparkFun v3 VALSET; RAWX/SFRBX
      → SparkFun file buffer → time-named `.ubx` on SD (one bounded
      512 B write per loop pass, pre-allocated contiguous files, 8 s
      sync); portrait touch UI (fix-state header, POS/SYS pages, LOG /
      PAGE / PWR buttons with two-tap shutdown confirm); SW Maps TCP NMEA
      server on :10110; SD `/config.txt` settings + serial menu.
      **1-hour full-stack soak PASSED 2026-07-19** (bringup-log.md §3
      gate 4): 66 min, zero resets, RTK FIX from minute 2 to the end,
      NTRIP uninterrupted, 10.8 MB RAWX written (~10 MB/h). Found live:
      caster DNS moved to `www.`, nina-fw TCP accept is data-gated, and
      a client-dedup bug (fixed) — see Q19. Touch buttons + BUFH both
      closed same day (bringup-log.md: all three buttons owner-verified;
      BUFH typically 9–14 KB against the 32 KB file buffer).
- [x] **Phase 3 — Base mode**: written and **compiling clean** (2026-07-11:
      RAM 9.7 % / flash 11.1 %). Survey-in (dur/acc from settings) or
      fixed-LLH TMODE; RTCM3 out **configured on UART2 only** (MSM4 set
      1005/1074/1084/1094/1124/1230 — config only, the XBee socket stays
      empty and UART2 baud is left for the future radio to decide);
      NAV-SVIN progress in the header + SVIN line; `b_` log prefix; live
      rover/base switch via serial menu (`mode=base`). **Spot check
      PASSED 2026-07-19**: live switch both directions, survey-in valid
      at 300 s / 1.46 m on the defaults, UART2 untouched. (The `b_`
      prefix needed a same-day fix — the log now rotates on a live mode
      switch; rotation re-checked and confirmed 2026-07-19.)
- [ ] **Phase 4 — Polish**: logging analytics, light-sensor auto-dim,
      speaker fix/loss chime. (On-screen config keyboard **descoped** —
      the Phase 6 web GUI covers all credential/config entry.)
- [x] **Phase 5 — WiFi+BLE coexistence — DONE 2026-07-28**
      (`docs/coex-bench.md`, branch `coex/nina-fw-airlift`): vendored
      arduino/nina-fw 3.0.1 with a one-pin AirLift patch (SPI MOSI
      12→14), host stack swapped to upstream WiFiNINA 2.1.1 +
      ArduinoBLE 2.1.0 (vendored, 2-line SPI-HCI transport gate patch)
      + Arduino_SpiNINA. De-risked on a Metro M4 AirLift and a
      hand-wired HUZZAH32 rig before touching the Pynt (stock firmware
      backed up first; §8 rollback keeps `pynt-rover` untouched).
      75-min integration soak on the assembled unit: RAWX + NTRIP +
      TCP + BLE NUS concurrent, zero NTRIP teardowns, 50k TCP
      sentences no drops, ~58 min BLE streaming — **owner ran iOS
      SW Maps live over BLE** (Q19 closed, both phone paths).
      Notable firmware truths found and designed around: data-gated
      `server.available()` (fixed via `accept()`), `WiFi.end()` wedges
      STA rejoin, the NINA socket table leaks across `WiFi.end()`,
      `scanNetworks()` kills any live network, and this Pynt's UF2
      bootloader wedges when parked idle (flash via 1200-touch flow).
- [x] **Phase 6 — Web config GUI — v1 (W0–W5) DONE 2026-07-31**
      (`docs/web-config-spike.md`, branch `web-config`): on-board HTTP
      server (port 80, Basic auth, TRNG-generated passwords shown on
      the TFT WEB page) serving a gzipped-from-PROGMEM SPA — live
      status dashboard, WiFi provisioning with a touch-button AP mode
      (both AP transitions are reboots by design), GNSS tuning (rate /
      dynamic model / mask / GSV via live VALSET), caster management
      (live reconnect), logging control, BLE + System cards (instant
      admin-password rotation). W6 (mDNS, sourcetable browser, SD file
      manager, profiles) deferred.
- [x] **Team-review response, Phases A–C — DONE 2026-08-04**
      (`docs/reviews/2026-08-03-team-review.md` + response plan; PRs #2,
      #3): pre-field hardening (PVT-age fix badge, SAMD51 WDT with
      per-module breadcrumb + HardFault PC/LR/CFSR crumbs in backup RAM,
      socket lifecycle, TCP client drop logic, GGA age gate, atomic
      settings save), correction/RF instrumentation (RXM-COR, RELPOSNED
      baseline, NAV-SAT C/N0, MON-RF jamming/AGC, receiver-truth
      correction age), and the fix for the **2026-08-02 freeze cluster
      root cause**: a global `SERIAL_BUFFER_SIZE=32768` cost 131 KiB of
      RAM (both rings × both Uart instances) and starved heap+stack into
      collision — replaced with the per-instance `GnssUart` (96 KiB
      reclaimed). Validated by a 180-min regression soak under the exact
      harness that froze the old build 8/8 times: **zero resets**. Also:
      **RTK FIXED reached and held outdoors 2026-08-02**; SD integrity
      gate closed (86.76 MB / 30,099 epochs / 0 bad checksums, gap-free);
      ≥8.36 h measured field-battery runtime.

## Reference builds

| | Metro (primary) | RTK-Feather (interim) | **This build** |
|---|---|---|---|
| MCU | ESP32-S3 (dual core) | HUZZAH32 + M0 Adalogger | SAMD51 (single) |
| WiFi | native | native (HUZZAH32) | ESP32 AirLift co-proc (SPI) |
| Display | — | 128×32 OLED + 3 buttons | 320×240 TFT + touch |
| Logging | on-board | second MCU + SD | on-board microSD (shared SPI) |
| Phone | BLE NUS | deferred | BLE NUS + WiFi TCP, concurrent |
| Config | serial | serial | web GUI + touch + serial |
| Status | Phases 1–3 built | retired (boards are coex spares) | Phases 0–3, 5, 6 + review response done; RTK FIXED outdoors 2026-08-02; ≥8.36 h field runtime; OPUS truth-check pending (plan Phase E) |
