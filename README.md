# PyPortal-Pynt-RTK — Single-Board RTK Field Device

Third build in the RTK series, following the Metro ESP32-S3 build
([`rah2003/SWMaps-propertylines`](https://github.com/rah2003/SWMaps-propertylines),
branch `claude/esp32-rtk-bridge-firmware-yokgan`) and the interim two-MCU
RTK-Feather build (local, `C:\Projects\RTK-Feather`). This build collapses
everything onto one board: an Adafruit PyPortal Pynt drives the ArduSimple
simpleRTK2B Lite (u-blox ZED-F9P) over **F9P UART1**, keeping **UART2 free
for a future plug-in radio**.

## Hardware

| Role | Part |
|---|---|
| MCU + display | Adafruit PyPortal Pynt (#4465) — ATSAMD51J20 @ 120 MHz, 2.4" 320×240 ILI9341 TFT (8-bit parallel), resistive touch, microSD, 8 MB QSPI flash |
| WiFi | ESP32 AirLift co-processor (NINA-W102) on the Pynt, driven over SPI (WiFiNINA) |
| GNSS | ArduSimple simpleRTK2B Lite (u-blox ZED-F9P), UART1 via Pixhawk JST-GH |
| Phone | iPhone + SW Maps over **WiFi TCP** (NMEA server on the hotspot network) |

## Hard constraints (inherited + new)

1. **F9P UART2 / XBee socket is reserved** for a future plug-in radio. No
   firmware function may depend on it. (Base mode may *configure* RTCM3
   output on UART2 — config only, no wiring.)
2. **F9P UART1 is the Pynt's working port** (RTCM3 in; NMEA + UBX out) via
   the Pynt's D3/D4 hardware UART. ⚠ **The Pynt's D3/D4 sockets are
   reportedly silkscreen-swapped** vs. the classic PyPortal — verify with a
   scope/loopback before trusting labels (see `docs/QUESTIONS.md`).
3. **The AirLift cannot run WiFi and BLE simultaneously.** WiFi/NTRIP wins;
   there is no BLE in this build. SW Maps connects over TCP instead.
4. **The ESP32 AirLift and the microSD share the main SPI bus.** NTRIP/TCP
   traffic and RAWX log writes contend — the logging pipeline must tolerate
   SPI stalls (two-stage buffering, ported from the Feather build).
5. **The Lite's USB is an FTDI bridge onto UART1.** Never attach the
   u-center adapter while the Pynt drives UART1 — one driver at a time.

## Architecture (single MCU, cooperative superloop or light RTOS-less tasks)

```
                          ┌──────────── PyPortal Pynt (SAMD51) ────────────┐
 iPhone hotspot ──WiFi──► │ ESP32 AirLift ◄─SPI─► NTRIP client ─┐          │
                          │       ▲                             ▼          │
 SW Maps ◄──TCP/NMEA────► │  TCP server ◄── NMEA tee ◄── UART (D3/D4) ◄────┼──► F9P UART1
                          │                     │            RTCM3 ▲───────┘   (UART2: radio,
                          │  UBX extractor ◄────┘                              untouched)
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
- **Phone link:** SW Maps via WiFi TCP; no BLE ever on this hardware.
- **Touch UI:** status pages (fix quality, correction age, NTRIP state,
  logging, sats/SNR) + touch controls (log start/stop, rover/base switch,
  safe shutdown). Credentials/config via USB serial menu — no on-screen
  keyboard in v1.
- **Repo:** public GitHub, `PyPortal-Pynt-RTK`. Secrets via gitignored
  `secrets.h` (`secrets.example.h` committed), same as Feather.
- **Inherited defaults (from Metro via Feather):** caster
  `acorn-gnss.net:2101`, mounts `VRS_SouthCentral_RTCM3`/`MS_RTCM3`, WiFi
  via iPhone personal hotspot, elevation mask 12°, constellations
  GPS+GLO+GAL+BDS (SBAS/QZSS off), F9P UART1 @ 115200.

## Phases

- [ ] **Phase 0 — Hardware verification pack** (`docs/hardware/`): wiring
      diagram (Pixhawk JST pinout → D3/D4, incl. the label-swap check),
      power tree (single-bank vs two-feed decision lives here), u-center
      pre-config checklist, bring-up checklists, SPI-contention analysis.
- [ ] **Phase 1 — Bring-up sketches**: TFT + touch smoke test, AirLift WiFi
      join + firmware version, SD write test, D3/D4 UART loopback + live
      NMEA echo, QSPI/heap headroom survey.
- [ ] **Phase 2 — Rover firmware**: NTRIP → RTCM3 → F9P UART1, GNSS config
      (UART1-only @115200), NMEA parse → status snapshot, touch UI pages +
      controls, RAWX tap → time-named `.ubx` on SD, SW Maps TCP server,
      serial config menu.
- [ ] **Phase 3 — Base mode**: survey-in / fixed position, RTCM3-on-UART2
      config (config only — no wiring), base status page.
- [ ] **Phase 4 — Polish**: on-screen config keyboard, logging analytics,
      light-sensor auto-dim, speaker fix/loss chime.

## Reference builds

| | Metro (primary) | RTK-Feather (interim) | **This build** |
|---|---|---|---|
| MCU | ESP32-S3 (dual core) | HUZZAH32 + M0 Adalogger | SAMD51 (single) |
| WiFi | native | native (HUZZAH32) | ESP32 AirLift co-proc (SPI) |
| Display | — | 128×32 OLED + 3 buttons | 320×240 TFT + touch |
| Logging | on-board | second MCU + SD | on-board microSD (shared SPI) |
| Phone | BLE NUS | deferred | WiFi TCP |
| Status | Phases 1–3 built | Phase 2 compiled, not field-run | planning |
