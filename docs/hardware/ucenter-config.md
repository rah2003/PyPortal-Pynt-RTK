# F9P Re-Verification Checklist (u-center over the Lite's USB adapter)

This Lite **moves over from the RTK-Feather rig** — it may already carry
the correct flash config. Re-verify rather than assume: run this checklist
as VALGET spot-checks first; only VALSET what differs.

**Bench rule (read first):** the Lite's XBee-to-USB adapter is an FTDI
bridge to **UART1** — the same port the Pynt uses. Do this with the
**JST-GH pigtail unplugged**. Never both. See `wiring.md`.

Setup: Lite on the XBee-to-USB adapter → classic u-center. Connection
baud: **115200 if the Feather-era config took; 38400 (F9P default) if the
board was ever factory-reset.** Use View → Generation 9 Configuration View
(VALSET/VALGET); apply to RAM, verify, then RAM+BBR+Flash.

## 1. Identity / firmware

- [ ] UBX-MON-VER: record FW version (want HPG 1.32+). Should match the
      Feather project log — if it doesn't, this is a different board.

## 2. UART1 port

| Key | ID | Value |
|---|---|---|
| `CFG-UART1-BAUDRATE` | `0x40520001` | **115200** |
| `CFG-UART1INPROT-UBX` | `0x10730001` | 1 (config/poll from the Pynt) |
| `CFG-UART1INPROT-NMEA` | `0x10730002` | 0 |
| `CFG-UART1INPROT-RTCM3X` | `0x10730004` | **1** (corrections in) |
| `CFG-UART1OUTPROT-UBX` | `0x10740001` | 1 |
| `CFG-UART1OUTPROT-NMEA` | `0x10740002` | 1 |
| `CFG-UART1OUTPROT-RTCM3X` | `0x10740004` | 0 (Rover; Base uses UART2) |

## 3. Raw-data messages on UART1 (for the Pynt's SD logger)

| Key | ID | Value |
|---|---|---|
| `CFG-MSGOUT-UBX_RXM_RAWX_UART1` | `0x209102a5` | 1 |
| `CFG-MSGOUT-UBX_RXM_SFRBX_UART1` | `0x20910232` | 1 |
| `CFG-MSGOUT-UBX_TIM_TM2_UART1` | (look up in Config View) | 1 — optional |

## 4. NMEA set on UART1 (SW Maps TCP + GGA-to-caster)

- [ ] GGA = 1, RMC = 1, GSA = 1, GST = 1 (accuracy figures for SW Maps)
- [ ] GSV = 1
- [ ] VTG = 0 unless wanted
- [ ] `CFG-NMEA-HIGHPREC` = 1 (7-digit lat/lon minutes)

## 5. Rates, dynamics, constellations

- [ ] `CFG-RATE-MEAS` = 1000 ms, `CFG-RATE-NAV` = 1 → 1 Hz.
- [ ] `CFG-NAVSPG-DYNMODEL`: 0 (portable) default.
- [ ] Elevation mask `CFG-NAVSPG-INFIL_MINELEV` = **12°** (inherited
      Metro answer).
- [ ] Constellations: GPS + GLO + GAL + BDS enabled; SBAS/QZSS off
      (inherited Metro answer) — spot-check `CFG-SIGNAL-*` group.

## 6. Ports we deliberately leave alone

- [ ] **UART2: do not disable, do not reconfigure** — Base mode will later
      configure RTCM3 output here (config only; socket stays empty).
- [ ] I2C/USB blocks: leave at defaults.

## 7. Save and verify

- [ ] Apply full set to Flash (RAM+BBR+Flash, Send).
- [ ] Power-cycle; reconnect at 115200; VALGET spot-check from the
      **Flash** layer (RAWX_UART1, BAUDRATE).
- [ ] Packet console: RAWX + SFRBX + NMEA set at 1 Hz, no other chatter.
- [ ] **Unplug the USB adapter before connecting the JST pigtail.**

## Recovery

Mis-configured into silence → USB adapter, UBX-CFG-CFG "Revert to default
configuration", then redo this checklist (reconnect at 38400 after reset).
