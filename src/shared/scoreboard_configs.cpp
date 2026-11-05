#include "scoreboard_configs.h"
#include <string.h>

// Both boards use the same Giga pins (only one is connected at a time), so
// the cables just swap over -- except the OE wires, see Play-Cricket.
const ScoreboardChainsConfig SCOREBOARD_CONFIGS[] = {
  {
    // Built to buildyourownscoreboard.wordpress.com: shift-register outputs
    // 0-6 drive segments A-G (output 7 unused, no DP). The chips' G pins are
    // tied to ground, so there's no OE wire: leave the Giga's OE wires
    // (D9, D4) unconnected on this board.
    .name = "Play-Cricket",
    .top = {
      .serPin      = 8,  // White
      .srckPin     = 11, // Yellow
      .rckPin      = 10, // Green
      .oePin       = NO_PIN,
      .numSegments = 9,
      .fields      = {FIELD_BAT_A, FIELD_SCORE, FIELD_BAT_B},
    },
    .bottom = {
      .serPin      = 5, // White
      .srckPin     = 7, // Yellow
      .rckPin      = 6, // Green
      .oePin       = NO_PIN,
      .numSegments = 6,
      .fields      = {FIELD_WICKETS, FIELD_OVERS, FIELD_TARGET},
    },
    // Bit order: bit0=A, bit1=B, bit2=C, bit3=D, bit4=E, bit5=F, bit6=G
    .digitTable = {
      0x3F, // 0
      0x06, // 1
      0x5B, // 2
      0x4F, // 3
      0x66, // 4
      0x6D, // 5
      0x7D, // 6
      0x07, // 7
      0x7F, // 8
      0x6F  // 9 (with the bottom bar, like Breadboard Test)
    },
  },
  {
    // The project's own board: TPIC6B595N chips driving 157136V12801
    // common-anode displays, OE wired to the Giga.
    .name = "Breadboard Test",
    .top = {
      .serPin      = 8,  // White
      .srckPin     = 11, // Yellow
      .rckPin      = 10, // Green
      .oePin       = 9,  // Blue (active-low, via 10k pull-up to Rail L)
      .numSegments = 9,
      .fields      = {FIELD_BAT_A, FIELD_SCORE, FIELD_BAT_B},
    },
    .bottom = {
      .serPin      = 5, // White
      .srckPin     = 7, // Yellow
      .rckPin      = 6, // Green
      .oePin       = 4, // Blue (active-low, via 10k pull-up to Rail L)
      .numSegments = 6,
      .fields      = {FIELD_OVERS, FIELD_WICKETS, FIELD_TARGET},
    },
    // Bit order: bit0=DP, bit1=C, bit2=D, bit3=E, bit4=G, bit5=F, bit6=A, bit7=B
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
  int idx = findScoreboardConfigIndexByName("Play-Cricket");
  return idx >= 0 ? idx : 0;
}
