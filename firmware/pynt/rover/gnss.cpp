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
#include "uart_gnss.h"

// SERCOM0 UART: TX=D3(PA04,pad0) RX=D4(PA05,pad1) — docs/hardware/wiring.md.
// GnssUart, not core Uart: per-instance ring sizing (32 KiB RX / 2 KiB TX)
// instead of the global SERIAL_BUFFER_SIZE that cost 131 KiB — uart_gnss.h.
static GnssUart SerialGNSS(&sercom0, GNSS_RX_PIN, GNSS_TX_PIN,
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

void onSvin(UBX_NAV_SVIN_data_t* svin) {
  g_base.svinActive = svin->active;
  g_base.svinValid = svin->valid;
  g_base.svinDurS = svin->dur;
  g_base.svinMeanAcc01mm = svin->meanAcc;
}

// ---- Phase C telemetry callbacks (team review P2/P10/P12) ----

void onRxmCor(UBX_RXM_COR_data_t* cor) {
  g_corr.corMsgs++;
  if (cor->statusInfo.bits.msgUsed == 2) {
    g_corr.corUsed++;
    g_corr.lastUsedMs = millis();
  }
  if (cor->statusInfo.bits.errStatus == 2) g_corr.corErrors++;
  if (cor->statusInfo.bits.msgTypeValid) g_corr.lastMsgType = cor->msgType;
}

void onRelPosNed(UBX_NAV_RELPOSNED_data_t* rel) {
  g_corr.relValid = rel->flags.bits.relPosValid;
  // relPosLength: cm + 0.1 mm high-precision component
  g_corr.baselineM =
      rel->relPosLength * 0.01f + rel->relPosHPLength * 0.0001f;
  g_corr.refStationId = rel->refStationId;
}

void onNavSat(UBX_NAV_SAT_data_t* sat) {
  uint16_t used = 0, cn0Sum = 0;
  uint8_t cn0Max = 0;
  for (uint16_t i = 0; i < sat->header.numSvs; i++) {
    if (!sat->blocks[i].flags.bits.svUsed) continue;
    used++;
    cn0Sum += sat->blocks[i].cno;
    if (sat->blocks[i].cno > cn0Max) cn0Max = sat->blocks[i].cno;
  }
  g_corr.satsUsed = (uint8_t)used;
  g_corr.meanCn0 = used ? (float)cn0Sum / used : 0;
  g_corr.maxCn0 = cn0Max;
}

void onPvt(UBX_NAV_PVT_data_t* pvt) {
  g_gnss.lastPvtMs = millis();
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

// quick=true is the in-loop retry path: the library's default maxWait is
// 1100 ms and begin() probes up to 3x per baud — a missing F9P used to
// stall the superloop ~6.6 s per attempt (team review H2). 250 ms still
// answers a live module comfortably at 115200.
bool connectGnss(bool quick = false) {
  const uint16_t maxWait = quick ? 250 : 1100;
  // Fast path: project baud already persisted in the F9P's flash (this
  // Lite moved over from the Feather rig with 115200 saved — checklists.md).
  uartBegin(kUart1Baud);
  if (gnss.begin(SerialGNSS, maxWait)) return true;

  // Factory-fresh module: raise the baud from the 38400 default.
  uartBegin(kUart1DefaultBaud);
  if (!gnss.begin(SerialGNSS, maxWait)) return false;
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
  gnss.setAutoNAVSVINcallbackPtr(&onSvin);  // survey-in progress (Base mode)
  // RAWX/SFRBX land in the library's file buffer; sd_logger.cpp drains it.
  gnss.setAutoRXMRAWX(true);
  gnss.logRXMRAWX(true);
  gnss.setAutoRXMSFRBX(true);
  gnss.logRXMSFRBX(true);
  // Phase C telemetry. The setAuto* calls enable their own UART1 MSGOUT;
  // RXM-COR's MSGOUT is in gnss_config.cpp applyMessages (the library
  // only registers the callback for COR). Added UART1 load at 1 Hz:
  // RELPOSNED 72 B + NAV-SAT ~8+12/sat (~450 B) — well inside the
  // 115200 budget (Bumble P4 math).
  gnss.setRXMCORcallbackPtr(&onRxmCor);
  gnss.setAutoRELPOSNEDcallbackPtr(&onRelPosNed);
  gnss.setAutoNAVSATcallbackPtr(&onNavSat);
}

bool modeApplyRequested = false;

// GGA field 13 (0-based counting the sentence id as field 0) is the age
// of differential data as the F9P itself reports it — parsed from the
// sentence already captured for the NTRIP upstream (cheapest possible
// receiver-truth age, Bumble P2).
float ggaDiffAgeS(const char* gga) {
  uint8_t commas = 0;
  const char* p = gga;
  while (*p && commas < 13) {
    if (*p == ',') commas++;
    p++;
  }
  if (commas < 13 || *p == ',' || *p == '*' || *p == '\0') return -1;
  return (float)atof(p);
}

}  // namespace

bool gnssInit() {
  gnss.setFileBufferSize(kFileBufferSize);  // must precede begin()
  if (!connectGnss()) {
    g_gnss.f9pDetected = false;
    return false;
  }
  g_gnss.f9pDetected = true;
  g_gnss.lastPvtMs = millis();  // PVT-age watchdog grace until the first fix
  Serial.println(F("[gnss] ZED-F9P connected (UART1 @115200 on SERCOM0/D3-D4)"));
  // MON-VER over UART1 — u-center can't coexist with this link (its USB
  // adapter bridges the same UART1), so the firmware reports the version.
  if (gnss.getModuleInfo()) {
    Serial.print(F("[gnss] "));
    Serial.print(gnss.getModuleName());
    Serial.print(F(" fw "));
    Serial.print(gnss.getFirmwareType());
    Serial.print(' ');
    Serial.print(gnss.getFirmwareVersionHigh());
    Serial.print('.');
    Serial.print(gnss.getFirmwareVersionLow());
    Serial.print(F(" protver "));
    Serial.print(gnss.getProtocolVersionHigh());
    Serial.print('.');
    Serial.println(gnss.getProtocolVersionLow());
  }
  if (!gnssApplyProjectConfig(gnss))
    Serial.println(F("[gnss] WARNING: some VALSET writes not ACKed"));
  setupLogging();
  return true;
}

void gnssPoll() {
  // The Feather build retried the F9P connect forever; keep that field
  // behavior — a late-powered or reseated Lite must not require a reboot.
  if (!g_gnss.f9pDetected) {
    static uint32_t lastRetryMs = 0;
    static uint32_t retryIntervalMs = 5000;
    if (millis() - lastRetryMs < retryIntervalMs) return;
    lastRetryMs = millis();
    if (!connectGnss(true)) {  // quick probe — keep the loop responsive
      retryIntervalMs = min(retryIntervalMs * 2, (uint32_t)30000);
      return;
    }
    retryIntervalMs = 5000;
    g_gnss.f9pDetected = true;
    g_gnss.lastPvtMs = millis();  // watchdog grace until the first PVT
    Serial.println(F("[gnss] ZED-F9P connected (late)"));
    if (!gnssApplyProjectConfig(gnss))
      Serial.println(F("[gnss] WARNING: some VALSET writes not ACKed"));
    setupLogging();
  }

  if (modeApplyRequested) {
    modeApplyRequested = false;
    g_base = BaseStatus();  // stale survey-in numbers must not survive a switch
    Serial.print(F("[gnss] applying mode: "));
    Serial.println(g_settings.mode == DeviceMode::Base ? F("base") : F("rover"));
    if (!gnssApplyProjectConfig(gnss))
      Serial.println(F("[gnss] WARNING: mode VALSET not fully ACKed"));
  }

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

  // PVT-age watchdog (team review H1): NAV-PVT arrives at nav rate; if it
  // stops (cable loose, brownout, module death) the last position must
  // not keep wearing a green RTK FIX badge. Clearing f9pDetected flips
  // the UI to NO GNSS and re-enters the reconnect path above.
  constexpr uint32_t kPvtStaleMs = 5000;
  if (g_gnss.f9pDetected && millis() - g_gnss.lastPvtMs > kPvtStaleMs) {
    g_gnss.f9pDetected = false;
    Serial.println(F("[gnss] no PVT for 5 s — F9P lost, reconnecting"));
  }

  if (millis() - lastStatusPushMs >= 200) {  // ~5 Hz
    lastStatusPushMs = millis();
    if (ggaFresh) {
      ggaFresh = false;
      strncpy(g_gnss.lastGga, pendingGga, sizeof(g_gnss.lastGga) - 1);
      g_gnss.lastGgaMs = millis();
      g_corr.ggaDiffAgeS = ggaDiffAgeS(g_gnss.lastGga);
    }
  }

  // MON-RF poll (Phase C, Bumble P10): no auto/callback support in the
  // library, so a bounded poll — every 10 s, 200 ms maxWait. AGC, noise
  // floor and the jamming indicator are the data the coex desense
  // question has been missing. (Bisected 2026-08-03 and cleared — the
  // boot-loop was heap/stack collision, uart_gnss.h.)
  static uint32_t lastRfPollMs = 0;
  if (g_gnss.f9pDetected && millis() - lastRfPollMs >= 10000) {
    lastRfPollMs = millis();
    UBX_MON_RF_data_t rf;
    if (gnss.getRFinformation(&rf, 200)) {
      g_corr.rfBlocks =
          rf.header.nBlocks > 2 ? (uint8_t)2 : rf.header.nBlocks;
      for (uint8_t b = 0; b < g_corr.rfBlocks; b++) {
        g_corr.jammingState[b] = rf.blocks[b].flags.bits.jammingState;
        g_corr.jamInd[b] = rf.blocks[b].jamInd;
        g_corr.noisePerMS[b] = rf.blocks[b].noisePerMS;
        g_corr.agcPct[b] = (uint8_t)((rf.blocks[b].agcCnt * 100UL) / 8191);
      }
      g_corr.lastRfMs = millis();
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
void gnssRequestModeApply() { modeApplyRequested = true; }

size_t gnssNextNmeaLine(char* out, size_t maxLen) {
  if (nmeaQCount == 0) return 0;
  strncpy(out, nmeaQueue[nmeaQTail], maxLen - 1);
  out[maxLen - 1] = '\0';
  nmeaQTail = (nmeaQTail + 1) % kNmeaQueueLines;
  nmeaQCount--;
  return strlen(out);
}
