#ifndef SCOREBOARD_H
#define SCOREBOARD_H

// The physical 7-segment scoreboard: current field values, the active rig
// (SCOREBOARD_CONFIGS entry), the active mode (scores or a test pattern,
// see shared/scoreboard_modes.h) and the thread that drives its shift
// registers.
//
// Callers on any thread only record what they want shown; a dedicated
// output thread (woken by rtos::EventFlags) renders the digits and shifts
// them out. A burst of score updates therefore costs one refresh, and the
// BLE loop never waits on the bit-banged shift registers.
//
// Each rig preset sets its rows' field order (shared/scoreboard_configs.cpp):
// a 9-digit top row (Bat A, Score, Bat B) and a 6-digit bottom row (Overs,
// Wickets and Target, in an order that differs between boards).

#include "shared/score_fields.h"

// Brings up the boot rig ("Play-Cricket", see
// shared/scoreboard_configs.cpp) in MODE_SCOREBOARD showing all zeros --
// pins configured, zeros pushed, and only then outputs enabled, so on a
// board with an OE wire random power-up latch contents are never shown --
// then reports the rig and mode to the M7. Returns once the hardware is up.
// Call once from setup(), after coreLinkBegin().
void scoreboardStart();

// Records a new value for 'field' and schedules a refresh. Values that
// don't fit the field's digits are clamped (negative -> 0, too big -> 9s).
// During a test the value is kept and shown once the test ends.
void scoreboardSetField(ScoreFieldId field, int value);

// Asks for a switch to SCOREBOARD_CONFIGS[index]. The output thread
// releases the old rig's pins, starts the new rig with the current scores
// and reports the rig now active to the M7 -- also when 'index' is out of
// range and nothing changed, so the M7's picker snaps back.
void scoreboardRequestConfig(int index);

// Asks for a switch to ScoreboardMode 'mode': the scores, or a test
// pattern that loops until the next switch (from its first step). The
// output thread reports the mode now active to the M7 -- also when 'mode'
// is out of range and nothing changed, so the M7's picker snaps back.
void scoreboardRequestMode(int mode);

#endif
