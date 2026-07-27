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

> **This host (Windows, OfficeSER): the stock esptool CLI does NOT work
> through the bridge — use `firmware/nina-fw-airlift/flash_via_bridge.py`
> instead** (bench note 2026-07-26): esptool forces DTR/RTS low on
> Windows, the SAMD CDC core drops device→host bytes until the host
> asserts them, so the CLI never sees the ROM loader. The wrapper opens
> the port with DTR/RTS asserted and hands it to `esptool.main(esp=...)`.
> Compressed writes also die deterministically mid-stream here — pass
> `--no-compress` to `write-flash`:
>
> ```bash
> python flash_via_bridge.py COM9 read-flash 0 0x200000 <board>-stock-backup.bin
> python flash_via_bridge.py COM9 write-flash --no-compress 0 NINA_W102-3.0.1-airlift.bin
> ```

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

- [x] No `AirLift not responding` (SPI link alive through the new firmware
      → the MOSI 12→14 patch is right)
- [x] `NINA firmware: 3.0.1` (stock would say 1.7.x)
- [x] Scan lists the hotspot SSID
- [x] Join succeeds; IP + RSSI keep printing at 5 s cadence, no drops
      while parked next to the phone

**All four passed 2026-07-26** — see bench notes below.

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

- [x] TCP stream and BLE notifications run **concurrently** ≥ 15 min
- [x] `up=` counter never restarts (no watchdog/brownout resets)
- [x] `drops=0`, or any WiFi drop re-joins by itself
- [x] BLE disconnect/reconnect mid-run doesn't disturb the TCP side
- [x] Serial line stays clean (no SPI timeout spew)

**All five passed 2026-07-26** — see bench notes below.

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
- **Bonus finding:** the TCP client was accepted **without sending a
  byte** — upstream nina-fw 3.0.1's `server.available()` is NOT
  data-gated. The Q19 silent-client workaround (send-anything-once /
  UDP fallback) is a stock-Adafruit-1.7.x behavior only; listen-only
  clients (u-center) should Just Work on this firmware. Re-verify on
  the Pynt in section 7 step 2.

Next: Spike C (section 6) — needs the Feather M0 Adalogger + AirLift
Wing on the bench, Wing GPIO0 jumper closed, spare FAT32 microSD.
