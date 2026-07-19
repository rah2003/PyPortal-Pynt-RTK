# Full-System Wiring — Pynt ↔ simpleRTK2B Lite

Only **four conductors** connect the two boards (power, TX, RX, ground) —
the Feather build's parallel tap and status link are gone; the Pynt does
everything on one MCU.

**Inherited vendor facts (verified for the Feather build, unchanged):**

- The Lite has **no native USB** — its XBee-to-USB adapter is an FTDI
  bridge onto **UART1**, the same F9P port the Pynt uses. The Pixhawk JST,
  bottom XBee header, and "USB/u-center" are all one port
  ([hookup guide](https://www.ardusimple.com/simplertk2blite-hookup-guide/),
  [ArduSimple Q&A](https://www.ardusimple.com/question/xbee-to-usb-adapter-connection-to-simplertk2blite/)).
- JST-GH pinout, from the F9P's perspective: **1 = 5V_IN, 2 = UART1 RX
  (F9P input), 3 = UART1 TX (F9P output), 4/5 = NC, 6 = GND**. Pin 1 is
  the white square / printed "1" on the PCB; **never trust pigtail wire
  colors** ([Pixhawk cable notes](https://www.ardusimple.com/product/pixhawk-cable-set/)).
- No I2C pads on the Lite — UART1 is the only MCU data path.

## System diagram

```mermaid
graph LR
  subgraph LITE["simpleRTK2B Lite (ZED-F9P)"]
    JST["Pixhawk JST-GH<br/>1:5V_IN 2:U1RX 3:U1TX 6:GND"]
    XBTOP["Top XBee socket = UART2<br/>EMPTY — reserved for future radio"]
    SMA["SMA → HC977 helical<br/>(3.3V bias, ~21 mA)"]
  end
  subgraph PYNT["PyPortal Pynt (SAMD51)"]
    D3["D3 JST — tape-labeled RX<br/>signal (PA04), SERCOM0 pad 0<br/>GND + VCC pins unused"]
    D4["D4 JST — tape-labeled TX<br/>signal (PA05), SERCOM0 pad 1<br/>GND + VCC pins carry power"]
    USB["micro-USB ← battery bank"]
  end

  JST -- "NMEA + UBX (RAWX/SFRBX) → 3.3V" --> D3
  D4 -- "RTCM3 + UBX cfg → 3.3V" --> JST
  D4 -- "5V (VCC pin)" --> JST
  D4 -- "GND pin" --- JST
```

**I2C STEMMA port is left fully unpopulated** — this project has no I2C
use, and D3/D4's own VCC/GND pins measured the same 5V as the STEMMA
port's (bring-up P1/P2, `docs/hardware/bringup-log.md`), so there's no
reason to route a third connector.

## Wire list (every conductor — sourced from just the D3 and D4 connectors)

| # | From | To | Signal | Level |
|---|---|---|---|---|
| 1 | Pynt **D4 JST VCC pin** (pin 2) | Lite JST **pin 1** (5V_IN) | power | 5 V, measured (Lite accepts 4.5–5.5 V) |
| 2 | Pynt **D4 JST GND pin** (pin 1) | Lite JST **pin 6** | ground | 0 V |
| 3 | Lite JST **pin 3** (F9P UART1 **TX**) | Pynt **D3 JST signal pin** (PA04, tape-labeled RX) | NMEA + UBX (RAWX/SFRBX) | 3.3 V |
| 4 | Pynt **D4 JST signal pin** (PA05, tape-labeled TX) | Lite JST **pin 2** (F9P UART1 **RX**) | RTCM3 corrections + UBX config | 3.3 V |

Wires 1 and 2 both come off the D4 connector alongside its signal wire —
three wires off one JST pigtail. D3's own GND/VCC pins go unused; only
its signal pin is wired. This replaces the earlier I2C-STEMMA-for-power
plan in `power.md` shape A now that D3/D4 VCC is confirmed 5V on
hardware — see `power.md` for the updated diagram.

Signal-integrity note: the Pynt's D3/D4 lines each pass through a **1 kΩ
series protection resistor + 3.6 V zener**
([pinouts](https://learn.adafruit.com/adafruit-pyportal/pinouts)). At
115200 baud (bit time 8.7 µs) into the F9P's high-impedance CMOS input the
RC formed with pin capacitance is nanoseconds — no effect. Don't raise the
baud past 230400 without re-checking edges on a scope.

## ⚠ D3/D4 label-swap check (do this before wiring the Lite)

Forum reports say the **Pynt's D3/D4 silkscreen is swapped** vs. the
classic PyPortal ([Adafruit forums](https://forums.adafruit.com/viewtopic.php?t=168933)).
Procedure (Phase 1 bring-up sketch has this test):

1. Jumper the two JST **signal** pins together (D3 socket ↔ D4 socket).
2. Run the UART loopback test: firmware TXs a known pattern on the
   SERCOM0 UART and checks RX echo.
3. If loopback passes, remove the jumper, then run the "which socket is
   TX?" probe: firmware idles TX high and pulses it once per second;
   a multimeter/scope on each socket identifies the physical TX socket.
4. **Label both sockets with tape** per the measurement. Only then wire
   the Lite (its RX ← our measured TX).

Wrong guess consequence: harmless (both ends are 3.3 V UARTs and the
Pynt's pins are series-protected) — it just won't talk. But finding that
out *after* the enclosure is built is why we tape-label now.

## UART recipe (Arduino / Adafruit SAMD core)

Arduino pins 3/4 = PA04/PA05, `PIO_SERCOM_ALT` capable
([variant.cpp](https://github.com/adafruit/ArduinoCore-samd/blob/master/variants/pyportal_m4/variant.cpp)).
PA04 = SERCOM0/PAD[0] (ALT), PA05 = SERCOM0/PAD[1] (ALT); SAMD51 UART TX
must sit on pad 0 → **firmware pin 3 = TX, firmware pin 4 = RX** (this is
the `GNSS_TX_PIN`/`GNSS_RX_PIN` numbering in `pins.h`, unchanged by the
label-swap finding below). What *does* change is which physical socket
that firmware pin comes out on: the swap check found firmware pin 3
(TX) is physically the socket silkscreened **D4**, and firmware pin 4
(RX) is silkscreened **D3** — hence the tape labels used everywhere else
in this doc:

```cpp
Uart SerialGNSS(&sercom0, /*RX*/ 4, /*TX*/ 3,
                SERCOM_RX_PAD_1, UART_TX_PAD_0);
// SAMD51 SERCOMs have four IRQ lines — all must be forwarded:
void SERCOM0_0_Handler() { SerialGNSS.IrqHandler(); }
void SERCOM0_1_Handler() { SerialGNSS.IrqHandler(); }
void SERCOM0_2_Handler() { SerialGNSS.IrqHandler(); }
void SERCOM0_3_Handler() { SerialGNSS.IrqHandler(); }
// after SerialGNSS.begin(115200):
pinPeripheral(3, PIO_SERCOM_ALT);
pinPeripheral(4, PIO_SERCOM_ALT);
```

SERCOM budget (from
[variant.cpp](https://github.com/adafruit/ArduinoCore-samd/blob/master/variants/pyportal_m4/variant.cpp)):
SERCOM2 = SPI (AirLift + microSD), SERCOM4 = `Serial1`/`SerialNina`
(ESP32 boot/BLE serial — **do not repurpose**), SERCOM5 = Wire (I2C
STEMMA). **Free: SERCOM0 (ours), SERCOM1, SERCOM3.** Note SERCOM1's pads
(PA16–PA19) are physically consumed by the TFT's 8-bit data bus, so
SERCOM3 is the only real spare. Pad math is from the SAMD51 datasheet mux
table — the Phase 1 loopback is the ground-truth verification.

## NEVER-connect list (one driver per line)

1. **XBee-to-USB adapter + JST pigtail simultaneously.** The adapter's
   FTDI is a second driver on F9P UART1 RX. u-center sessions happen with
   the pigtail unplugged (or wire 2 detached). Multiple *power* sources
   are sanctioned by ArduSimple; the prohibition is **data contention**.
2. **Top XBee socket stays empty** — UART2 is the future radio's. Its
   VCC pin is a 3.3 V/250 mA *output*; never feed power in.
3. **Do not power the Lite from a Pynt 3.3 V pin.** The Lite regulates
   its own 3.3 V from 5V_IN; the Pynt's 3.3 V rail already carries the
   SAMD51 + TFT + AirLift (see `power.md`).
4. ~~D3/D4 JST power pins: connect nothing until measured~~ — **measured
   2026-07-17: both read 5V** (`power.md` P1, confirmed in
   `bringup-log.md`). D4's VCC/GND pins are now the chosen power/ground
   source for the Lite; see the wire list above.
