#include "scoreboard_configs.h"
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

int findScoreboardConfigIndexByName(const char* name) {
  for (int i = 0; i < SCOREBOARD_CONFIG_COUNT; i++) {
    if (strcmp(SCOREBOARD_CONFIGS[i].name, name) == 0) return i;
  }
  return -1;
}

int defaultScoreboardConfigIndex() {
  int idx = findScoreboardConfigIndexByName("BuildYourOwnScoreboard");
  return idx >= 0 ? idx : 0;
}
