// GNSS stream owner — descended from RTK-Feather's gnss_task.cpp, but a
// polled module instead of a FreeRTOS task, and it re-acquires Metro's
// SparkFun file-buffer logging (setFileBufferSize + auto-RAWX/SFRBX):
// with one MCU and one UART there is no passive tap — the library must
// own the byte stream, so the logger drains the library's buffer instead
// of a separate extractor (delta noted in docs/hardware/platform.md).
#include "gnss.h"

#include <SparkFun_u-blox_GNSS_v3.h>
#include <wiring_private.h>

#include "gnss_config.h"
#include "pins.h"
#include "settings.h"
#include "shared.h"

// SERCOM0 UART: TX=D3(PA04,pad0) RX=D4(PA05,pad1) — docs/hardware/wiring.md.
static Uart SerialGNSS(&sercom0, GNSS_RX_PIN, GNSS_TX_PIN,
                       SERCOM_RX_PAD_1, UART_TX_PAD_0);
void SERCOM0_0_Handler() { SerialGNSS.IrqHandler(); }
void SERCOM0_1_Handler() { SerialGNSS.IrqHandler(); }
void SERCOM0_2_Handler() { SerialGNSS.IrqHandler(); }
void SERCOM0_3_Handler() { SerialGNSS.IrqHandler(); }

static SFE_UBLOX_GNSS_SERIAL gnss;

static constexpr uint32_t kUart1Baud = 115200;
static constexpr uint32_t kUart1DefaultBaud = 38400;

// SparkFun file buffer for RAWX/SFRBX (Metro pattern). 32 KiB — the
// platform.md contention math wants >= 2.9 KB to ride out a 250 ms SD
// stall; 32 KiB is ~11x that, affordable in 256 KiB of RAM.
static constexpr uint16_t kFileBufferSize = 32768;

// ---------------------------------------------------------------- NMEA
// processNMEA is a WEAK member in the SparkFun v3 library; defining it
// replaces the no-op. GGA is captured for the NTRIP upstream; every
// complete sentence is also queued for the SW Maps TCP server.
namespace {

constexpr size_t kNmeaQueueLines = 8;
constexpr size_t kNmeaLineMax = 128;
char nmeaQueue[kNmeaQueueLines][kNmeaLineMax];
uint8_t nmeaQHead = 0, nmeaQTail = 0, nmeaQCount = 0;

char nmeaLine[kNmeaLineMax];
size_t nmeaLen = 0;
char pendingGga[kNmeaLineMax] = {0};
bool ggaFresh = false;

void feedNmeaByte(char c) {
  if (nmeaLen < sizeof(nmeaLine) - 1) nmeaLine[nmeaLen++] = c;
  if (c != '\n') return;
  nmeaLine[nmeaLen] = '\0';
  if (nmeaLen > 6 && strncmp(nmeaLine + 3, "GGA", 3) == 0) {
    strncpy(pendingGga, nmeaLine, sizeof(pendingGga) - 1);
    ggaFresh = true;
  }
  if (nmeaQCount < kNmeaQueueLines) {  // full queue: drop the sentence whole
    strncpy(nmeaQueue[nmeaQHead], nmeaLine, kNmeaLineMax - 1);
    nmeaQueue[nmeaQHead][kNmeaLineMax - 1] = '\0';
    nmeaQHead = (nmeaQHead + 1) % kNmeaQueueLines;
    nmeaQCount++;
  }
  nmeaLen = 0;
}

}  // namespace

void DevUBLOXGNSS::processNMEA(char c) { feedNmeaByte(c); }

