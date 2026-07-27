/*
 *
 * Copyright (c) 2025 Arduino SA
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "NTP.h"

unsigned long NTP::getTime(WiFiUDP & udp)
{

  udp.begin(NTP::getRandomPort(MIN_NTP_PORT, MAX_NTP_PORT));
  sendNTPpacket(udp);

  bool is_timeout = false;
  unsigned long const start = millis();
  do
  {
    is_timeout = (millis() - start) >= NTP_TIMEOUT_MS;
  } while(!is_timeout && !udp.parsePacket());

  if(is_timeout) {
    udp.stop();
    return 0;
  }

  uint8_t ntp_packet_buf[NTP_PACKET_SIZE];
  udp.read(ntp_packet_buf, NTP_PACKET_SIZE);
  udp.stop();

  unsigned long const highWord      = ntp_packet_buf[40] << 8 | ntp_packet_buf[41];
  unsigned long const lowWord       = ntp_packet_buf[42] << 8 | ntp_packet_buf[43];
  unsigned long const secsSince1900 = highWord << 16 | lowWord;

  /* Check for corrupted NTP response */
  if(secsSince1900 == 0) {
    return 0;
  }

  unsigned long const seventyYears  = 2208988800UL;
  unsigned long const epoch         = secsSince1900 - seventyYears;

  return epoch;
}

void NTP::sendNTPpacket(WiFiUDP & udp)
{
  uint8_t ntp_packet_buf[NTP_PACKET_SIZE] = {0};

  ntp_packet_buf[0]  = 0b11100011;
  ntp_packet_buf[1]  = 0;
  ntp_packet_buf[2]  = 6;
  ntp_packet_buf[3]  = 0xEC;
  ntp_packet_buf[12] = 49;
  ntp_packet_buf[13] = 0x4E;
  ntp_packet_buf[14] = 49;
  ntp_packet_buf[15] = 52;

  udp.beginPacket(NTP_TIME_SERVER, NTP_TIME_SERVER_PORT);
  udp.write(ntp_packet_buf, NTP_PACKET_SIZE);
  udp.endPacket();
}

int NTP::getRandomPort(int const min_port, int const max_port)
{
  return random(min_port, max_port);
}
