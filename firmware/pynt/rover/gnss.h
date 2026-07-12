#pragma once
#include <Arduino.h>

// Owns SerialGNSS (SERCOM0 UART to F9P UART1) and the SparkFun GNSS
// object. Everything GNSS flows through here: config, NAV-PVT status,
// NMEA (GGA capture + line tee for the TCP server), RTCM injection, and
// the RAWX/SFRBX file buffer the SD logger drains.
bool gnssInit();          // connect + apply project config; false if no F9P
void gnssPoll();          // service the stream; call every loop pass
void gnssInjectRtcm(const uint8_t* data, size_t len);
size_t gnssLogAvailable();
size_t gnssLogExtract(uint8_t* out, size_t maxLen);
uint16_t gnssLogBufHighWater();
void gnssRequestFactoryRecover();  // executed on the next poll

// Complete NMEA sentences for the TCP server: returns length (0 = none),
// copies one sentence (with CRLF) into out. Drained by tcp_nmea.cpp.
size_t gnssNextNmeaLine(char* out, size_t maxLen);
