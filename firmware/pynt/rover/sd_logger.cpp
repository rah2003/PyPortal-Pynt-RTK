// SD logging pipeline — ported from RTK-Feather's adalogger_m0/rover
// main.cpp with the review-driven rules kept intact:
//   - pre-allocated contiguous file (shrinks FAT-housekeeping stalls)
//   - at most ONE 512 B chunk written per loop pass (bounded stall)
//   - freeClusterCount() exactly once at boot (it's a full FAT scan)
//   - never O_TRUNC an existing file; a-z collision suffixes
//   - 8 s sync cadence (bounded loss on power yank)
// Deltas for this platform: time comes from g_gnss (no TSYNC protocol);
// the source is the SparkFun file buffer (gnssLogExtract), not a UART
// tap; SdFat (FAT32+exFAT) instead of SdFat32.
#include "sd_logger.h"

#include "gnss.h"
#include "pins.h"
#include "settings.h"
#include "shared.h"

namespace {

SdFat sd;
FsFile logFile;

uint32_t totalBytesWritten = 0;
uint32_t cachedFreeKB = 0;
uint32_t lastFlushMs = 0;
bool shutdownLatched = false;

constexpr uint32_t kPreAllocBytes = 32UL * 1024 * 1024;

bool makeName(char* out, size_t outLen) {
  if (!g_gnss.timeValid) return false;
  char dir[16];
  snprintf(dir, sizeof(dir), "/%04u%02u%02u", g_gnss.year, g_gnss.month,
           g_gnss.day);
  if (!sd.exists(dir)) sd.mkdir(dir);
  snprintf(out, outLen, "%s/r_%02u%02u%02u.ubx", dir, g_gnss.hour,
           g_gnss.minute, g_gnss.second);
  return true;
}

void closeFile() {
  if (!g_log.fileOpen) return;
  logFile.truncate(g_log.bytesWritten);  // trim the pre-allocation tail
  logFile.sync();
  logFile.close();
  g_log.fileOpen = false;
  Serial.print(F("[sd] closed "));
  Serial.print(g_log.fileName);
  Serial.print(F(" ("));
  Serial.print(g_log.bytesWritten);
  Serial.println(F(" bytes)"));
}

bool openFile() {
  char name[44];
  if (!makeName(name, sizeof(name))) return false;  // no valid GNSS time yet
  if (sd.exists(name)) {  // collision guard: never overwrite a log
    size_t len = strlen(name);
    char c;
    for (c = 'a'; c <= 'z'; c++) {
      snprintf(name + len - 4, 6, "%c.ubx", c);
      if (!sd.exists(name)) break;
    }
    if (c > 'z') {
      Serial.println(F("[sd] 27 filename collisions — not logging"));
      return false;
    }
  }
  if (!logFile.open(name, O_RDWR | O_CREAT | O_TRUNC)) {
    Serial.print(F("[sd] failed to open "));
    Serial.println(name);
    return false;
  }
  if (!logFile.preAllocate(kPreAllocBytes))
    Serial.println(F("[sd] preAllocate failed — logging non-contiguous"));
  strncpy(g_log.fileName, name, sizeof(g_log.fileName) - 1);
  g_log.fileName[sizeof(g_log.fileName) - 1] = '\0';
  g_log.bytesWritten = 0;
  g_log.fileOpen = true;
  Serial.print(F("[sd] logging to "));
  Serial.println(name);
  return true;
}

void drainOneChunk() {
  // ONE bounded write per pass: while logFile.write() rides out an SD
  // stall, the GNSS stream accumulates in the SERCOM buffer + SparkFun
  // file buffer (platform.md two-stage budget).
  static uint8_t chunk[512];
  if (gnssLogAvailable() == 0) return;
  size_t n = gnssLogExtract(chunk, sizeof(chunk));
  if (n == 0) return;
  if (g_log.fileOpen) {
    logFile.write(chunk, n);
    g_log.bytesWritten += n;
    totalBytesWritten += n;
  }
}

}  // namespace

bool sdLoggerInit() {
  g_log.sdOk = sd.begin(SD_CS, SD_SCK_MHZ(12));
  if (!g_log.sdOk) {
    Serial.println(F("[sd] init FAILED at CS=32 — logging + config disabled"));
    return false;
  }
  Serial.println(F("[sd] init OK"));
  // One-time full FAT scan, HERE, before logging can start — never at 1 Hz
  // (Feather review finding: it starved the stream for seconds).
  int32_t freeClusters = sd.freeClusterCount();
  if (freeClusters > 0) {
    cachedFreeKB = (uint32_t)(((uint64_t)freeClusters *
                               sd.sectorsPerCluster() * 512ULL) / 1024ULL);
    Serial.print(F("[sd] "));
    Serial.print(cachedFreeKB);
    Serial.println(F(" KB free"));
  }
  g_log.loggingEnabled = g_settings.logUbx;
  return true;
}

void sdLoggerPoll() {
  if (!g_log.sdOk) return;

  if (g_log.loggingEnabled && !shutdownLatched && g_gnss.timeValid &&
      !g_log.fileOpen) {
    openFile();
  }

  drainOneChunk();
  g_log.bufHighWater = gnssLogBufHighWater();
  uint32_t writtenKB = totalBytesWritten / 1024;
  g_log.sdFreeKB = cachedFreeKB > writtenKB ? cachedFreeKB - writtenKB : 0;

  uint32_t now = millis();
  if (g_log.fileOpen && now - lastFlushMs > 8000) {
    lastFlushMs = now;
    logFile.sync();  // bounded loss: a yanked bank costs <= 8 s
  }
}

void sdLoggerSetEnabled(bool on) {
  if (shutdownLatched) return;  // shutdown wins until reset
  g_log.loggingEnabled = on;
  if (!on) closeFile();  // reopens fresh (new timestamp) on next enable
}

void sdLoggerShutdown() {
  closeFile();
  g_log.loggingEnabled = false;
  shutdownLatched = true;  // matches "safe to power off"
}

bool sdIsOk() { return g_log.sdOk; }
SdFat& sdCard() { return sd; }
