#ifndef SHIFT_REGISTER_H
#define SHIFT_REGISTER_H

#include <Arduino.h>
#include "scoreboard_config/pins_and_constants.h"

// Sets pin modes for one shift-register chain and enables its outputs
// (drives OE low). Call once per chain -- from setup() for the chain(s)
// active at boot, and again for a chain that becomes newly active if the
// scoreboard config is switched at runtime.
void setupShiftRegisterChain(const ShiftRegisterChainConfig& chain);

// Pushes 'chain.numSegments' bytes from 'values' out to that chain's
// daisy-chained shift registers, then latches so all connected displays
// update simultaneously. Because of the shift-register chain, values[0] is
// sent FIRST (so it ends up in the farthest chip in the chain) and the
// LAST element is sent last (so it ends up in Chip 1, the chip closest to
// the Arduino). 'values' must contain at least chain.numSegments bytes.
void pushToShiftRegisters(const ShiftRegisterChainConfig& chain, byte values[]);

#endif
