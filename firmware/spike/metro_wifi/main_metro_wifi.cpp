// Spike A — upstream-stack WiFi smoke test (Metro M4 AirLift Lite).
//
// Proves the NEW host stack (upstream WiFiNINA 2.x + vendored
// Arduino_SpiNINA) talks to the NEW module firmware (vendored
// arduino/nina-fw 3.0.1 + MOSI 12->14 AirLift patch) before any BLE is
// involved. No setPins() exists in this stack — Arduino_SpiNINA takes
// SPIWIFI_SS/ACK/RESET and NINA_GPIO0 from the metro_m4_airlift variant
// (36/37/38/35), verified in the Adafruit SAMD core 1.7.16.
//
// PASS (docs/coex-bench.md section 4):
//   - "NINA firmware: 3.0.1" (stock Adafruit prints 1.7.x)
//   - scan lists the hotspot SSID
//   - join succeeds, IP + RSSI print and keep refreshing
#include <Arduino.h>
#include <SPI.h>
#include <WiFiNINA.h>

#if __has_include("../secrets.h")
#include "../secrets.h"
#else  // compiles without credentials, same pattern as the rover
#define SPIKE_WIFI_SSID "set-me: firmware/spike/secrets.h"
#define SPIKE_WIFI_PASS ""
#endif

namespace {
uint32_t lastStatusMs = 0;
}

void setup() {
  Serial.begin(115200);
  uint32_t t0 = millis();
  while (!Serial && millis() - t0 < 3000) delay(10);

  Serial.println();
  Serial.println(F("=== Spike A: Metro M4 AirLift, upstream WiFiNINA 2.x ==="));
  Serial.println(F("build: " __DATE__ " " __TIME__));

  if (WiFi.status() == WL_NO_MODULE) {
    Serial.println(F("[FAIL] AirLift not responding over SPI."));
    Serial.println(F("       Wrong/old nina-fw, or MOSI patch missing (12 vs 14)."));
    while (true) delay(1000);
  }

  Serial.print(F("NINA firmware: "));
  Serial.println(WiFi.firmwareVersion());  // want exactly "3.0.1"

  Serial.println(F("Scanning..."));
  int8_t n = WiFi.scanNetworks();
  for (int8_t i = 0; i < n; i++) {
    Serial.print(F("  "));
    Serial.print(WiFi.SSID(i));
    Serial.print(F("  "));
    Serial.print(WiFi.RSSI(i));
    Serial.println(F(" dBm"));
  }

  Serial.print(F("Joining "));
  Serial.println(F(SPIKE_WIFI_SSID));
  if (WiFi.begin(SPIKE_WIFI_SSID, SPIKE_WIFI_PASS) != WL_CONNECTED) {
    Serial.println(F("[FAIL] join failed — check secrets.h / hotspot visible above"));
    while (true) delay(1000);
  }
  Serial.print(F("[PASS] up, IP "));
  Serial.println(WiFi.localIP());
}

void loop() {
  if (millis() - lastStatusMs >= 5000) {
    lastStatusMs = millis();
    Serial.print(F("status="));
    Serial.print(WiFi.status() == WL_CONNECTED ? F("connected") : F("DROPPED"));
    Serial.print(F("  rssi="));
    Serial.print(WiFi.RSSI());
    Serial.print(F("  uptime="));
    Serial.print(millis() / 1000);
    Serial.println(F("s"));
  }
}
