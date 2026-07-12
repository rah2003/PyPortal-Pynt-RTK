// PyPortal-Pynt-RTK Rover — main entry. Single-core superloop (SAMD51):
// what the Feather build split across two MCUs and four FreeRTOS tasks
// runs here as six polled modules in one loop. Boot order matters:
// SD first (settings live on the card), then settings, then everything
// that consumes them.
#include <Arduino.h>

#include "pins.h"

#include "features.h"
#include "gnss.h"
#include "ntrip.h"
#include "sd_logger.h"
#include "serial_menu.h"
#include "settings.h"
#include "shared.h"
#include "tcp_nmea.h"
#include "ui.h"

namespace {
uint32_t loopWorstUs = 0;  // loop-time high-water; 'status' shows health
uint32_t lastLoopReportMs = 0;
}  // namespace

void setup() {
  Serial.begin(115200);
  uint32_t t0 = millis();
  while (!Serial && millis() - t0 < 3000) delay(10);

  Serial.println();
  Serial.println(F("=== PyPortal-Pynt-RTK Rover ==="));
  Serial.println(F("build: " __DATE__ " " __TIME__));

  // The AirLift shares the SPI bus with the SD card, and its chip-select
  // is a floating input until WiFi.setPins() runs (inside ntripInit,
  // AFTER the SD comes up because settings live on the card). Deselect
  // it explicitly so the NINA can't drive MISO during SD init.
  pinMode(SPIWIFI_SS, OUTPUT);
  digitalWrite(SPIWIFI_SS, HIGH);

  uiInit();          // screen up first: boot progress is visible in the field
  sdLoggerInit();    // before settingsLoad — /config.txt lives on the card
  settingsLoad();
  settingsPrint(Serial);
  sdLoggerSetEnabled(g_settings.logUbx);  // now that the config is read

  if (!gnssInit())
    Serial.println(F(
        "[main] no F9P — check wiring.md wires 2/3 (and the D3/D4 label swap)"));

  ntripInit();
  tcpNmeaInit();

  Serial.println(F("[main] running — 'help' for the serial menu"));
}

void loop() {
  uint32_t t0 = micros();

  gnssPoll();      // parse UBX/NMEA, fire PVT callback, fill file buffer
  ntripPoll();     // WiFi + caster; injects RTCM into the F9P
  tcpNmeaPoll();   // SW Maps NMEA broadcast
  sdLoggerPoll();  // drain file buffer -> .ubx (one bounded chunk/pass)
  uiPoll();        // touch + 2 Hz redraw
  serialMenuPoll();

  uint32_t dt = micros() - t0;
  if (dt > loopWorstUs) loopWorstUs = dt;
  if (millis() - lastLoopReportMs >= 60000) {  // once a minute to console
    lastLoopReportMs = millis();
    Serial.print(F("[main] worst loop "));
    Serial.print(loopWorstUs / 1000.0f, 1);
    Serial.println(F(" ms (resets each report)"));
    loopWorstUs = 0;
  }
}
