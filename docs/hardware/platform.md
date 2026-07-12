# Pynt Platform Notes — buses, contention, display, touch

## Bus map (who owns what)

| Bus | Peripherals | Notes |
|---|---|---|
| SERCOM0 UART (D3/D4) | ZED-F9P UART1 | ours; recipe in `wiring.md` |
| SERCOM2 SPI | **ESP32 AirLift + microSD** (separate CS) | the contention point — see below |
| SERCOM4 UART (`Serial1`/`SerialNina`) | ESP32 bootstrap/BLE serial | leave alone |
| SERCOM5 I2C (STEMMA) | ADT7410 temp sensor onboard; port unused by us (5 V pin doubles as the Lite's power source, `power.md` shape A) | free |
| 8-bit parallel (PA16–23 + ctrl) | ILI9341 TFT | dedicated — display traffic never touches SPI |
| Analog | resistive touch YD/XL/YU/XR = pins 17/18/19/20 | 4-wire, polled ADC |
| QSPI | 8 MB flash | available for config/assets if ever needed |

The TFT being on its own parallel bus is the Pynt's structural advantage:
UI refreshes cost CPU but zero SPI bandwidth.

## SPI contention analysis (AirLift × microSD)

Single superloop, one SPI transaction at a time — WiFiNINA socket reads
(NTRIP RTCM3 in, TCP NMEA out) and SdFat writes serialize naturally. The
risk is **latency, not corruption**: a slow SD write blocks NTRIP/TCP
servicing, and vice versa.

Budget, worst case:

- F9P UART1 output at 115200 ≈ 11.5 KB/s ceiling; actual RAWX + SFRBX +
  NMEA @ 1 Hz ≈ 1–2.5 KB/s in ~burst-per-epoch shape.
- The PNY Elite U1 card can stall a single write **100–250 ms**
  (class-U1 worst case; measured value comes from the Phase 1 card test).
- 250 ms stall at 11.5 KB/s ⇒ ≤ ~2.9 KB must buffer in the UART RX ring.

With 256 KB of RAM (vs. the Adalogger M0's 32 KB and ~6.7 KiB headroom),
we allocate a **32 KB UART RX ring buffer** (≈ 2.8 s at full line rate)
and a **32 KB SD staging buffer**, and the problem the Feather build had
to engineer around ceases to be tight. Two-stage buffering (UART ISR ring
→ staging → pre-allocated contiguous file, 512-byte aligned writes) is
still ported from Feather because it is the right shape, not because the
margins demand it.

NTRIP side of the same coin: RTCM3 in is ~0.5–1 KB/s; WiFiNINA's internal
socket buffer on the NINA side absorbs SD-write windows. Correction age
tolerance is seconds — a 250 ms hiccup is invisible.

## Display: ILI9341 over 8-bit parallel

- Bus: 8 data lines (Arduino 34–41 = PA16–23) + control (5–12, 24–26
  region; exact defines from
  [variant.cpp](https://github.com/adafruit/ArduinoCore-samd/blob/master/variants/pyportal_m4/variant.cpp)).
  SPI mode exists only via solder-jumper surgery — not planned.
- Driver: `Adafruit_ILI9341` has a parallel constructor
  (`tft8bitbus`, used by Adafruit's own PyPortal Arduino demos); fallback
  is `Adafruit_Arcada`, which knows the PyPortal variant wholesale.
  **Verify at first compile** — this is Q11's remaining tail.
- **Portrait 240×320** (owner decision, 2026-07-11): `setRotation(0)` or
  `setRotation(2)` — pick when cable-exit direction is known (enclosure
  comes after electronics). All UI layout targets 240 (w) × 320 (h).

## Touch: 4-wire resistive

- Pins: TOUCH_YD=17 (PB00), TOUCH_XL=18 (PB01), TOUCH_YU=19 (PA06),
  TOUCH_XR=20 (PB08) — plain ADC polling via `Adafruit_TouchScreen`.
- PA06 is also SERCOM0/PAD[2]; our UART uses only pads 0/1 — **no
  conflict** (noted so nobody "optimizes" the UART onto pads 2/3 later).
- Resistive + gloves = works (field-relevant); calibration constants and
  a debounce/hysteresis pass are Phase 2 UI work. Touch targets sized
  ≥ 48 px for field use.

## CPU/RAM budget sanity

120 MHz M4F, 256 KB RAM, 1 MB flash. Feather's HUZZAH32 rover build used
17 % RAM / 66 % flash on a 520 KB-RAM ESP32 doing strictly more radio
work (native WiFi stack in-MCU; ours lives on the NINA co-processor).
The Pynt adds the TFT framebuffer-less GFX path (no full framebuffer —
ILI9341 is drawn-through) and WiFiNINA's modest footprint. No red flags;
the Phase 1 bring-up sketch prints free-RAM high-water marks to confirm.
