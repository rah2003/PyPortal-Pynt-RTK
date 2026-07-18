// Pynt pin map — every number here traces to a cited source; the bring-up
// tests are the ground-truth verification (docs/hardware/checklists.md).
#pragma once

#include <Arduino.h>

// ---- TFT: ILI9341 on the 8-bit parallel bus ----
// Values from Adafruit's own graphicstest_pyportal.ino (Adafruit_ILI9341
// examples) — the reference for the tft8bitbus constructor.
#define TFT_D0        34   // first of 8 contiguous data pins (PA16-PA23)
#define TFT_WR        26
#define TFT_DC        10
#define TFT_CS        11
#define TFT_RST       24
#define TFT_RD         9
#define TFT_BACKLIGHT 25

// ---- Resistive touch (4-wire, raw ADC) ----
// Pin numbers from Adafruit's official "Adafruit PyPortal Pynt Pinout.pdf"
// (github.com/adafruit/Adafruit-PyPortal-PCB) — the pyportal_m4
// variant.cpp comments alone are misleading here; this repo previously had
// all four off by one (16/17/18/19-style shift), confirmed against the
// official diagram and the bring-up 'y' raw-pin test (2026-07-17: Y-axis
// gradient never formed because the "YD" pin wasn't actually the YD
// electrode). Electrode polarity (XP vs XM etc.) is still a convention
// guess — the 'p'/'y' raw-dump tests validate; if axes mirror, swap the
// pair and record it in checklists.md.
#define TOUCH_YD 18  // PB00
#define TOUCH_XL 19  // PB01
#define TOUCH_YU 20  // PA06 (also SERCOM0/PAD2 — unused by our UART)
#define TOUCH_XR 21  // PB08

// ---- microSD (shared SPI with the AirLift; SERCOM2) ----
#define SD_CS 32

// ---- ESP32 AirLift (WiFiNINA over the shared SPI) ----
// The pyportal_m4 variant defines these; #ifndef keeps us compatible if
// the macro names differ between core versions.
#ifndef SPIWIFI_SS
#define SPIWIFI_SS   8
#endif
#ifndef SPIWIFI_ACK
#define SPIWIFI_ACK  5
#endif
#ifndef ESP32_RESETN
#define ESP32_RESETN 7
#endif
#ifndef ESP32_GPIO0
#define ESP32_GPIO0  6
#endif
#define SPIWIFI SPI

// ---- GNSS UART: F9P UART1 on D3/D4 via SERCOM0 (docs/hardware/wiring.md) ----
// D3 = PA04 = SERCOM0/PAD0 (ALT) = TX (SAMD51 TX must be pad 0)
// D4 = PA05 = SERCOM0/PAD1 (ALT) = RX
// NOTE the physical-socket label swap risk (wiring.md) — pin numbers are
// firmware-side and correct regardless; only the *sockets* may be
// mislabeled on the silkscreen.
#define GNSS_TX_PIN 3
#define GNSS_RX_PIN 4
#define GNSS_BAUD   115200
