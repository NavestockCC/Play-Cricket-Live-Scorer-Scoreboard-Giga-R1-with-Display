#include "pins_and_constants.h"
#include "scoreboard/display_controller.h"
#include <string.h>

const ScoreboardChainsConfig SCOREBOARD_CONFIGS[] = {
  {
    .name = "BreadboardTest",
    .top = {
      .serPin      = 8,  // White
      .srckPin     = 11, // Yellow
      .rckPin      = 10, // Green
      .oePin       = 9,  // Blue (active-low, via 10k pull-up to Rail L)
      .numSegments = 9,
    },
    .bottom = {
      .serPin      = 5, // White
      .srckPin     = 7, // Yellow
      .rckPin      = 6, // Green
      .oePin       = 4, // Blue (active-low, via 10k pull-up to Rail L)
      .numSegments = 6,
    },
    .digitTable = {
      0xEE, // 0
      0x82, // 1
      0xDC, // 2
      0xD6, // 3
      0xB2, // 4
      0x76, // 5
      0x7E, // 6
      0xC2, // 7
      0xFE, // 8
      0xF6  // 9
    },
  },
  {
    .name = "BuildYourOwnScoreboard",
    .top = {
      .serPin      = 8,  // White
      .srckPin     = 11, // Yellow
      .rckPin      = 10, // Green
      .oePin       = 9,  // Blue (active-low, via 10k pull-up to Rail L)
      .numSegments = 9,
    },
    .bottom = {
      .serPin      = 5, // White
      .srckPin     = 7, // Yellow
      .rckPin      = 6, // Green
      .oePin       = 4, // Blue (active-low, via 10k pull-up to Rail L)
      .numSegments = 6,
    },
    .digitTable = {
      0xEE, // 0
      0x82, // 1
      0xDC, // 2
      0xD6, // 3
      0xB2, // 4
      0x76, // 5
      0x7E, // 6
      0xC2, // 7
      0xFE, // 8
      0xF6  // 9
    },
  },
};



const int SCOREBOARD_CONFIG_COUNT = sizeof(SCOREBOARD_CONFIGS) / sizeof(SCOREBOARD_CONFIGS[0]);

// Returns the index of the config named 'name', or -1 if none match.
static int findScoreboardConfigIndexByName(const char* name) {
  for (int i = 0; i < SCOREBOARD_CONFIG_COUNT; i++) {
    if (strcmp(SCOREBOARD_CONFIGS[i].name, name) == 0) return i;
  }
  return -1;
}

// Resolves the default active config by name, falling back to the first
// config if the named default isn't found.
static int initialActiveScoreboardConfigIndex() {
  int idx = findScoreboardConfigIndexByName("BuildYourOwnScoreboard");
  return idx >= 0 ? idx : 0;
}

int activeScoreboardConfigIndex = initialActiveScoreboardConfigIndex();

const ScoreboardChainsConfig& getActiveScoreboardConfig() {
  return SCOREBOARD_CONFIGS[activeScoreboardConfigIndex];
}

bool setActiveScoreboardConfigByName(const char* name) {
  int idx = findScoreboardConfigIndexByName(name);
  if (idx < 0) return false;
  activeScoreboardConfigIndex = idx;
  return true;
}



const byte ALL_SEGMENTS_ON  = 0xFF;
const byte ALL_SEGMENTS_OFF = 0x00;


//Initialise Setup
ScoreBoard_Layout ScoreBoard;

void initScoreBoard() {
  ScoreBoard.BatA.digitLength = 3;
  ScoreBoard.BatA = calculateNumberToBytes(0, ScoreBoard.BatA.digitLength);
  ScoreBoard.BatB.digitLength = 3;
  ScoreBoard.BatB = calculateNumberToBytes(0, ScoreBoard.BatB.digitLength);
  ScoreBoard.Score.digitLength = 3;
  ScoreBoard.Score = calculateNumberToBytes(0, ScoreBoard.Score.digitLength);
  ScoreBoard.Overs.digitLength = 2;
  ScoreBoard.Overs = calculateNumberToBytes(0, ScoreBoard.Overs.digitLength);
  ScoreBoard.Wickets.digitLength = 1;
  ScoreBoard.Wickets = calculateNumberToBytes(0, ScoreBoard.Wickets.digitLength);
  ScoreBoard.Target.digitLength  = 3;
  ScoreBoard.Target = calculateNumberToBytes(0, ScoreBoard.Target.digitLength);
}


