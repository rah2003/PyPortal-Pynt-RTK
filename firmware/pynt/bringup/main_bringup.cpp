// PyPortal-Pynt-RTK — Phase 1 bring-up test suite
//
// Serial-menu bench tool covering every item in
// docs/hardware/checklists.md ("PyPortal Pynt" section). Each test prints
// PASS-looks-like guidance so the checklist can be worked top to bottom.
//
// Ancestry: structure follows RTK-Feather's per-board bring-up sketches;
// the GNSS UART recipe is docs/hardware/wiring.md's SERCOM0 mapping.

#include <Arduino.h>
#include <wiring_private.h>  // pinPeripheral()
#include <Adafruit_GFX.h>
#include <Adafruit_ILI9341.h>
#include <TouchScreen.h>
#include <WiFiNINA.h>
#include <SdFat.h>

#include "pins.h"
#if __has_include("secrets.h")
#include "secrets.h"
#define HAVE_SECRETS 1
#else
#define HAVE_SECRETS 0
#endif

// ---- GNSS UART on SERCOM0: TX=D3(PA04,pad0) RX=D4(PA05,pad1) ----
Uart SerialGNSS(&sercom0, GNSS_RX_PIN, GNSS_TX_PIN,
                SERCOM_RX_PAD_1, UART_TX_PAD_0);
void SERCOM0_0_Handler() { SerialGNSS.IrqHandler(); }
void SERCOM0_1_Handler() { SerialGNSS.IrqHandler(); }
void SERCOM0_2_Handler() { SerialGNSS.IrqHandler(); }
void SERCOM0_3_Handler() { SerialGNSS.IrqHandler(); }

static void gnssUartBegin() {
  SerialGNSS.begin(GNSS_BAUD);
  pinPeripheral(GNSS_TX_PIN, PIO_SERCOM_ALT);
  pinPeripheral(GNSS_RX_PIN, PIO_SERCOM_ALT);
}

Adafruit_ILI9341 tft(tft8bitbus, TFT_D0, TFT_WR, TFT_DC, TFT_CS,
                     TFT_RST, TFT_RD);
// Electrode polarity is a convention guess (pins.h) — 'p' test validates.
TouchScreen ts(TOUCH_XL, TOUCH_YU, TOUCH_XR, TOUCH_YD, 300);
SdFat sd;

static bool tftUp = false;

extern "C" char *sbrk(int);
static int freeRam() {
  char top;
  return &top - reinterpret_cast<char *>(sbrk(0));
}

static bool keyPressed() {  // drains and reports any pending USB input
  bool hit = false;
  while (Serial.available()) { Serial.read(); hit = true; }
  return hit;
}

// ---------------------------------------------------------------- tests

static void testTft() {
  Serial.println(F("[t] TFT: init + fills + both portrait rotations"));
  pinMode(TFT_BACKLIGHT, OUTPUT);
  digitalWrite(TFT_BACKLIGHT, HIGH);
  if (!tftUp) { tft.begin(); tftUp = true; }
  uint32_t t0 = millis();
  tft.fillScreen(ILI9341_RED);
  tft.fillScreen(ILI9341_GREEN);
  tft.fillScreen(ILI9341_BLUE);
  Serial.print(F("  three full fills took ")); Serial.print(millis() - t0);
  Serial.println(F(" ms (parallel bus datum for platform.md)"));
  static const uint8_t kPortraitRotations[2] = {0, 2};
  for (uint8_t r : kPortraitRotations) {
    tft.setRotation(r);   // the two portrait candidates (240x320)
    tft.fillScreen(ILI9341_BLACK);
    tft.setTextColor(ILI9341_WHITE);
    tft.setTextSize(3);
    tft.setCursor(10, 10);
    tft.print(F("rot "));
    tft.print(r);
    tft.setCursor(10, 50);
    tft.print(F("UP^"));
    Serial.print(F("  rotation ")); Serial.print(r);
    Serial.println(F(": note which physical edge 'UP^' points to"));
    delay(2500);
  }
  Serial.println(F("  PASS = clean fills, readable text, no tearing."));
  Serial.println(F("  Record the rotation whose UP matches the planned cable exit."));
}

