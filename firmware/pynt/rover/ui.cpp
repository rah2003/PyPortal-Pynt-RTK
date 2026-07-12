// Portrait touch UI. New module — the display concept (fix state up top,
// correction-age flash when stale, pages) follows the Feather's
// display.cpp, rebuilt for 240x320 color + touch buttons.
//
// Field ergonomics: buttons are 76x64 px (>= 48 px touch targets,
// platform.md), the header is color-coded by fix state so it reads at
// arm's length, and the correction-age line goes inverse-video when
// corrections are >10 s stale (Feather pattern).
//
// Touch calibration: provisional constants below; replace with the
// 4-corner raw values from the bring-up 'p' test (checklists.md item).
#include "ui.h"

#include <Adafruit_GFX.h>
#include <Adafruit_ILI9341.h>
#include <TouchScreen.h>

#include "ntrip.h"
#include "pins.h"
#include "sd_logger.h"
#include "settings.h"
#include "shared.h"

namespace {

// Which of the two portrait rotations — bench decision at bring-up
// (checklists.md TFT item); 0 and 2 are the candidates.
constexpr uint8_t kRotation = 0;

Adafruit_ILI9341 tft(tft8bitbus, TFT_D0, TFT_WR, TFT_DC, TFT_CS, TFT_RST,
                     TFT_RD);
TouchScreen ts(TOUCH_XL, TOUCH_YU, TOUCH_XR, TOUCH_YD, 300);

// Provisional raw-ADC calibration — REPLACE from the 'p' bring-up test.
constexpr int16_t kRawMin = 250, kRawMax = 3800;
constexpr int16_t kPressMin = 100, kPressMax = 4000;

constexpr int16_t W = 240, H = 320;
constexpr int16_t kHeaderH = 36;
constexpr int16_t kBtnY = 252, kBtnH = 64, kBtnW = 76, kBtnGap = 6;

// 565 colors
constexpr uint16_t C_BG = 0x0000, C_TEXT = 0xFFFF, C_DIM = 0x8410;
constexpr uint16_t C_RED = 0xF800, C_ORANGE = 0xFD20, C_BLUE = 0x2E5F,
                   C_GREEN = 0x07E0, C_BTN = 0x2104, C_BTNTXT = 0xFFFF;

uint8_t page = 0;  // 0 = POS, 1 = SYS
bool layoutDrawn = false;
uint32_t lastDrawMs = 0;
uint32_t lastTouchMs = 0;
uint32_t pwrConfirmMs = 0;  // two-tap shutdown confirm window
bool shutdownDone = false;
bool staleFlash = false;

struct Btn {
  int16_t x;
  const char* label;
};

void fixState(const char*& txt, uint16_t& color) {
  if (!g_gnss.f9pDetected) { txt = "NO GNSS"; color = C_RED; return; }
  if (g_gnss.carrSoln == 2) { txt = "RTK FIX"; color = C_GREEN; return; }
  if (g_gnss.carrSoln == 1) { txt = "FLOAT"; color = C_BLUE; return; }
  if (g_gnss.fixType >= 3) { txt = "3D"; color = C_ORANGE; return; }
  if (g_gnss.fixType == 2) { txt = "2D"; color = C_ORANGE; return; }
  txt = "NO FIX";
  color = C_RED;
}

void drawButtons() {
  const char* logLabel = g_log.fileOpen ? "LOG\xFE" : "LOG";  // 0xFE = solid block
  const char* pwrLabel = (pwrConfirmMs && millis() - pwrConfirmMs < 3000)
                             ? "SURE?"
                             : "PWR";
  const Btn btns[3] = {{4, logLabel}, {4 + kBtnW + kBtnGap, "PAGE"},
                       {4 + 2 * (kBtnW + kBtnGap), pwrLabel}};
  for (const Btn& b : btns) {
    tft.fillRoundRect(b.x, kBtnY, kBtnW, kBtnH, 8,
                      shutdownDone ? C_BG : C_BTN);
    tft.drawRoundRect(b.x, kBtnY, kBtnW, kBtnH, 8, C_DIM);
    tft.setTextColor(C_BTNTXT);
    tft.setTextSize(2);
    int16_t tw = strlen(b.label) * 12;
    tft.setCursor(b.x + (kBtnW - tw) / 2, kBtnY + kBtnH / 2 - 8);
    tft.print(b.label);
  }
}

void drawStaticLayout() {
  tft.fillScreen(C_BG);
  drawButtons();
  layoutDrawn = true;
}

// One labeled value line. y in body coords; blanks then reprints.
void line(int16_t y, const char* label, const char* value,
          bool invert = false) {
  tft.fillRect(0, y, W, 20, invert ? C_TEXT : C_BG);
  tft.setTextSize(2);
  tft.setTextColor(C_DIM, invert ? C_TEXT : C_BG);
  tft.setCursor(4, y + 2);
  tft.print(label);
  tft.setTextColor(invert ? C_BG : C_TEXT, invert ? C_TEXT : C_BG);
  tft.setCursor(4 + (int16_t)strlen(label) * 12, y + 2);
  tft.print(value);
}

void drawHeader() {
  const char* txt;
  uint16_t color;
  fixState(txt, color);
  tft.fillRect(0, 0, W, kHeaderH, color);
  tft.setTextSize(3);
  tft.setTextColor(C_BG);
  tft.setCursor(6, 7);
  tft.print(txt);
  char sv[12];
  snprintf(sv, sizeof(sv), "%uSV", g_gnss.numSV);
  tft.setCursor(W - 6 - (int16_t)strlen(sv) * 18, 7);
  tft.print(sv);
}

void drawPagePos() {
  char v[40];
  int16_t y = kHeaderH + 8;
  snprintf(v, sizeof(v), "%.7f", g_gnss.latDeg);
  line(y, "LAT ", v); y += 22;
  snprintf(v, sizeof(v), "%.7f", g_gnss.lonDeg);
  line(y, "LON ", v); y += 22;
  snprintf(v, sizeof(v), "%.1f m", g_gnss.hMslM);
  line(y, "ALT ", v); y += 22;
  snprintf(v, sizeof(v), "%lu.%lu cm", (unsigned long)g_gnss.hAccMm / 10,
           (unsigned long)g_gnss.hAccMm % 10);
  line(y, "HACC", v); y += 22;
  snprintf(v, sizeof(v), "%.1f", g_gnss.pdop);
  line(y, "PDOP", v); y += 22;

  uint32_t age = correctionAgeMs();
  bool stale = age != UINT32_MAX && age > 10000;
  if (age == UINT32_MAX) snprintf(v, sizeof(v), "--");
  else snprintf(v, sizeof(v), "%lu s", (unsigned long)(age / 1000));
  line(y, "CORR", v, stale && staleFlash); y += 22;

  line(y, "NTRP", ntripStateName()); y += 22;
  if (g_link.wifiUp) snprintf(v, sizeof(v), "up %d dBm", g_link.wifiRssi);
  else snprintf(v, sizeof(v), "down");
  line(y, "WIFI", v);
}

void drawPageSys() {
  char v[40];
  int16_t y = kHeaderH + 8;
  snprintf(v, sizeof(v), "%s", g_log.sdOk ? "ok" : "FAIL");
  line(y, "SD  ", v); y += 22;
  snprintf(v, sizeof(v), "%s",
           g_log.fileOpen ? (strrchr(g_log.fileName, '/')
                                 ? strrchr(g_log.fileName, '/') + 1
                                 : g_log.fileName)
                          : "-");
  line(y, "FILE", v); y += 22;
  snprintf(v, sizeof(v), "%lu KB", (unsigned long)(g_log.bytesWritten / 1024));
  line(y, "SIZE", v); y += 22;
  snprintf(v, sizeof(v), "%lu MB", (unsigned long)(g_log.sdFreeKB / 1024));
  line(y, "FREE", v); y += 22;
  snprintf(v, sizeof(v), "%lu B", (unsigned long)g_log.bufHighWater);
  line(y, "BUFH", v); y += 22;
  snprintf(v, sizeof(v), "%u", g_link.tcpClients);
  line(y, "TCP ", v); y += 22;
  snprintf(v, sizeof(v), "%lu KB", (unsigned long)(g_link.rtcmBytes / 1024));
  line(y, "RTCM", v); y += 22;
  snprintf(v, sizeof(v), "%lu min", (unsigned long)(millis() / 60000));
  line(y, "UPTM", v);
}

// Map a raw touch sample to screen x/y for the active rotation.
bool readTouch(int16_t& sx, int16_t& sy) {
  TSPoint p = ts.getPoint();
  if (p.z < kPressMin || p.z > kPressMax) return false;
  int16_t x = map(p.x, kRawMin, kRawMax, 0, W - 1);
  int16_t y = map(p.y, kRawMin, kRawMax, 0, H - 1);
  if (kRotation == 2) { x = W - 1 - x; y = H - 1 - y; }
  sx = constrain(x, 0, W - 1);
  sy = constrain(y, 0, H - 1);
  return true;
}

void handleTouch() {
  if (shutdownDone) return;
  int16_t x, y;
  if (!readTouch(x, y)) return;
  if (millis() - lastTouchMs < 300) return;  // debounce/repeat lockout
  lastTouchMs = millis();
  if (y < kBtnY) return;  // only the button bar is touchable in v1

  uint8_t idx = x / (kBtnW + kBtnGap);
  if (idx > 2) idx = 2;
  switch (idx) {
    case 0:  // LOG toggle
      sdLoggerSetEnabled(!(g_log.fileOpen || g_log.loggingEnabled));
      drawButtons();
      break;
    case 1:  // PAGE
      page = (page + 1) % 2;
      tft.fillRect(0, kHeaderH, W, kBtnY - kHeaderH, C_BG);
      break;
    case 2:  // PWR: two-tap confirm within 3 s
      if (pwrConfirmMs && millis() - pwrConfirmMs < 3000) {
        sdLoggerShutdown();
        shutdownDone = true;
        tft.fillScreen(C_BG);
        tft.setTextSize(3);
        tft.setTextColor(C_GREEN);
        tft.setCursor(20, 140);
        tft.print(F("SAFE TO"));
        tft.setCursor(20, 170);
        tft.print(F("POWER OFF"));
      } else {
        pwrConfirmMs = millis();
        drawButtons();
      }
      break;
  }
}

}  // namespace

void uiInit() {
  pinMode(TFT_BACKLIGHT, OUTPUT);
  digitalWrite(TFT_BACKLIGHT, HIGH);
  tft.begin();
  tft.setRotation(kRotation);
  drawStaticLayout();
}

void uiPoll() {
  handleTouch();
  if (shutdownDone) return;

  if (millis() - lastDrawMs < 500) return;  // 2 Hz redraw
  lastDrawMs = millis();
  staleFlash = !staleFlash;

  if (pwrConfirmMs && millis() - pwrConfirmMs >= 3000) {
    pwrConfirmMs = 0;  // confirm window expired
    drawButtons();
  }

  drawHeader();
  if (page == 0) drawPagePos();
  else drawPageSys();
}
