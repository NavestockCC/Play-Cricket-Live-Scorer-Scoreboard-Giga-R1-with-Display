#ifndef PINS_AND_CONSTANTS_H
#define PINS_AND_CONSTANTS_H

#include <Arduino.h>

#include "shared/scoreboard_configs.h"

// Index into SCOREBOARD_CONFIGS for the rig currently in use. Owned by the
// M4; changed at runtime when the M7's rig picker requests a switch (see
// switchScoreboardConfig()). Defaults to defaultScoreboardConfigIndex().
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

// One scoreboard field: the current number plus how many physical digits
// it occupies. Stores the number rather than precomputed segment bytes, so
// the bytes are always rendered with the active config's digitTable (see
// update_Scoreboard()) -- switching rigs can't leave stale encodings behind.
struct ScoreField {
  int value;
  int digitLength; // 1..MAX_DISPLAY_NUMBER
};

//Config of the score board
struct ScoreBoard_Layout{
  ScoreField BatA;
  ScoreField BatB;
  ScoreField Score;
  ScoreField Wickets;
  ScoreField Overs;
  ScoreField Target;
};

extern ScoreBoard_Layout ScoreBoard;

void initScoreBoard();

#endif