static void testTouchDump() {
  Serial.println(F("[p] touch raw dump — press corners; any serial key exits"));
  while (!keyPressed()) {
    TSPoint p = ts.getPoint();
    if (p.z > 100) {  // crude pressure gate
      Serial.print(F("  x=")); Serial.print(p.x);
      Serial.print(F(" y=")); Serial.print(p.y);
      Serial.print(F(" z=")); Serial.println(p.z);
    }
    delay(50);
  }
  Serial.println(F("  Record 4-corner raw values in checklists.md."));
}

// Bypasses the TouchScreen library's math entirely. The library computes Y
// by driving YU/YD and sensing only through XR (pins.h TOUCH_XR) — if that
// single sense pin is bad, Y reads stuck regardless of the drive pins'
// health. This test drives each axis and reads BOTH opposite-axis
// electrodes at once, so a dead sense pin shows up next to a live one
// under the *same* drive condition — isolating sense-pin vs drive-pin
// faults instead of just reporting the library's already-broken output.
static void testTouchRawDump() {
  Serial.println(F("[y] touch raw pin diagnostic — press/drag around the panel; any serial key exits"));
  Serial.println(F("  Xdrive senses YU+YD (both should move together as you slide X)"));
  Serial.println(F("  Ydrive senses XL+XR (both should move together as you slide Y)"));
  Serial.println(F("  A pin stuck near 0 or 1023 while its neighbor moves = that pin/node is dead"));
  while (!keyPressed()) {
    // --- X-drive phase: drive XL/XR, sense YU and YD together ---
    pinMode(TOUCH_YU, INPUT);
    pinMode(TOUCH_YD, INPUT);
    pinMode(TOUCH_XL, OUTPUT);
    pinMode(TOUCH_XR, OUTPUT);
    digitalWrite(TOUCH_XL, HIGH);
    digitalWrite(TOUCH_XR, LOW);
    delayMicroseconds(20);
    int yu = analogRead(TOUCH_YU);
    int yd = analogRead(TOUCH_YD);

    // --- Y-drive phase: drive YU/YD, sense XL and XR together ---
    pinMode(TOUCH_XL, INPUT);
    pinMode(TOUCH_XR, INPUT);
    pinMode(TOUCH_YU, OUTPUT);
    pinMode(TOUCH_YD, OUTPUT);
    digitalWrite(TOUCH_YU, HIGH);
    digitalWrite(TOUCH_YD, LOW);
    delayMicroseconds(20);
    int xl = analogRead(TOUCH_XL);
    int xr = analogRead(TOUCH_XR);

    Serial.print(F("  Xdrive: YU=")); Serial.print(yu);
    Serial.print(F(" YD=")); Serial.print(yd);
    Serial.print(F("   |   Ydrive: XL=")); Serial.print(xl);
    Serial.print(F(" XR=")); Serial.println(xr);
    delay(200);
  }
}

static bool wifiPinsSet = false;
static void wifiEnsurePins() {
  if (!wifiPinsSet) {
    WiFi.setPins(SPIWIFI_SS, SPIWIFI_ACK, ESP32_RESETN, ESP32_GPIO0,
                 &SPIWIFI);
    wifiPinsSet = true;
  }
}

static void testWifiVersionScan() {
  Serial.println(F("[w] AirLift: firmware version + scan"));
  wifiEnsurePins();
  if (WiFi.status() == WL_NO_MODULE) {
    Serial.println(F("  FAIL: no module — check SPIWIFI pin set (pins.h)"));
    return;
  }
  Serial.print(F("  NINA firmware: "));
  Serial.println(WiFi.firmwareVersion());
  Serial.println(F("  (want >= 1.7.7 per checklists.md)"));
  int n = WiFi.scanNetworks();
  Serial.print(F("  networks: ")); Serial.println(n);
  for (int i = 0; i < n && i < 10; i++) {
    Serial.print(F("   ")); Serial.print(WiFi.SSID(i));
    Serial.print(F("  ")); Serial.print(WiFi.RSSI(i));
    Serial.println(F(" dBm"));
  }
}

