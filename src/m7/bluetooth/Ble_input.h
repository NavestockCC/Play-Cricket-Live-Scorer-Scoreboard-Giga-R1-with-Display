#ifndef BLE_INPUT_H
#define BLE_INPUT_H

#include "shared/line_queue.h"

// Starts the BLE thread: advertises as "PlayCricketScorer" with the
// writable scoreboard characteristic, and for every recognised score
// message sends it to the M4 (physical scoreboard) and posts it to
// 'uiInbox' (score cards + log). Connection status and failures are posted
// to 'uiInbox' as log ('L') and debug ('D') lines, and the connected phone's
// signal strength as an 'R' line once a second. Call once from setup().
//
// All ArduinoBLE calls happen on that one thread, as the library requires.
void bleStart(LineQueue& uiInbox);

#endif
