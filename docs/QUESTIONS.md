# Open Questions

Status legend: ❓ needs owner answer · 📄 resolve from vendor docs in Phase 0 ·
✅ answered.

## Answered at kickoff (2026-07-11)

1. ✅ **Toolchain** — PlatformIO Arduino C++ (port Metro/Feather modules).
2. ✅ **v1 scope** — Rover + RAWX logging + Base mode, all in v1.
3. ✅ **Phone link** — SW Maps over WiFi TCP; BLE permanently out (AirLift
   can't do WiFi+BLE together).
4. ✅ **Repo** — public GitHub, `PyPortal-Pynt-RTK`.
5. ✅ **Touch UI depth** — status pages + touch controls; config stays on
   the USB serial menu (no on-screen keyboard in v1).
6. ✅ **Inherited defaults** — Metro's answers carry over wholesale: caster
   `acorn-gnss.net:2101` (mounts `VRS_SouthCentral_RTCM3`/`MS_RTCM3`),
   iPhone hotspot, 12° mask, GPS+GLO+GAL+BDS.

## Open

7. ❓ **Same Lite or second Lite?** Is the simpleRTK2B Lite + antenna moving
   over from the RTK-Feather rig (cannibalizing it), or is this a separate
   unit needing its own u-center pre-config pass? Docs are written to work
   either way until answered.
8. ❓→📄 **Power shape** — decide in `docs/hardware/power.md` (Phase 0):
   single bank port → Pynt USB with the Lite's 5 V tapped from the Pynt's
   5 V rail (needs verification the rail can source ~210 mA extra), vs.
   two-feed (bank port 2 → Lite 5V_IN directly, common ground). Total
   system ≈ 550–650 mA peak (SAMD51 + TFT backlight + AirLift WiFi bursts
   + F9P ~130 mA + active antenna ≤75 mA).
9. 📄 **Pynt D3/D4 label swap** — forum reports say the Pynt's D3/D4
   sockets are silkscreened swapped vs. the classic PyPortal. Verify by
   loopback/scope during Phase 1 bring-up before wiring the Lite.
10. 📄 **D3/D4 SERCOM mapping in Arduino** — `busio.UART(board.D3, board.D4)`
    works in CircuitPython, so the pads are UART-capable; confirm which
    SERCOM/pads the Adafruit SAMD core variant assigns and whether a custom
    `Uart` instance + `pinPeripheral()` is needed. Max reliable baud at
    115200 to be verified in Phase 1.
11. 📄 **TFT driver in 8-bit parallel mode** — PyPortal's ILI9341 is on an
    8-bit parallel bus (SPI only via solder-jumper surgery, not planned).
    Confirm Adafruit_ILI9341's `tft8bitbus` constructor path + pin list for
    the Pynt variant, or fall back to Adafruit_Arcada.
12. 📄 **SPI contention budget** — AirLift (NTRIP + TCP server) and microSD
    (RAWX writes) share the SPI bus. Port the Feather's two-stage-buffering
    stall analysis; measure worst-case SD write latency with WiFi active in
    Phase 1.
13. ❓ **Cable stock** — JST-GH 6-pin (Pixhawk) pigtail on hand for the
    Lite? And 3-pin JST PH pigtails for the Pynt's D3/D4 sockets, or
    solder to pads?
14. ❓ **microSD card** — brand/size/speed class (drives SD latency
    assumptions, same as Feather Q-B).
15. ❓ **GNSS antenna** — which active antenna, ground plane available?
    (Feather Q-C; inherited unanswered.)
16. ❓ **Enclosure / assembly** — the Pynt is a display-first board; any
    mounting/enclosure plan affects connector strain relief and the
    touch-UI layout (portrait vs landscape).
17. ❓ **Does the RTK-Feather rig stay in service?** If yes (e.g. as a
    dedicated base), this build's Base mode could pair with it — worth a
    docs note on the RTCM3 hand-off path once the radio question matures.
