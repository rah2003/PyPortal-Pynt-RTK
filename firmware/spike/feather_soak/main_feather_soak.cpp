// Spike C — shared-SPI contention soak (Feather M0 Adalogger + AirLift
// FeatherWing). The Pynt's riskiest structural fact is the AirLift and
// microSD sharing one SPI bus; this rig reproduces it on spare hardware
// with BLE traffic added, before the Pynt itself is ever reflashed.
//
// Pins come from build_flags (feather_m0 variant defines no AirLift
// macros): SPIWIFI_SS=13, SPIWIFI_ACK=11, SPIWIFI_RESET=12, NINA_GPIO0=10.
// HARDWARE PREREQ: the FeatherWing's GPIO0 solder jumper MUST be closed —
// upstream WiFiNINA 2.x polls GPIO0 as the module's data-ready IRQ
// (SpiDrv::available() in Arduino_SpiNINA), it is not optional the way it
// was with the Adafruit 1.x stack. Adalogger SD CS is pin 4.
//
// PASS (docs/coex-bench.md section 6): >= 60 min with SD writes + WiFi
// TCP stream + BLE notify all running: uptime never restarts, sdErrors=0,
// worst SD write latency recorded, BLE stays subscribable, WiFi drops
// recover on their own.
#include <Arduino.h>
#include <SPI.h>
#include <ArduinoBLE.h>
#include <SdFat.h>
#include <WiFiNINA.h>

#if __has_include("../secrets.h")
#include "../secrets.h"
#else  // compiles without credentials, same pattern as the rover
#define SPIKE_WIFI_SSID "set-me: firmware/spike/secrets.h"
#define SPIKE_WIFI_PASS ""
#endif

#ifndef SOAK_SD_CS
#define SOAK_SD_CS 4  // Feather M0 Adalogger onboard microSD
#endif

namespace {

BLEService nus("6E400001-B5A3-F393-E0A9-E50E24DCCA9E");
BLECharacteristic nusTx("6E400003-B5A3-F393-E0A9-E50E24DCCA9E", BLENotify, 120);

WiFiServer server(10110);
WiFiClient client;  // one slot is enough for the soak

SdFat sd;
FsFile logFile;

uint8_t record[512];  // one Pynt-sized logger chunk per second
uint32_t writes = 0, sdErrors = 0, wifiDrops = 0;
uint32_t worstWriteUs = 0;
uint32_t lastTickMs = 0, lastReportMs = 0;
bool wifiWasUp = false;

void writeRecord() {
  // Stamp the record so a post-soak read can spot gaps/corruption.
  uint32_t t = millis();
  memcpy(record, &t, sizeof(t));
  memcpy(record + 4, &writes, sizeof(writes));

  uint32_t t0 = micros();
  bool ok = logFile && logFile.write(record, sizeof(record)) == sizeof(record);
  if (ok && (writes % 8 == 7)) ok = logFile.sync();  // rover's 8 s cadence
  uint32_t dt = micros() - t0;

  if (!ok) sdErrors++;
  if (dt > worstWriteUs) worstWriteUs = dt;
  writes++;
}

}  // namespace

void setup() {
  Serial.begin(115200);
  uint32_t t0 = millis();
  while (!Serial && millis() - t0 < 3000) delay(10);

  Serial.println();
  Serial.println(F("=== Spike C: Feather soak — SD + WiFi + BLE on shared SPI ==="));
  Serial.println(F("build: " __DATE__ " " __TIME__));

  // Same boot-order rule as the Pynt rover: park the AirLift CS before
  // SD init so the NINA can't drive MISO during card bring-up.
  pinMode(SPIWIFI_SS, OUTPUT);
  digitalWrite(SPIWIFI_SS, HIGH);

  if (!sd.begin(SOAK_SD_CS, SD_SCK_MHZ(12))) {
    Serial.println(F("[FAIL] SD init — card present? FAT32?"));
    while (true) delay(1000);
  }
  logFile = sd.open("soak.bin", O_WRITE | O_CREAT | O_TRUNC);
  if (!logFile) {
    Serial.println(F("[FAIL] soak.bin open"));
    while (true) delay(1000);
  }
  for (size_t i = 8; i < sizeof(record); i++) record[i] = (uint8_t)i;

  if (WiFi.status() == WL_NO_MODULE) {
    Serial.println(F("[FAIL] AirLift Wing not responding — GPIO0 jumper closed? nina-fw flashed?"));
    while (true) delay(1000);
  }
  Serial.print(F("NINA firmware: "));
  Serial.println(WiFi.firmwareVersion());

  Serial.print(F("Joining "));
  Serial.println(F(SPIKE_WIFI_SSID));
  if (WiFi.begin(SPIKE_WIFI_SSID, SPIKE_WIFI_PASS) == WL_CONNECTED) {
    Serial.print(F("WiFi up, IP "));
    Serial.println(WiFi.localIP());
    server.begin();
  } else {
    Serial.println(F("[warn] join failed — soak continues, loop retries"));
  }

  if (!BLE.begin()) {
    Serial.println(F("[FAIL] BLE.begin()"));
    while (true) delay(1000);
  }
  BLE.setLocalName("PyntRTK-soak");
  BLE.setAdvertisedService(nus);
  nus.addCharacteristic(nusTx);
  BLE.addService(nus);
  BLE.advertise();
  Serial.println(F("[ble] advertising as PyntRTK-soak — start the clock"));
}

void loop() {
  BLE.poll();

  if (WiFi.status() == WL_CONNECTED) {
    if (!wifiWasUp) server.begin();
    wifiWasUp = true;
    if (!client || !client.connected()) {
      WiFiClient incoming = server.available();
      if (incoming) client = incoming;
    } else {
      while (client.available()) client.read();  // drain
    }
  } else if (wifiWasUp) {
    wifiWasUp = false;
    wifiDrops++;
    WiFi.begin(SPIKE_WIFI_SSID, SPIKE_WIFI_PASS);
  }

  if (millis() - lastTickMs >= 1000) {
    lastTickMs = millis();
    writeRecord();

    char line[120];
    int n = snprintf(line, sizeof(line),
                     "SOAK up=%lus w=%lu err=%lu worst=%lums wifi=%d drops=%lu ble=%d\r\n",
                     (unsigned long)(millis() / 1000), (unsigned long)writes,
                     (unsigned long)sdErrors,
                     (unsigned long)(worstWriteUs / 1000),
                     WiFi.status() == WL_CONNECTED, (unsigned long)wifiDrops,
                     BLE.connected() ? 1 : 0);
    if (n > 0) {
      if (BLE.connected()) nusTx.writeValue((const uint8_t*)line, (unsigned)n);
      if (client && client.connected()) client.write((const uint8_t*)line, n);
    }
  }

  if (millis() - lastReportMs >= 30000) {
    lastReportMs = millis();
    Serial.print(F("[soak] writes="));
    Serial.print(writes);
    Serial.print(F(" sdErrors="));
    Serial.print(sdErrors);
    Serial.print(F(" worstWrite="));
    Serial.print(worstWriteUs / 1000.0f, 1);
    Serial.print(F("ms wifiDrops="));
    Serial.print(wifiDrops);
    Serial.print(F(" ble="));
    Serial.println(BLE.connected() ? F("connected") : F("advertising"));
  }
}
