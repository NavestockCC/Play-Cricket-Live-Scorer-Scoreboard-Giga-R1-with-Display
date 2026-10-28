#include "pins_and_constants.h"

int activeScoreboardConfigIndex = defaultScoreboardConfigIndex();

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
  ScoreBoard.BatA    = {0, 3};
  ScoreBoard.BatB    = {0, 3};
  ScoreBoard.Score   = {0, 3};
  ScoreBoard.Overs   = {0, 2};
  ScoreBoard.Wickets = {0, 1};
  ScoreBoard.Target  = {0, 3};
}
