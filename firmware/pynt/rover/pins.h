// Pynt pin map — identical to the bring-up sketch's pins.h; keep the two
// in sync (they are separate build environments, not shared source).
// Sources: TFT pins from Adafruit's graphicstest_pyportal.ino; touch/SD/
// AirLift from the pyportal_m4 variant; GNSS UART per docs/hardware/wiring.md.
#pragma once

#include <Arduino.h>

// ---- TFT: ILI9341, 8-bit parallel ----
#define TFT_D0        34
#define TFT_WR        26
#define TFT_DC        10
#define TFT_CS        11
#define TFT_RST       24
#define TFT_RD         9
#define TFT_BACKLIGHT 25

// ---- Resistive touch (raw ADC; calibration in ui.cpp) ----
// Pin numbers per Adafruit's official "Adafruit PyPortal Pynt Pinout.pdf"
// (github.com/adafruit/Adafruit-PyPortal-PCB) — previously off by one
// across all four pins, found via the bring-up sketch's 'y' raw-pin test
// (2026-07-17). Keep in sync with ../bringup/pins.h.
#define TOUCH_YD 18
#define TOUCH_XL 19
#define TOUCH_YU 20
#define TOUCH_XR 21

// ---- microSD (shared SPI with the AirLift) ----
#define SD_CS 32

// ---- ESP32 AirLift ----
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

// ---- GNSS UART: F9P UART1 on SERCOM0, TX=D3(PA04,p0) RX=D4(PA05,p1) ----
#define GNSS_TX_PIN 3
#define GNSS_RX_PIN 4
