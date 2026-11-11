#include "scoreboard_modes.h"

// Same order as ScoreboardMode.
const char* const SCOREBOARD_MODE_NAMES[MODE_COUNT] = {
  "Scoreboard",          // MODE_SCOREBOARD
  "Test: count 0-9",     // MODE_TEST_DIGITS
  "Test: segment walk",  // MODE_TEST_SEGMENTS
  "Test: digit chase",   // MODE_TEST_CHASE
  "Test: all on/off",    // MODE_TEST_ALL_ON_OFF
};
