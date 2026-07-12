#pragma once

// SW Maps phone link: NMEA sentences served over TCP on the hotspot
// network (replaces the ancestors' BLE NUS — the AirLift can't run WiFi
// and BLE together; kickoff decision 2026-07-11).
void tcpNmeaInit();  // after ntripInit (WiFi.setPins must have happened)
void tcpNmeaPoll();  // accept clients, broadcast queued NMEA lines
