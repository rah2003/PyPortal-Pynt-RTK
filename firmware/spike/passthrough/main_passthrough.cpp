// Serial<->NINA passthrough for flashing the ESP32 AirLift with esptool.
// One sketch, three envs (passthrough-metro / passthrough-feather /
// passthrough-pynt) — pins come from the variant where it defines them
// (Metro, Pynt) or from build_flags (Feather + AirLift Wing).
//
// On boot this holds ESP32 GPIO0 LOW through a reset pulse, dropping the
// module into its ROM serial bootloader (that ROM is mask ROM — it cannot
// be bricked by a bad flash; worst case you rerun this and reflash).
// Then it pumps bytes USB<->SerialNina at a FIXED 115200. esptool must be
// invoked with --baud 115200 and --before/--after no_reset, because this
// sketch owns the strap pins, not esptool.
#include <Arduino.h>

#ifndef SerialNina
#define SerialNina Serial1  // feather_m0 variant has no SerialNina alias
#endif
#if !defined(NINA_GPIO0) || !defined(NINA_RESETN)
#ifdef ESP32_GPIO0
#define NINA_GPIO0 ESP32_GPIO0
#define NINA_RESETN ESP32_RESETN
#else
#error "Define NINA_GPIO0 / NINA_RESETN (build_flags) for this board"
#endif
#endif

void setup() {
  Serial.begin(115200);
  SerialNina.begin(115200);

#ifdef SPIWIFI_SS
  // Keep the SPI CS deselected: nina-fw samples SS at boot (LOW would
  // select its legacy UART-HCI mode) and nothing should sit on the bus
  // while the ROM loader owns the pins.
  pinMode(SPIWIFI_SS, OUTPUT);
  digitalWrite(SPIWIFI_SS, HIGH);
#endif

  pinMode(NINA_GPIO0, OUTPUT);
  digitalWrite(NINA_GPIO0, LOW);  // strap: boot to ROM serial loader
  pinMode(NINA_RESETN, OUTPUT);
  digitalWrite(NINA_RESETN, LOW);  // reset pulse (EN is active-low)
  delay(100);
  digitalWrite(NINA_RESETN, HIGH);
  delay(100);
  // GPIO0 stays LOW; harmless once the ROM loader is running, and it
  // keeps any spurious reset landing back in the loader.
}

void loop() {
  while (Serial.available()) SerialNina.write((uint8_t)Serial.read());
  while (SerialNina.available()) Serial.write((uint8_t)SerialNina.read());
}
