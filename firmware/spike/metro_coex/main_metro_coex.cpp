// Spike B — SIMULTANEOUS WiFi + BLE on the AirLift (Metro M4 AirLift Lite).
//
// The whole point of the branch: nina-fw 3.0.1 multiplexes BLE HCI onto
// the same SPI command server WiFi uses, so upstream WiFiNINA 2.x and
// ArduinoBLE 2.x share one bus/one module with no mode switch. BLE here
// is a Nordic-UART-style (NUS) notify stream — deliberately the same
// service SW Maps speaks, so this spike doubles as the prototype for the
// eventual phone-over-BLE path on the Pynt.
//
// The vendored lib/ArduinoBLE carries a 2-line patch: its SPI-HCI
// transport is gated to four Arduino boards upstream; -DBLE_NINA_SPI_TRANSPORT
// (set in platformio.ini) opts Adafruit AirLift boards in.
//
// PASS (docs/coex-bench.md section 5):
//   - WiFi joins, TCP server answers on :10110 (nc/telnet from a laptop
//     on the same hotspot; it streams the 1 Hz status line and echoes)
//   - phone (nRF Connect / LightBlue) sees "PyntRTK-coex", subscribing to
//     the NUS TX characteristic streams the same 1 Hz line
//   - BOTH stay live together >= 15 min, no watchdog resets (uptime
//     counter never restarts), WiFi drops = 0 or recovers alone
#include <Arduino.h>
#include <SPI.h>
#include <ArduinoBLE.h>
#include <WiFiNINA.h>

#if __has_include("../secrets.h")
#include "../secrets.h"
#else  // compiles without credentials, same pattern as the rover
#define SPIKE_WIFI_SSID "set-me: firmware/spike/secrets.h"
#define SPIKE_WIFI_PASS ""
#endif

namespace {

// Nordic UART Service, as used by SW Maps' BLE GNSS input.
BLEService nus("6E400001-B5A3-F393-E0A9-E50E24DCCA9E");
BLECharacteristic nusTx("6E400003-B5A3-F393-E0A9-E50E24DCCA9E", BLENotify, 120);
BLECharacteristic nusRx("6E400002-B5A3-F393-E0A9-E50E24DCCA9E",
                        BLEWrite | BLEWriteWithoutResponse, 120);

WiFiServer server(10110);
constexpr uint8_t kMaxClients = 2;
WiFiClient clients[kMaxClients];

uint32_t lastTickMs = 0;
uint32_t wifiDrops = 0;
bool wifiWasUp = false;

void acceptClients() {
  WiFiClient incoming = server.available();
  if (!incoming) return;
  for (uint8_t i = 0; i < kMaxClients; i++)
    if (clients[i] && clients[i] == incoming) return;
  for (uint8_t i = 0; i < kMaxClients; i++) {
    if (!clients[i] || !clients[i].connected()) {
      clients[i] = incoming;
      Serial.println(F("[tcp] client connected"));
      return;
    }
  }
  incoming.stop();
}

}  // namespace

void setup() {
  Serial.begin(115200);
  uint32_t t0 = millis();
  while (!Serial && millis() - t0 < 3000) delay(10);

  Serial.println();
  Serial.println(F("=== Spike B: Metro M4 AirLift, WiFi + BLE together ==="));
  Serial.println(F("build: " __DATE__ " " __TIME__));

  if (WiFi.status() == WL_NO_MODULE) {
    Serial.println(F("[FAIL] AirLift not responding over SPI"));
    while (true) delay(1000);
  }
  Serial.print(F("NINA firmware: "));
  Serial.println(WiFi.firmwareVersion());

  Serial.print(F("Joining "));
  Serial.println(F(SPIKE_WIFI_SSID));
  if (WiFi.begin(SPIKE_WIFI_SSID, SPIKE_WIFI_PASS) != WL_CONNECTED) {
    Serial.println(F("[warn] join failed — continuing so BLE can still be checked"));
  } else {
    Serial.print(F("WiFi up, IP "));
    Serial.println(WiFi.localIP());
    server.begin();
    Serial.println(F("[tcp] server on :10110"));
  }

  // BLE second, on the already-initialized SPI link. BLE_BEGIN is just
  // another command to the NINA's command server.
  if (!BLE.begin()) {
    Serial.println(F("[FAIL] BLE.begin() — is nina-fw really 3.0.x?"));
    while (true) delay(1000);
  }
  BLE.setLocalName("PyntRTK-coex");
  BLE.setAdvertisedService(nus);
  nus.addCharacteristic(nusTx);
  nus.addCharacteristic(nusRx);
  BLE.addService(nus);
  BLE.advertise();
  Serial.println(F("[ble] advertising as PyntRTK-coex"));
}

void loop() {
  BLE.poll();

  if (WiFi.status() == WL_CONNECTED) {
    if (!wifiWasUp) server.begin();  // (re)bind after join
    wifiWasUp = true;
    acceptClients();
    for (uint8_t i = 0; i < kMaxClients; i++) {
      if (!clients[i]) continue;
      if (!clients[i].connected()) {
        clients[i].stop();
        clients[i] = WiFiClient();
        continue;
      }
      while (clients[i].available()) {  // echo, proves two-way TCP under coex
        uint8_t b = clients[i].read();
        clients[i].write(&b, 1);
      }
    }
  } else if (wifiWasUp) {
    wifiWasUp = false;
    wifiDrops++;
    Serial.println(F("[wifi] dropped — retrying"));
    WiFi.begin(SPIKE_WIFI_SSID, SPIKE_WIFI_PASS);  // bounded blocking retry
  }

  if (millis() - lastTickMs >= 1000) {
    lastTickMs = millis();
    char line[120];
    int n = snprintf(line, sizeof(line),
                     "COEX up=%lus wifi=%d rssi=%d drops=%lu ble_conn=%d\r\n",
                     (unsigned long)(millis() / 1000),
                     WiFi.status() == WL_CONNECTED, (int)WiFi.RSSI(),
                     (unsigned long)wifiDrops, BLE.connected() ? 1 : 0);
    if (n > 0) {
      if (BLE.connected()) nusTx.writeValue((const uint8_t*)line, (unsigned)n);
      for (uint8_t i = 0; i < kMaxClients; i++)
        if (clients[i] && clients[i].connected())
          clients[i].write((const uint8_t*)line, n);
      Serial.print(line);
    }
  }
}
