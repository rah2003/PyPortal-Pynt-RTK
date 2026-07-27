#pragma once
#include <Arduino.h>

// BLE NUS phone link — FEATURE_BLE only (pynt-rover-coex env). The iOS
// SW Maps path from Q19: SW Maps on iOS speaks BLE NUS, not TCP.
// No no-op stubs here: every call site is #if FEATURE_BLE gated so the
// shipping pynt-rover build stays byte-identical.
void bleNusInit();  // call after ntripInit() — rides the same SPI link
void bleNusPoll();  // every loop pass; bounded work, never blocks
// Tee target: one complete NMEA sentence (with CRLF). Called from
// tcp_nmea.cpp's drain; queues only, never touches the SPI bus itself.
void bleNusOnNmeaLine(const char* line, size_t len);
