#include "shift_register.h"

// Half-period of the bit-banged SRCK/RCK clocks. mbed GPIO writes are fast
// enough to clock the TPIC6B595s at several MHz, too fast for long scoreboard
// cable runs; 1us keeps the edges at roughly the speed Arduino shiftOut()
// produced (~500kHz).
const uint32_t CLOCK_HALF_PERIOD_NS = 1000;

static std::unique_ptr<mbed::DigitalInOut> makeOutput(int arduinoPin, int initialValue) {
  return std::unique_ptr<mbed::DigitalInOut>(new mbed::DigitalInOut(
      digitalPinToPinName(arduinoPin), PIN_OUTPUT, PullNone, initialValue));
}

void ShiftRegisterChain::attach(const ShiftRegisterChainConfig& config) {
  release();
  // G/OE is active-low and pulled up to Rail L: created HIGH (outputs off).
  oe   = makeOutput(config.oePin, 1);
  ser  = makeOutput(config.serPin, 0);
  srck = makeOutput(config.srckPin, 0);
  rck  = makeOutput(config.rckPin, 0);
  numSegments = config.numSegments;
}

void ShiftRegisterChain::release() {
  if (!oe) return;
  oe->write(1); // blank the displays first
  oe->input();  // external pull-up keeps OE high
  ser->input();
  srck->input();
  rck->input();
  oe.reset();
  ser.reset();
  srck.reset();
  rck.reset();
  numSegments = 0;
}

void ShiftRegisterChain::enableOutputs() {
  if (oe) oe->write(0);
}

void ShiftRegisterChain::shiftByte(byte value) {
  for (int bit = 7; bit >= 0; bit--) { // MSB first
    ser->write((value >> bit) & 1);
    wait_ns(CLOCK_HALF_PERIOD_NS);
    srck->write(1);
    wait_ns(CLOCK_HALF_PERIOD_NS);
    srck->write(0);
  }
}

void ShiftRegisterChain::push(const byte values[], int count) {
  if (!oe) return;
  rck->write(0);
  for (int i = count; i < numSegments; i++) shiftByte(ALL_SEGMENTS_OFF);
  for (int i = 0; i < count; i++) shiftByte(values[i]);
  wait_ns(CLOCK_HALF_PERIOD_NS);
  rck->write(1);
  wait_ns(CLOCK_HALF_PERIOD_NS);
}
