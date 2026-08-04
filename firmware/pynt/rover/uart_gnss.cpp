/*
  Adapted from Uart.cpp, Copyright (c) 2015 Arduino LLC (LGPL 2.1+),
  Adafruit SAMD core. RTS/CTS support dropped — the F9P link doesn't
  use flow control. See uart_gnss.h for why this is vendored.
*/

#include "uart_gnss.h"

#include "wiring_private.h"

GnssUart::GnssUart(SERCOM* _s, uint8_t _pinRX, uint8_t _pinTX,
                   SercomRXPad _padRX, SercomUartTXPad _padTX) {
  sercom = _s;
  uc_pinRX = _pinRX;
  uc_pinTX = _pinTX;
  uc_padRX = _padRX;
  uc_padTX = _padTX;
}

void GnssUart::begin(unsigned long baudrate) { begin(baudrate, SERIAL_8N1); }

void GnssUart::begin(unsigned long baudrate, uint16_t config) {
  pinPeripheral(uc_pinRX, g_APinDescription[uc_pinRX].ulPinType);
  pinPeripheral(uc_pinTX, g_APinDescription[uc_pinTX].ulPinType);

  sercom->initUART(UART_INT_CLOCK, SAMPLE_RATE_x16, baudrate);
  sercom->initFrame(extractCharSize(config), LSB_FIRST, extractParity(config),
                    extractNbStopBit(config));
  sercom->initPads(uc_padTX, uc_padRX);

  sercom->enableUART();
}

void GnssUart::end() {
  sercom->resetUART();
  rxBuffer.clear();
  txBuffer.clear();
}

void GnssUart::flush() {
  while (txBuffer.available())
    ;  // wait until TX buffer is empty

  sercom->flushUART();
}

void GnssUart::IrqHandler() {
  if (sercom->isFrameErrorUART()) {
    // frame error, next byte is invalid so read and discard it
    sercom->readDataUART();

    sercom->clearFrameErrorUART();
  }

  if (sercom->availableDataUART()) {
    rxBuffer.store_char(sercom->readDataUART());
  }

  if (sercom->isDataRegisterEmptyUART()) {
    if (txBuffer.available()) {
      uint8_t data = txBuffer.read_char();

      sercom->writeDataUART(data);
    } else {
      sercom->disableDataRegisterEmptyInterruptUART();
    }
  }

  if (sercom->isUARTError()) {
    sercom->acknowledgeUARTError();
    sercom->clearStatusUART();
  }
}

int GnssUart::available() { return rxBuffer.available(); }

int GnssUart::availableForWrite() { return txBuffer.availableForStore(); }

int GnssUart::peek() { return rxBuffer.peek(); }

int GnssUart::read() { return rxBuffer.read_char(); }

size_t GnssUart::write(const uint8_t data) {
  if (sercom->isDataRegisterEmptyUART() && txBuffer.available() == 0) {
    sercom->writeDataUART(data);
  } else {
    // spin lock until a spot opens up in the buffer
    while (txBuffer.isFull()) {
      uint8_t interruptsEnabled = ((__get_PRIMASK() & 0x1) == 0);

      if (interruptsEnabled) {
        uint32_t exceptionNumber = (SCB->ICSR & SCB_ICSR_VECTACTIVE_Msk);

        if (exceptionNumber == 0 ||
            NVIC_GetPriority((IRQn_Type)(exceptionNumber - 16)) >
                SERCOM_NVIC_PRIORITY) {
          // no exception or called from an ISR with lower priority,
          // wait for free buffer spot via IRQ
          continue;
        }
      }

      // interrupts are disabled or called from ISR with higher or equal
      // priority than the SERCOM IRQ — manually call the UART IRQ
      // handler when the data register is empty
      if (sercom->isDataRegisterEmptyUART()) {
        IrqHandler();
      }
    }

    txBuffer.store_char(data);

    sercom->enableDataRegisterEmptyInterruptUART();
  }

  return 1;
}

SercomNumberStopBit GnssUart::extractNbStopBit(uint16_t config) {
  switch (config & HARDSER_STOP_BIT_MASK) {
    case HARDSER_STOP_BIT_1:
    default:
      return SERCOM_STOP_BIT_1;

    case HARDSER_STOP_BIT_2:
      return SERCOM_STOP_BITS_2;
  }
}

SercomUartCharSize GnssUart::extractCharSize(uint16_t config) {
  switch (config & HARDSER_DATA_MASK) {
    case HARDSER_DATA_5:
      return UART_CHAR_SIZE_5_BITS;

    case HARDSER_DATA_6:
      return UART_CHAR_SIZE_6_BITS;

    case HARDSER_DATA_7:
      return UART_CHAR_SIZE_7_BITS;

    case HARDSER_DATA_8:
    default:
      return UART_CHAR_SIZE_8_BITS;
  }
}

SercomParityMode GnssUart::extractParity(uint16_t config) {
  switch (config & HARDSER_PARITY_MASK) {
    case HARDSER_PARITY_NONE:
    default:
      return SERCOM_NO_PARITY;

    case HARDSER_PARITY_EVEN:
      return SERCOM_EVEN_PARITY;

    case HARDSER_PARITY_ODD:
      return SERCOM_ODD_PARITY;
  }
}
