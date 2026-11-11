#ifndef TEST_PATTERNS_H
#define TEST_PATTERNS_H

// The test modes' patterns (see shared/scoreboard_modes.h): each one is a
// loop of steps, rendered per row in the active rig's segment wiring.

#include <Arduino.h>
#include "shared/scoreboard_configs.h"
#include "shared/scoreboard_modes.h"

// Number of steps in one loop of test 'mode' on 'config' (1 for
// MODE_SCOREBOARD, which isn't a test).
int testPatternSteps(ScoreboardMode mode, const ScoreboardChainsConfig& config);

// Renders step 'step' of test 'mode' for a row of 'chips' chips into 'out',
// in ShiftRegisterChain::push() order (out[0] ends up in the farthest chip,
// out[chips - 1] in Chip 1). Returns 'chips'.
int renderTestRow(ScoreboardMode mode, int step, const ScoreboardChainsConfig& config,
                  int chips, byte out[]);

#endif
