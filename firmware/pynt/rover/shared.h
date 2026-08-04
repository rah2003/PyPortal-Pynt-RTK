// Shared status state. Descended from RTK-Feather's shared.h, radically
// simplified by the single-MCU architecture: one core, one superloop —
// no cross-core ring buffers (NTRIP bytes inject directly into the GNSS
// library in ntrip.cpp), no mutexes (writers and readers are the same
// thread), no M0Status (there is no second MCU; the logger reports
// through LogStatus below).
#pragma once
#include <Arduino.h>

#include "features.h"  // FEATURE_BLE gates the BLE fields below

struct GnssStatus {
  bool f9pDetected = false;
  uint8_t fixType = 0;   // NAV-PVT fixType: 0 none, 2 2D, 3 3D
  uint8_t carrSoln = 0;  // 0 none, 1 float, 2 fixed
  uint8_t numSV = 0;
  double latDeg = 0, lonDeg = 0;
  double hMslM = 0;
  uint32_t hAccMm = 0, vAccMm = 0;
  float pdop = 0;
  bool timeValid = false;
  uint16_t year = 0;
  uint8_t month = 0, day = 0, hour = 0, minute = 0, second = 0;
  char lastGga[120] = {0};  // most recent GGA sentence (NTRIP upstream)
  uint32_t lastGgaMs = 0;
  uint32_t lastPvtMs = 0;  // PVT-age watchdog: a silent F9P must not keep
                           // a green fix badge (team review H1)
};

struct LinkStatus {
  bool wifiUp = false;
  bool ntripConnected = false;
  uint32_t lastRtcmMs = 0;
  uint32_t rtcmBytes = 0;
  int8_t wifiRssi = 0;   // dBm; 0 = unknown
  uint8_t tcpClients = 0;  // NMEA TCP connections (tcp_nmea.cpp)
  char wifiIp[16] = {0};   // dotted-quad once joined; the phone app needs
                           // this + tcpport to connect, so the UI shows it
#if FEATURE_BLE           // written by ble_nus.cpp only
  bool bleUp = false;         // BLE.begin() succeeded (nina-fw >= 3.0)
  bool bleConnected = false;  // a central is connected
  bool bleSubscribed = false; // ...and subscribed to NUS TX (streaming)
  uint32_t bleLinesTx = 0;    // NMEA sentences notified
  uint32_t bleDrops = 0;      // ring overflowed (slow phone), drop-oldest
#endif
};

struct LogStatus {
  bool sdOk = false;
  bool loggingEnabled = true;  // "always-on" default (Metro Q6 pattern)
  bool fileOpen = false;
  char fileName[40] = {0};
  uint32_t bytesWritten = 0;
  uint32_t sdFreeKB = 0;
  uint32_t bufHighWater = 0;  // GNSS file-buffer high-water (platform.md math check)
};

struct BaseStatus {  // NAV-SVIN, only meaningful in Base mode
  bool svinActive = false;
  bool svinValid = false;
  uint32_t svinDurS = 0;
  uint32_t svinMeanAcc01mm = 0;  // 0.1 mm units, as reported
};

// Phase C (team review P2/P10/P12): receiver-truth correction and RF
// telemetry. Everything the old "correction age" hid — whether the F9P
// actually USED the corrections, how far the (virtual) base is, and
// what the RF front end sees while WiFi+BLE transmit inches away.
struct CorrHealth {
  // UBX-RXM-COR: per correction message as the receiver ingests it
  uint32_t corMsgs = 0;      // correction messages seen
  uint32_t corUsed = 0;      // statusInfo.msgUsed == 2
  uint32_t corErrors = 0;    // statusInfo.errStatus == 2 (erroneous)
  uint16_t lastMsgType = 0;  // last RTCM type ingested (e.g. 1074)
  uint32_t lastUsedMs = 0;   // millis() of last USED correction — the
                             // honest correction age (vs link age)
  // UBX-NAV-RELPOSNED: rover<->base vector
  bool relValid = false;
  float baselineM = 0;       // distance to the (virtual) reference station
  uint16_t refStationId = 0;
  // UBX-NAV-SAT aggregate (1 Hz)
  uint8_t satsUsed = 0;      // sats actually used in the solution
  float meanCn0 = 0;         // mean C/N0 over used sats, dBHz
  uint8_t maxCn0 = 0;
  // UBX-MON-RF (polled ~0.1 Hz): per-band front-end health
  uint8_t rfBlocks = 0;
  uint8_t jammingState[2] = {0, 0};  // 0 unk, 1 ok, 2 warning, 3 critical
  uint8_t jamInd[2] = {0, 0};        // CW interference indicator 0-255
  uint16_t noisePerMS[2] = {0, 0};
  uint8_t agcPct[2] = {0, 0};        // AGC count scaled to 0-100
  uint32_t lastRfMs = 0;
  // GGA field 13 (age of differential, as the F9P itself reports it) —
  // parsed from the sentence already captured for the NTRIP upstream
  float ggaDiffAgeS = -1;    // -1 = field empty / no fix yet
};

// Receiver-confirmed correction age in ms (UINT32_MAX if none used yet).
uint32_t corrRxAgeMs();

extern GnssStatus g_gnss;  // written by gnss.cpp only
extern LinkStatus g_link;  // written by ntrip.cpp / tcp_nmea.cpp only
extern LogStatus g_log;    // written by sd_logger.cpp only
extern BaseStatus g_base;  // written by gnss.cpp only
extern CorrHealth g_corr;  // written by gnss.cpp only (Phase C)

// Correction age in ms (UINT32_MAX if never received).
uint32_t correctionAgeMs();
