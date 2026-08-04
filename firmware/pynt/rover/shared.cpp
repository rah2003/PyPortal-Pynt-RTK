#include "shared.h"

GnssStatus g_gnss;
LinkStatus g_link;
LogStatus g_log;
BaseStatus g_base;
CorrHealth g_corr;

uint32_t correctionAgeMs() {
  if (g_link.lastRtcmMs == 0) return UINT32_MAX;
  return millis() - g_link.lastRtcmMs;
}

uint32_t corrRxAgeMs() {
  if (g_corr.lastUsedMs == 0) return UINT32_MAX;
  return millis() - g_corr.lastUsedMs;
}
