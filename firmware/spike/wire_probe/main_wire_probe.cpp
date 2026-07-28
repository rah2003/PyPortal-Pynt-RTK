// Wire probe for the HUZZAH32-as-nina rig (coex-bench Appendix A.2).
// Flash to the M0 when the soak reports "nina module not responding":
// re-runs a pin-by-pin diagnosis every 5 s (wiggle wires and watch).
//
// What each stage proves, using nina-fw/Arduino_SpiNINA's own
// choreography (READY idles LOW when the slave is armed; the slave
// drives READY HIGH within ~5 ms of CS asserting — spi_drv.cpp
// spiSlaveSelect — and while processing a command):
//   1. boot trace   — RESET wire + module boot (READY HIGH during boot,
//                     LOW once nina-fw arms its SPI slave)
//   2. CS ack       — CS wire + READY wire (READY reacts to CS edge)
//   3. fw version   — SCK, MOSI, MISO wires (bit-banged GET_FW_VERSION,
//                     expect E0 B7 ... "3.0.1" back)
// Pins come from the same -D set as the soak env; SPI pins are the M0's
// hardware SPI header, driven as plain GPIOs (bit-bang, ~50 kHz).
#include <Arduino.h>

#ifndef SPIWIFI_SS
#error "build via the feather-wire-probe env (-D pin set)"
#endif