static void testWifiJoinEcho() {
#if !HAVE_SECRETS
  Serial.println(F("[W] needs secrets.h (copy secrets.example.h)"));
#else
  Serial.println(F("[W] join + TCP echo server on :10110 (any key exits)"));
  wifiEnsurePins();
  Serial.print(F("  joining ")); Serial.println(WIFI_SSID);
  if (WiFi.begin(WIFI_SSID, WIFI_PASS) != WL_CONNECTED) {
    for (int i = 0; i < 20 && WiFi.status() != WL_CONNECTED; i++) delay(500);
  }
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println(F("  FAIL: no join"));
    return;
  }
  Serial.print(F("  IP: ")); Serial.println(WiFi.localIP());
  Serial.println(F("  from a laptop on the same hotspot: nc <ip> 10110"));
  WiFiServer server(10110);
  server.begin();
  while (!keyPressed()) {
    WiFiClient client = server.available();
    if (client && client.available()) {
      String line = client.readStringUntil('\n');
      client.print(F("echo: "));
      client.println(line);
      Serial.print(F("  echoed: ")); Serial.println(line);
    }
  }
  WiFi.disconnect();
  Serial.println(F("  done (proves the SW Maps TCP path)"));
#endif
}

static void testSdLatency() {
  Serial.println(F("[s] SD: init + 4 MB append test + latency histogram"));
  if (!sd.begin(SD_CS, SD_SCK_MHZ(12))) {
    Serial.println(F("  FAIL: init — card seated? FAT32?"));
    return;
  }
  sd.remove("bringup.bin");
  FsFile f = sd.open("bringup.bin", O_WRONLY | O_CREAT);
  if (!f) { Serial.println(F("  FAIL: open")); return; }
  static uint8_t buf[512];
  for (size_t i = 0; i < sizeof buf; i++) buf[i] = (uint8_t)i;
  // buckets (ms): <1, 1-5, 5-10, 10-50, 50-100, 100-250, >250
  uint32_t bucket[7] = {0};
  uint32_t worstUs = 0;
  const uint32_t kWrites = 8192;  // 4 MiB
  for (uint32_t i = 0; i < kWrites; i++) {
    uint32_t t0 = micros();
    if (f.write(buf, sizeof buf) != sizeof buf) {
      Serial.print(F("  FAIL: short write at ")); Serial.println(i);
      f.close();
      return;
    }
    uint32_t dt = micros() - t0;
    if (dt > worstUs) worstUs = dt;
    uint32_t ms = dt / 1000;
    bucket[ms < 1 ? 0 : ms < 5 ? 1 : ms < 10 ? 2 : ms < 50 ? 3
           : ms < 100 ? 4 : ms < 250 ? 5 : 6]++;
  }
  f.close();
  static const char *label[7] = {"<1ms", "1-5", "5-10", "10-50", "50-100",
                                 "100-250", ">250ms"};
  for (int i = 0; i < 7; i++) {
    Serial.print(F("  ")); Serial.print(label[i]);
    Serial.print(F(": ")); Serial.println(bucket[i]);
  }
  Serial.print(F("  worst single write: "));
  Serial.print(worstUs / 1000.0f, 1);
  Serial.println(F(" ms — feed this into platform.md's buffer math"));
}

