#pragma once
#include <SparkFun_u-blox_GNSS_v3.h>

// Applies the full project config (ports, messages, nav, rover TMODE)
// over UBX-CFG-VALSET. Split from gnss.cpp so Phase 3's Base-mode config
// extends this file without touching the stream plumbing.
bool gnssApplyProjectConfig(SFE_UBLOX_GNSS_SERIAL& gnss);
bool gnssFactoryRecover(SFE_UBLOX_GNSS_SERIAL& gnss);
