#ifndef DISPLAY_CONTROLLER_H
#define DISPLAY_CONTROLLER_H


#include "scoreboard_config/pins_and_constants.h"

//Calculates the Max number for any display
int MaxValue(int numSegments);

// Calculates segment bytes for 'number', using exactly 'numSegments' digits.
// Uses the active config's digitTable (see getActiveScoreboardConfig()), so
// results reflect whichever scoreboard rig is currently selected.
// Physical wiring: Arduino -> Chip 1 (nearest, highest place value) -> ... -> Chip N (farthest, units)
// outValues[0] = units digit (sent first, ends up in farthest chip)
// outValues[numSegments-1] = highest place value digit (sent last, ends up in Chip 1, nearest Arduino)
DigitToBytes calculateNumberToBytes(int number, int numSegments);

// Copies 'source' (length sourceLength) into 'destination', starting at
// 'destination[startIndex]'. Returns the next free index in destination
// (useful for chaining multiple copies one after another).
int appendBytes(byte destination[], int startIndex, byte source[], int sourceLength);

// Update the scoreboard with the latest values stored in the ScoreBoard
// variable, pushing to both physical rows (top/bottom) of the active config.
// Segment bytes are rendered here from the stored numbers, using the active
// config's digitTable.
void update_Scoreboard ();

// Brings up the active config's two chains: configures their pins (outputs
// still disabled), pushes the current ScoreBoard values, then enables the
// outputs. Call from setup() after initScoreBoard().
void startScoreboardHardware();

// Switches the active rig to SCOREBOARD_CONFIGS[index]: releases the old
// rig's pins, then starts the new rig via startScoreboardHardware(). Returns
// false (and changes nothing) if 'index' is out of range.
bool switchScoreboardConfig(int index);

// True if the ScoreBoard field layout (BatA+Score+BatB on top,
// Overs+Wickets+Target on the bottom) uses exactly as many digits as each
// of 'config's chains has chips. A mismatch won't overrun any buffer, but
// digits would land on the wrong chips.
bool scoreboardLayoutMatches(const ScoreboardChainsConfig& config);

#endif
