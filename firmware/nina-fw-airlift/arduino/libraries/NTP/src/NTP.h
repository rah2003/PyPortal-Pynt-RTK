/*
 *
 * Copyright (c) 2025 Arduino SA
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <Arduino.h>
#include <WiFiUdp.h>

class NTP
{
public:

  static unsigned long getTime(WiFiUDP & udp);
  static int getRandomPort(int const min_port, int const max_port);

private:

  static size_t        const NTP_PACKET_SIZE      = 48;
  static int           const NTP_TIME_SERVER_PORT = 123;
  static int           const NTP_LOCAL_PORT       = 8888;
  static int           const MIN_NTP_PORT         = 49152;
  static int           const MAX_NTP_PORT         = 65535;
  static unsigned long const NTP_TIMEOUT_MS       = 1000;
  static constexpr const char * NTP_TIME_SERVER   = "pool.ntp.org";

  static void sendNTPpacket(WiFiUDP & udp);
};