namespace {

constexpr uint8_t PIN_CS = SPIWIFI_SS;
constexpr uint8_t PIN_READY = SPIWIFI_ACK;
constexpr uint8_t PIN_RST = SPIWIFI_RESET;
constexpr uint8_t PIN_SCK_BB = PIN_SPI_SCK;
constexpr uint8_t PIN_MOSI_BB = PIN_SPI_MOSI;
constexpr uint8_t PIN_MISO_BB = PIN_SPI_MISO;

uint8_t xfer(uint8_t out) {
  uint8_t in = 0;
  for (int8_t b = 7; b >= 0; b--) {
    digitalWrite(PIN_MOSI_BB, (out >> b) & 1);
    delayMicroseconds(4);
    digitalWrite(PIN_SCK_BB, HIGH);  // slave samples MOSI on this edge
    delayMicroseconds(4);
    in = (uint8_t)((in << 1) | digitalRead(PIN_MISO_BB));
    digitalWrite(PIN_SCK_BB, LOW);
  }
  return in;
}

bool waitReady(bool level, uint32_t timeoutMs) {
  uint32_t t0 = millis();
  while (digitalRead(PIN_READY) != (level ? HIGH : LOW)) {
    if (millis() - t0 > timeoutMs) return false;
  }
  return true;
}

void probe() {
  Serial.println(F("---- probe pass ----"));

  // 1. reset + boot trace
  digitalWrite(PIN_CS, HIGH);
  digitalWrite(PIN_RST, LOW);
  delay(10);
  digitalWrite(PIN_RST, HIGH);
  uint32_t t0 = millis();
  int last = digitalRead(PIN_READY);
  Serial.print(F("[1] READY at reset: "));
  Serial.println(last ? F("HIGH") : F("LOW"));
  uint8_t transitions = 0;
  while (millis() - t0 < 2500 && transitions < 8) {
    int now = digitalRead(PIN_READY);
    if (now != last) {
      Serial.print(F("    t="));
      Serial.print(millis() - t0);
      Serial.print(F("ms -> "));
      Serial.println(now ? F("HIGH") : F("LOW"));
      last = now;
      transitions++;
    }
  }
  if (digitalRead(PIN_READY) == LOW)
    Serial.println(F("[1] PASS: READY idles LOW (module armed)"));
  else {
    Serial.println(F("[1] FAIL: READY not LOW after boot — READY wire"
                     " (D11 -> \"33\") or RESET wire (D12 -> \"RST\")"));
    return;  // later stages meaningless
  }

  // 2. CS ack: nina-fw attaches a FALLING interrupt on CS (GPIO5, internal
  // pullup) that drives READY HIGH immediately — authoritative CS-wire test
  digitalWrite(PIN_CS, LOW);
  bool ack = waitReady(true, 5);
  digitalWrite(PIN_CS, HIGH);
  if (ack) Serial.println(F("[2] PASS: READY acks CS (CS + READY wires good)"));
  else {
    Serial.println(
        F("[2] FAIL: no READY reaction to CS — CS wire (D13 -> \"SCK\"/GPIO5)"));
    // 2b. beacon: toggle CS for 4 s so the wire can be found physically.
    // If the HUZZAH32's red LED blinks now, the wire sits on its "13"
    // pin (GPIO13 = LED) — move it to "SCK". A multimeter on the "SCK"
    // pin should read ~1.6 V avg while this runs (steady 3.3 V = wire
    // not arriving; that's the chip's internal pullup).
    Serial.println(F("[2b] CS beacon 4 s — watch the HUZZAH32 red LED /"
                     " meter the \"SCK\" pin"));
    uint32_t bEnd = millis() + 4000;
    while (millis() < bEnd) {
      digitalWrite(PIN_CS, LOW);
      delay(20);
      digitalWrite(PIN_CS, HIGH);
      delay(20);
    }
  }

  // 3. GET_FW_VERSION (0x37) via bit-bang, SpiDrv choreography. Runs even
  // after a [2] FAIL — the real driver also proceeds past its 5 ms ack
  // timeout, and the result adds signal either way.
  {
    digitalWrite(PIN_CS, LOW);
    waitReady(true, 5);
    uint8_t junk[4];
    const uint8_t cmd[4] = {0xE0, 0x37, 0x00, 0xEE};
    for (uint8_t i = 0; i < 4; i++) junk[i] = xfer(cmd[i]);
    digitalWrite(PIN_CS, HIGH);
    if (!waitReady(false, 1000)) {
      Serial.println(F("[3] FAIL: READY never re-armed after command"));
      return;
    }
    digitalWrite(PIN_CS, LOW);
    if (!waitReady(true, 5)) { /* ack again; tolerate */ }
    uint8_t rsp[24];
    for (uint8_t i = 0; i < sizeof(rsp); i++) rsp[i] = xfer(0xFF);
    digitalWrite(PIN_CS, HIGH);

    Serial.print(F("[3] response:"));
    for (uint8_t i = 0; i < sizeof(rsp); i++) {
      Serial.print(' ');
      Serial.print(rsp[i], HEX);
    }
    Serial.println();
    bool sawStart = false;
    for (uint8_t i = 0; i + 1 < sizeof(rsp); i++) {
      if (rsp[i] == 0xE0 && rsp[i + 1] == 0xB7) {
        sawStart = true;
        Serial.print(F("[3] PASS: nina-fw answered; version bytes: "));
        for (uint8_t j = i + 4; j < sizeof(rsp) && rsp[j] >= ' ' &&
                                rsp[j] <= '~';
             j++)
          Serial.print((char)rsp[j]);
        Serial.println();
        break;
      }
    }
    if (!sawStart) {
      bool allFF = true, allZero = true;
      for (uint8_t i = 0; i < sizeof(rsp); i++) {
        if (rsp[i] != 0xFF) allFF = false;
        if (rsp[i] != 0x00) allZero = false;
      }
      if (allFF || allZero)
        Serial.println(F("[3] FAIL: MISO flat — MISO wire (M0 MISO ->"
                         " \"SDA\"/GPIO23) most suspect"));
      else
        Serial.println(F("[3] FAIL: garbage — SCK (M0 SCK -> \"MOSI\"/GPIO18)"
                         " or MOSI (M0 MOSI -> \"14\"/GPIO14) suspect"));
    }
  }
}

}  // namespace

void setup() {
  Serial.begin(115200);
  uint32_t t0 = millis();
  while (!Serial && millis() - t0 < 3000) delay(10);
  Serial.println();
  Serial.println(F("=== HUZZAH32 wire probe (Appendix A.2 rig) ==="));

  pinMode(PIN_CS, OUTPUT);
  digitalWrite(PIN_CS, HIGH);
  pinMode(PIN_RST, OUTPUT);
  digitalWrite(PIN_RST, HIGH);
  pinMode(PIN_READY, INPUT);
  pinMode(PIN_SCK_BB, OUTPUT);
  digitalWrite(PIN_SCK_BB, LOW);  // SPI mode 0 idle
  pinMode(PIN_MOSI_BB, OUTPUT);
  digitalWrite(PIN_MOSI_BB, LOW);
  pinMode(PIN_MISO_BB, INPUT);
}

void loop() {
  probe();
  delay(5000);  // wiggle wires between passes
}
