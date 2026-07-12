#include "shared.h"

GnssStatus g_gnss;
LinkStatus g_link;
LogStatus g_log;

uint32_t correctionAgeMs() {
  if (g_link.lastRtcmMs == 0) return UINT32_MAX;
  return millis() - g_link.lastRtcmMs;
}
