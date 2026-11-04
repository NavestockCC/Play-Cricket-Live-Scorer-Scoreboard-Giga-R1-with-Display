#ifndef SCOREBOARD_CONFIGS_H
#define SCOREBOARD_CONFIGS_H

// Scoreboard rig presets, shared by both cores: the M4 uses the pins and
// digitTable to drive the shift registers, the M7 uses the names for the
// rig picker on the touchscreen. Both firmwares compile this same table, so
// a preset index means the same rig on either core.

#include <Arduino.h>

// Config for one daisy-chained shift-register row (its own independent
// SER/SRCK/RCK/OE lines and digit count). The scoreboard now has two such
// rows wired to separate Giga pin sets.
struct ShiftRegisterChainConfig {
  int serPin;      // SER IN -> Chip 1 of this chain
  int srckPin;     // Shift register clock (shared across this chain's chips)
  int rckPin;      // Latch / storage register clock (shared across this chain's chips)
  int oePin;       // Output enable (active-low, pulled up to Rail L via 10k)
  int numSegments; // Number of chained 7-seg displays/chips on this row
};

// Both rows of the physical scoreboard.
// Top: 9 digits. Field mapping (for the future full pipeline wiring):
//   BatA(3) + Score(3) + BatB(3) = 9.
// Bottom: 6 digits. Field mapping (for the future full pipeline wiring):
//   Overs(2) + Wickets(1) + Target(3) = 6.
struct ScoreboardChainsConfig {
  const char* name; // Human-readable rig name, shown in the Giga display picker

  ShiftRegisterChainConfig top;
  ShiftRegisterChainConfig bottom;

  // Precomputed segment bytes for digits 0-9, using this project's
  // custom bit order (bit0=DP, bit1=C, bit2=D, bit3=E, bit4=G, bit5=F, bit6=A, bit7=B)
  byte digitTable[10];
};

// All available scoreboard rig presets. Add another entry here when a new
// rig (different pins and/or digitTable) needs to be supported.
extern const ScoreboardChainsConfig SCOREBOARD_CONFIGS[];
extern const int SCOREBOARD_CONFIG_COUNT;

// Returns the index of the config named 'name' (exact, case-sensitive
// match), or -1 if none match.
int findScoreboardConfigIndexByName(const char* name);

// Index of the rig used at boot ("BuildYourOwnScoreboard", falling back to
// the first preset if that name isn't found).
int defaultScoreboardConfigIndex();

#endif