static void testUartLoopback() {
  Serial.println(F("[l] UART loopback — jumper the two D3/D4 JST SIGNAL pins"));
  gnssUartBegin();
  while (SerialGNSS.available()) SerialGNSS.read();
  const char pattern[] = "PYNT-RTK-LOOPBACK-0123456789";
  SerialGNSS.print(pattern);
  SerialGNSS.flush();
  delay(50);
  char rx[sizeof pattern] = {0};
  size_t n = 0;
  while (SerialGNSS.available() && n < sizeof pattern - 1)
    rx[n++] = (char)SerialGNSS.read();
  if (n == sizeof pattern - 1 && memcmp(rx, pattern, n) == 0) {
    Serial.println(F("  PASS: pattern echoed intact at 115200"));
    Serial.println(F("  SERCOM0 recipe verified (QUESTIONS.md Q10 closed)"));
  } else {
    Serial.print(F("  FAIL: got ")); Serial.print(n);
    Serial.println(F(" bytes — jumper on? right sockets? (label swap!)"));
  }
}

static void testTxIdentify() {
  Serial.println(F("[x] TX identify: 1 Hz pulses on the TX line (any key exits)"));
  Serial.println(F("  probe each JST signal pin; the pulsing one is TX -> tape-label it"));
  gnssUartBegin();
  while (!keyPressed()) {
    SerialGNSS.write((uint8_t)0x55);  // brief edge burst
    SerialGNSS.flush();
    delay(1000);
  }
}

static void testNmeaEcho() {
  Serial.println(F("[n] NMEA echo from the F9P (wires 2+3 connected; any key exits)"));
  gnssUartBegin();
  uint32_t bytes = 0, lines = 0, t0 = millis();
  while (!keyPressed()) {
    while (SerialGNSS.available()) {
      char c = (char)SerialGNSS.read();
      Serial.write(c);
      bytes++;
      if (c == '\n') lines++;
    }
    if (millis() - t0 >= 5000) {
      Serial.print(F("\r\n  [")); Serial.print(bytes / 5);
      Serial.print(F(" B/s, ")); Serial.print(lines / 5);
      Serial.println(F(" lines/s]"));
      bytes = lines = 0;
      t0 = millis();
    }
  }
}

static void reportRam() {
  Serial.print(F("[r] free RAM: "));
  Serial.print(freeRam());
  Serial.println(F(" bytes (record the low-water mark after other tests)"));
}

static void menu() {
  Serial.println();
  Serial.println(F("== PyPortal-Pynt-RTK bring-up (docs/hardware/checklists.md) =="));
  Serial.println(F(" t  TFT fills + portrait rotations"));
  Serial.println(F(" p  touch raw dump"));
  Serial.println(F(" y  touch raw PIN diagnostic (isolates dead pin)"));
  Serial.println(F(" w  AirLift version + scan"));
  Serial.println(F(" W  WiFi join + TCP echo :10110 (needs secrets.h)"));
  Serial.println(F(" s  SD 4MB write + latency histogram"));
  Serial.println(F(" l  UART loopback (D3-D4 jumpered)"));
  Serial.println(F(" x  TX-socket identify pulses"));
  Serial.println(F(" n  live NMEA echo from F9P"));
  Serial.println(F(" r  free-RAM report"));
  Serial.println(F(" ?  this menu"));
}

void setup() {
  Serial.begin(115200);
  uint32_t t0 = millis();
  while (!Serial && millis() - t0 < 4000) {}
  menu();
}

void loop() {
  if (!Serial.available()) return;
  char c = (char)Serial.read();
  while (Serial.available()) Serial.read();  // drain CR/LF
  switch (c) {
    case 't': testTft(); break;
    case 'p': testTouchDump(); break;
    case 'y': testTouchRawDump(); break;
    case 'w': testWifiVersionScan(); break;
    case 'W': testWifiJoinEcho(); break;
    case 's': testSdLatency(); break;
    case 'l': testUartLoopback(); break;
    case 'x': testTxIdentify(); break;
    case 'n': testNmeaEcho(); break;
    case 'r': reportRam(); break;
    case '\r': case '\n': break;
    default: menu(); break;
  }
}
