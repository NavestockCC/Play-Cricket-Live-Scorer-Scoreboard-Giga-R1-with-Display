#ifndef SHIFT_REGISTER_H
#define SHIFT_REGISTER_H

#include <Arduino.h>
#include "scoreboard_config/pins_and_constants.h"

// Sets pin modes for one shift-register chain, leaving its outputs DISABLED
// (OE held high). Push valid data with pushToShiftRegisters() first, then
// call enableShiftRegisterOutputs(), so random power-up/stale latch contents
// are never shown.
void setupShiftRegisterChain(const ShiftRegisterChainConfig& chain);

// Enables the chain's outputs (drives active-low OE LOW).
void enableShiftRegisterOutputs(const ShiftRegisterChainConfig& chain);

// Disables the chain's outputs and stops driving its pins (all set to
// INPUT; the external pull-up keeps OE high). Use when switching away from
// a rig, so its pins don't stay driven by the Giga.
void releaseShiftRegisterChain(const ShiftRegisterChainConfig& chain);

// Pushes 'count' bytes from 'values' out to that chain's daisy-chained shift
// registers, then latches so all connected displays update simultaneously.
// Because of the shift-register chain, values[0] is sent FIRST (so it ends
// up in the farthest chip in the chain) and the LAST element is sent last
// (so it ends up in Chip 1, the chip closest to the Arduino).
// If 'count' is less than chain.numSegments, blank bytes are sent first so
// the leftover (farthest) chips are switched off. If it is greater, the
// first (excess) bytes shift out past the farthest chip. Only 'count' bytes
// are ever read from 'values'.
void pushToShiftRegisters(const ShiftRegisterChainConfig& chain, const byte values[], int count);

#endif
