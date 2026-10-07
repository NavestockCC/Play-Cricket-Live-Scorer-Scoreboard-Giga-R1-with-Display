#ifndef PINS_AND_CONSTANTS_H
#define PINS_AND_CONSTANTS_H

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
  const char* name; // Human-readable rig name, for a future Giga display picker

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

// Index into SCOREBOARD_CONFIGS for the rig currently in use. Deliberately
// non-const -- intended to be changed at runtime later (e.g. from a Giga
// display selection menu). Defaults to "BuildYourOwnScoreboard" (see .cpp).
extern int activeScoreboardConfigIndex;

// Returns the currently selected scoreboard config.
const ScoreboardChainsConfig& getActiveScoreboardConfig();

// Selects the active config by its ScoreboardChainsConfig::name (exact,
// case-sensitive match). Returns false and leaves the active config
// unchanged if no config with that name exists.
bool setActiveScoreboardConfigByName(const char* name);

// largest number of digits needed to display a single number(batA) on the scoreboard
const int MAX_DISPLAY_NUMBER = 3;

extern const byte ALL_SEGMENTS_ON;  // A,B,C,D,E,F,G,DP all on
extern const byte ALL_SEGMENTS_OFF;

struct DigitToBytes {
  byte values[MAX_DISPLAY_NUMBER]; // values[0]=units, values[1]=tens, values[2]=hundreds
  int digitLength;                 // how many of the values[] slots are actually used
};

//Config of the score board
struct ScoreBoard_Layout{
  DigitToBytes BatA;
  DigitToBytes BatB;
  DigitToBytes Score;
  DigitToBytes Wickets;
  DigitToBytes Overs;
  DigitToBytes Target;
};

extern ScoreBoard_Layout ScoreBoard;

void initScoreBoard();

#endif
