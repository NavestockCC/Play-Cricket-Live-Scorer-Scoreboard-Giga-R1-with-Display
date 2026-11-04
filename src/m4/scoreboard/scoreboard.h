#ifndef SCOREBOARD_H
#define SCOREBOARD_H

// The physical 7-segment scoreboard: current field values, the active rig
// (SCOREBOARD_CONFIGS entry) and the thread that drives its shift registers.
//
// Callers on any thread only record what they want shown; a dedicated
// output thread (woken by rtos::EventFlags) renders the digits and shifts
// them out. A burst of score updates therefore costs one refresh, and the
// BLE loop never waits on the bit-banged shift registers.
//
// Layout, left-to-right on the hardware:
//   Top row    (9 digits): Bat A(3)  Score(3)   Bat B(3)
//   Bottom row (6 digits): Overs(2)  Wickets(1) Target(3)

#include "shared/score_fields.h"

// Brings up the boot rig ("BuildYourOwnScoreboard", see
// shared/scoreboard_configs.cpp) showing all zeros -- pins configured, zeros
// pushed, and only then outputs enabled, so random power-up latch contents
// are never shown -- then reports the rig to the M7. Returns once the
// hardware is up. Call once from setup(), after coreLinkBegin().
void scoreboardStart();

// Records a new value for 'field' and schedules a refresh. Values that
// don't fit the field's digits are clamped (negative -> 0, too big -> 9s).
void scoreboardSetField(ScoreFieldId field, int value);

// Asks for a switch to SCOREBOARD_CONFIGS[index]. The output thread
// releases the old rig's pins, starts the new rig with the current scores
// and reports the rig now active to the M7 -- also when 'index' is out of
// range and nothing changed, so the M7's picker snaps back.
void scoreboardRequestConfig(int index);

#endif
