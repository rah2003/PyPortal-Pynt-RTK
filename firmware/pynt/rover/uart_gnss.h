/*
  Adapted from Uart.h/Uart.cpp, Copyright (c) 2015 Arduino LLC (LGPL 2.1+),
  Adafruit SAMD core. See LICENSE for the vendored-code note.
*/

// Why this exists instead of the core Uart: the core sizes BOTH ring
// buffers of EVERY Uart instance from one global SERIAL_BUFFER_SIZE.
// Building with -DSERIAL_BUFFER_SIZE=32768 (needed so the GNSS RX ring
// rides out WiFiNINA's ~11 s blocking WiFi.begin() windows) therefore
// cost 4 x 32 KiB = 131 KiB of the SAMD51's 192 KiB RAM — two rings on
// the unused Serial1 plus a same-size TX ring the GNSS link never fills.
// The heap and stack shared the ~50 KiB that remained, and _sbrk only
// checks the stack pointer at allocation time, so heap blocks granted
// during setup() sat where the stack would later grow: hard faults at
// wandering addresses (bench 2026-08-03, boot-loop postmortem). This
// class carries the one big RX ring the design actually needs and a
// small TX ring, and Serial1 falls back to the core default.

#pragma once

#include <Arduino.h>

#include "HardwareSerial.h"
#include "RingBuffer.h"
#include "SERCOM.h"

// 32 KiB RX: F9P streams ~2.5 kB/s; covers ~13 s of blocked loop
// (bench-measured WiFi.begin() joins run ~10.9 s, 2026-07-19 soak).
// 2 KiB TX: caster RTCM forwarded to the F9P arrives in ~1 kB bursts;
// this absorbs a burst without write() spinning at 115200 drain rate.
inline constexpr int kGnssRxRingBytes = 32768;
inline constexpr int kGnssTxRingBytes = 2048;

class GnssUart : public HardwareSerial {
 public:
  GnssUart(SERCOM* _s, uint8_t _pinRX, uint8_t _pinTX, SercomRXPad _padRX,
           SercomUartTXPad _padTX);
  void begin(unsigned long baudRate);
  void begin(unsigned long baudrate, uint16_t config);
  void end();
  int available();
  int availableForWrite();
  int peek();
  int read();
  void flush();
  size_t write(const uint8_t data);
  using Print::write;

  void IrqHandler();

  operator bool() { return true; }

 private:
  SERCOM* sercom;
  RingBufferN<kGnssRxRingBytes> rxBuffer;
  RingBufferN<kGnssTxRingBytes> txBuffer;

  uint8_t uc_pinRX;
  uint8_t uc_pinTX;
  SercomRXPad uc_padRX;
  SercomUartTXPad uc_padTX;

  SercomNumberStopBit extractNbStopBit(uint16_t config);
  SercomUartCharSize extractCharSize(uint16_t config);
  SercomParityMode extractParity(uint16_t config);
};