// ---------------------------------------------------------------- core
namespace {

bool factoryRecoverRequested = false;
uint32_t lastStatusPushMs = 0;

void onPvt(UBX_NAV_PVT_data_t* pvt) {
  g_gnss.fixType = pvt->fixType;
  g_gnss.carrSoln = pvt->flags.bits.carrSoln;
  g_gnss.numSV = pvt->numSV;
  g_gnss.latDeg = pvt->lat * 1e-7;
  g_gnss.lonDeg = pvt->lon * 1e-7;
  g_gnss.hMslM = pvt->hMSL * 1e-3;
  g_gnss.hAccMm = pvt->hAcc;
  g_gnss.vAccMm = pvt->vAcc;
  g_gnss.pdop = pvt->pDOP * 0.01f;
  g_gnss.timeValid = pvt->valid.bits.validDate && pvt->valid.bits.validTime;
  g_gnss.year = pvt->year;
  g_gnss.month = pvt->month;
  g_gnss.day = pvt->day;
  g_gnss.hour = pvt->hour;
  g_gnss.minute = pvt->min;
  g_gnss.second = pvt->sec;
}

void uartBegin(uint32_t baud) {
  SerialGNSS.begin(baud);
  // Uart::begin muxes the pins as PIO_SERCOM; PA04/PA05 need ALT — must
  // follow every begin() call (docs/hardware/wiring.md recipe).
  pinPeripheral(GNSS_TX_PIN, PIO_SERCOM_ALT);
  pinPeripheral(GNSS_RX_PIN, PIO_SERCOM_ALT);
}

bool connectGnss() {
  // Fast path: project baud already persisted in the F9P's flash (this
  // Lite moved over from the Feather rig with 115200 saved — checklists.md).
  uartBegin(kUart1Baud);
  if (gnss.begin(SerialGNSS)) return true;

  // Factory-fresh module: raise the baud from the 38400 default.
  uartBegin(kUart1DefaultBaud);
  if (!gnss.begin(SerialGNSS)) return false;
  Serial.println(F("[gnss] F9P at default 38400, switching to 115200"));
  gnss.newCfgValset(VAL_LAYER_ALL);
  gnss.addCfgValset(UBLOX_CFG_UART1_BAUDRATE, kUart1Baud);
  gnss.sendCfgValset(250);  // ACK arrives at the new baud — may be lost
  delay(200);
  uartBegin(kUart1Baud);
  return gnss.begin(SerialGNSS);
}

void setupLogging() {
  gnss.setAutoPVTcallbackPtr(&onPvt);  // NAV-PVT at nav rate, no polling
  // RAWX/SFRBX land in the library's file buffer; sd_logger.cpp drains it.
  gnss.setAutoRXMRAWX(true);
  gnss.logRXMRAWX(true);
  gnss.setAutoRXMSFRBX(true);
  gnss.logRXMSFRBX(true);
}

}  // namespace

bool gnssInit() {
  gnss.setFileBufferSize(kFileBufferSize);  // must precede begin()
  if (!connectGnss()) {
    g_gnss.f9pDetected = false;
    return false;
  }
  g_gnss.f9pDetected = true;
  Serial.println(F("[gnss] ZED-F9P connected (UART1 @115200 on SERCOM0/D3-D4)"));
  if (!gnssApplyProjectConfig(gnss))
    Serial.println(F("[gnss] WARNING: some VALSET writes not ACKed"));
  setupLogging();
  return true;
}

void gnssPoll() {
  if (factoryRecoverRequested) {
    factoryRecoverRequested = false;
    gnssFactoryRecover(gnss);
    uartBegin(kUart1DefaultBaud);  // factory default baud
    if (gnss.begin(SerialGNSS)) {
      gnss.newCfgValset(VAL_LAYER_ALL);
      gnss.addCfgValset(UBLOX_CFG_UART1_BAUDRATE, kUart1Baud);
      gnss.sendCfgValset(250);
      delay(200);
      uartBegin(kUart1Baud);
      if (gnss.begin(SerialGNSS)) {
        Serial.println(F("[gnss] reapplying project config..."));
        gnssApplyProjectConfig(gnss);
        setupLogging();
      }
    } else {
      Serial.println(F("[gnss] F9P not answering after factory reset"));
    }
  }

  // Parse whatever the F9P queued: fires the PVT callback, feeds
  // processNMEA, and appends RAWX/SFRBX to the file buffer.
  gnss.checkUblox();
  gnss.checkCallbacks();

  if (millis() - lastStatusPushMs >= 200) {  // ~5 Hz
    lastStatusPushMs = millis();
    if (ggaFresh) {
      ggaFresh = false;
      strncpy(g_gnss.lastGga, pendingGga, sizeof(g_gnss.lastGga) - 1);
      g_gnss.lastGgaMs = millis();
    }
  }
}

void gnssInjectRtcm(const uint8_t* data, size_t len) {
  gnss.pushRawData(const_cast<uint8_t*>(data), len);
}

size_t gnssLogAvailable() { return gnss.fileBufferAvailable(); }

size_t gnssLogExtract(uint8_t* out, size_t maxLen) {
  // extractFileBufferData returns BOOL and fails outright if asked for
  // more than fileBufferAvailable() — request exactly what exists,
  // capped to the caller's chunk (PaulZC RAWX_Logger pattern).
  uint16_t avail = gnss.fileBufferAvailable();
  uint16_t n = (uint16_t)min((size_t)avail, maxLen);
  if (n == 0) return 0;
  if (!gnss.extractFileBufferData(out, n)) return 0;
  return n;
}

uint16_t gnssLogBufHighWater() { return gnss.getMaxFileBufferAvail(); }

void gnssRequestFactoryRecover() { factoryRecoverRequested = true; }

size_t gnssNextNmeaLine(char* out, size_t maxLen) {
  if (nmeaQCount == 0) return 0;
  strncpy(out, nmeaQueue[nmeaQTail], maxLen - 1);
  out[maxLen - 1] = '\0';
  nmeaQTail = (nmeaQTail + 1) % kNmeaQueueLines;
  nmeaQCount--;
  return strlen(out);
}
