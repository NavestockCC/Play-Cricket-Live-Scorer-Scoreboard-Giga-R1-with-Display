#ifndef SHIFT_REGISTER_H
#define SHIFT_REGISTER_H

#include <Arduino.h>
#include <mbed.h>
#include <memory>
#include "shared/scoreboard_configs.h"

const byte ALL_SEGMENTS_OFF = 0x00;

// One daisy-chained row of TPIC6B595N shift registers, driven through mbed
// DigitalInOut pins. Not thread-safe: owned by the scoreboard output thread.
class ShiftRegisterChain {
public:
  // Takes the chain's pins, leaving its outputs DISABLED (OE held high
  // from the moment the pin becomes an output, so it never glitches low).
  // Push valid data with push() first, then enableOutputs(), so random
  // power-up/stale latch contents are never shown. Releases any pins
  // attached before.
  void attach(const ShiftRegisterChainConfig& config);

  // Disables the outputs and stops driving the pins (all set to inputs; the
  // external pull-up keeps OE high). Use when switching away from a rig, so
  // its pins don't stay driven by the Giga.
  void release();

  // Enables the chain's outputs (drives active-low OE LOW).
  void enableOutputs();

  // Shifts 'count' bytes from 'values' into the chain, then latches so all
  // its displays update at once. values[0] is sent FIRST, so it ends up in
  // the farthest chip; the last element ends up in Chip 1, nearest the Giga.
  // If 'count' is less than the chain's chip count, blank bytes are sent
  // first so the leftover (farthest) chips are switched off. If it is
  // greater, the first (excess) bytes shift out past the farthest chip.
  void push(const byte values[], int count);

private:
  void shiftByte(byte value);

  int numSegments = 0;
  std::unique_ptr<mbed::DigitalInOut> ser;
  std::unique_ptr<mbed::DigitalInOut> srck;
  std::unique_ptr<mbed::DigitalInOut> rck;
  std::unique_ptr<mbed::DigitalInOut> oe;
};

#endif
