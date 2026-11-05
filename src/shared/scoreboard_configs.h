#ifndef SCOREBOARD_CONFIGS_H
#define SCOREBOARD_CONFIGS_H

// Scoreboard rig presets, shared by both cores: the M4 uses the pins, field
// order and digitTable to drive the shift registers, the M7 uses the names
// for the rig picker on the touchscreen. Both firmwares compile this same
// table, so a preset index means the same rig on either core.

#include <Arduino.h>
#include "score_fields.h"

// Fields per row of the physical scoreboard.
const int FIELDS_PER_ROW = 3;

// oePin value for a row with no output-enable wire: the board ties its
// chips' G pins to ground, so their outputs are always on. The Giga then
// never drives that row's OE pin.
const int NO_PIN = -1;

// Config for one daisy-chained shift-register row (its own independent
// SER/SRCK/RCK/OE lines and digit count). The scoreboard has two such rows
// wired to separate Giga pin sets.
struct ShiftRegisterChainConfig {
  int serPin;      // SER IN -> Chip 1 of this chain
  int srckPin;     // Shift register clock (shared across this chain's chips)
  int rckPin;      // Latch / storage register clock (shared across this chain's chips)
  int oePin;       // Output enable (active-low, pulled up on the board), or NO_PIN
  int numSegments; // Number of chained 7-seg displays/chips on this row

  // The row's fields, left to right as seen from the front. Chip 1 (nearest
  // the Giga) is the leftmost digit.
  ScoreFieldId fields[FIELDS_PER_ROW];
};

// Both rows of the physical scoreboard: a 9-digit top row and a 6-digit
// bottom row.
struct ScoreboardChainsConfig {
  const char* name; // Human-readable rig name, shown in the Giga display picker

  ShiftRegisterChainConfig top;
  ShiftRegisterChainConfig bottom;

  // Segment bytes for digits 0-9. Bit n drives shift-register output n
  // (Drain n); which segment that lights depends on the board's wiring.
  byte digitTable[10];
};

// All available scoreboard rig presets. Add another entry here when a new
// rig (different pins, field order and/or digitTable) needs to be supported.
extern const ScoreboardChainsConfig SCOREBOARD_CONFIGS[];
extern const int SCOREBOARD_CONFIG_COUNT;

// Returns the index of the config named 'name' (exact, case-sensitive
// match), or -1 if none match.
int findScoreboardConfigIndexByName(const char* name);

// Index of the rig used at boot ("Play-Cricket", falling back to the first
// preset if that name isn't found).
int defaultScoreboardConfigIndex();

#endif
