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
void update_Scoreboard ();

#endif
