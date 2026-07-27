# WiFi + BLE Coexistence Bench — spare-board spikes

Working doc for branch `coex/nina-fw-airlift`. Goal: prove simultaneous
WiFi + BLE on AirLift hardware with custom module firmware **on spare
boards first**. Companion to `firmware/nina-fw-airlift/AIRLIFT.md`
(module firmware) and the `VENDORED.md` notes in `lib/`.

> **HARD RULE — the assembled Pynt RTK unit is NOT touched until Spikes
> A, B and C have all passed.** It passed WiFi-only bench (bringup-log
> sections 1–3) on stock Adafruit firmware + the `pynt-rover` env, and
> that known-good state is the fallback for the whole experiment.

Order of operations:

1. Build the module firmware (section 2) — laptop only, no hardware.
2. Metro M4 AirLift Lite: backup, flash, Spike A, Spike B (sections 3–5).
3. Feather M0 Adalogger + AirLift FeatherWing: flash Wing, Spike C soak
   (section 6).
4. Only then: the Pynt (section 7).

## 1. Pinned version set

| Piece | Version | Where |
|---|---|---|
| Module firmware | arduino/nina-fw **3.0.1** (`57d12f4`) + MOSI 12→14 | vendored `firmware/nina-fw-airlift/` |
| ESP-IDF (builds it) | **v4.4.8**, installed in WSL2 Ubuntu-22.04 at `/opt/esp-idf-v4.4.8` (Docker `espressif/idf:v4.4.8` equivalent) | this machine / AIRLIFT.md |
| WiFiNINA (host) | **2.1.1** (`272159c`) | `platformio.ini` git-tag pin |
| ArduinoBLE (host) | **2.1.0** (`281377b`) + 2-line gate patch | vendored `lib/ArduinoBLE/` |
| Arduino_SpiNINA (host) | **0.0.2** (`200cc35`), unpatched | vendored `lib/Arduino_SpiNINA/` |
| Adafruit SAMD core | **1.7.16** (PIO `framework-arduino-samd-adafruit 1.10716.0`) | pinned by platform `atmelsam` |
| esptool | **5.3.1**, installed on the Windows side (`esptool` on PATH; 5.x command names are dashed: `read-flash`, `write-flash`) | laptop |

Rollback binaries: per-board `esptool read-flash` dumps (primary) and
stock `NINA_W102-1.7.x.bin` from
<https://github.com/adafruit/nina-fw/releases> (secondary).

## 2. Build the module firmware (no hardware needed)

**Done 2026-07-26** on this machine via WSL2 Ubuntu-22.04 + ESP-IDF
v4.4.8 (setup + exact commands: `firmware/nina-fw-airlift/AIRLIFT.md`).
Output already sits at
`firmware/nina-fw-airlift/NINA_W102-3.0.1-airlift.bin` — exactly
2,097,152 bytes, `0xE9` image magic at 0x1000 and 0x30000, "3.0.1"
version string embedded. One image for all three boards, flashed at
offset 0x0. Gitignored (reproducible); park backups next to it.

To rebuild from scratch: WSL path in AIRLIFT.md (verified), or Docker
`espressif/idf:v4.4.8` (equivalent, untested here). Either way the
flashable file is combine.py's **`_ALL.bin`** output.

## 3. Flashing procedure (Metro shown; same flow for every board)

The ESP32's serial bootloader is mask ROM — an interrupted or bad flash
is **always** recoverable by redoing this section. Reflash-risk, not
brick-risk.

1. `pio run -e passthrough-metro -t upload` — the sketch straps the ESP32
   into its ROM loader (GPIO0 low through a reset pulse) and bridges
   USB↔NINA UART at a fixed 115200.
2. Note the board's COM port (Device Manager, or `pio device list`).
3. **Backup first** (~3 min at 115200):

   ```bash
   esptool --port COM7 --baud 115200 --before no_reset --after no_reset read-flash 0 0x200000 metro-stock-backup.bin
   ```

4. Write the custom image:

   ```bash
   esptool --port COM7 --baud 115200 --before no_reset --after no_reset write-flash 0 NINA_W102-3.0.1-airlift.bin
   ```

Keep `--baud 115200` and the two `no_reset` flags: the passthrough owns
the strap pins and its bridge speed is fixed. After flashing, upload the
next spike env — its own boot resets the module into the new firmware.

**Recovery** = same passthrough, `write-flash 0 <backup>.bin` (or the
Adafruit release bin).

Per-board passthrough envs: `passthrough-metro`, `passthrough-feather`
(Wing pins via build flags), `passthrough-pynt` (**locked until
section 7**).

## 4. Spike A — upstream WiFi stack alone (Metro)

Prereqs: custom firmware flashed (section 3);
`firmware/spike/secrets.h` created from `secrets.example.h`; hotspot up.

```
pio run -e metro-spike-wifi -t upload && pio device monitor -b 115200
```

Pass gates — all four, else stop and debug before any BLE work:

- [ ] No `AirLift not responding` (SPI link alive through the new firmware
      → the MOSI 12→14 patch is right)
- [ ] `NINA firmware: 3.0.1` (stock would say 1.7.x)
- [ ] Scan lists the hotspot SSID
- [ ] Join succeeds; IP + RSSI keep printing at 5 s cadence, no drops
      while parked next to the phone

## 5. Spike B — WiFi + BLE simultaneously (Metro)

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

- [ ] TCP stream and BLE notifications run **concurrently** ≥ 15 min
- [ ] `up=` counter never restarts (no watchdog/brownout resets)
- [ ] `drops=0`, or any WiFi drop re-joins by itself
- [ ] BLE disconnect/reconnect mid-run doesn't disturb the TCP side
- [ ] Serial line stays clean (no SPI timeout spew)

Record the numbers in `docs/hardware/bringup-log.md` style notes at the
bottom of this file.

## 6. Spike C — shared-SPI soak (Feather M0 Adalogger + AirLift Wing)

This is the Pynt's dress rehearsal: SD writes + WiFi TCP + BLE notify
contending on one SPI bus for an hour.

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

## 7. Pynt (LOCKED until A–C pass)

Gate: sections 4, 5, 6 all green, numbers recorded below.

The unlock sequence (do not start any of it early):

1. `pio run -e passthrough-pynt -t upload`; backup to
   `pynt-stock-backup.bin`; flash the same `NINA_W102-3.0.1-airlift.bin`
   (section 3 commands).
2. `pio run -e pynt-rover-coex -t upload` — the rover on the upstream
   stack, still WiFi-only in behavior. Re-run the Phase-2 checks and the
   deferred gate-4 1-hour soak (bringup-log section 3) on THIS stack:
   the WiFiNINA-fork assumptions in `ntrip.cpp` (blocking `WiFi.begin()`
   window, `server.available()` accept semantics) were characterized
   against the Adafruit 1.x fork and must be re-verified.
3. Only then: add the BLE module (`FEATURE_BLE`, NUS to SW Maps — Spike
   B's service, ported behind a feature gate) and re-soak in coex mode.

Instant fallback at any point: `passthrough-pynt` + `write-flash 0
pynt-stock-backup.bin`, then `pio run -e pynt-rover -t upload` — the
shipping env still builds against the pinned Adafruit fork and was left
untouched.

## Bench notes / results

(append dated notes here as sections close, bringup-log style)
