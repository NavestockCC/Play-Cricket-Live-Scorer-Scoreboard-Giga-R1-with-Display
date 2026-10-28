#ifndef BLE_INPUT_H
#define BLE_INPUT_H

#include <Arduino.h>

// Starts BLE advertising as "BTScoreboard" and sets up the writable
// scoreboard characteristic. Call once from setup().
// Returns false if the onboard BLE module failed to initialize.
bool initBleInput();

// Checks for an incoming BLE connection/write and, if new messages
// arrived, parses them, updates the matching ScoreBoard fields, forwards
// them to the M7 display and refreshes the physical display. Call this
// repeatedly from loop(). Returns true if a message was processed this call.
bool handleBleInput();

// Feeds the "slowest loop() iteration since last BLE connect" diagnostic.
// Cheap (a single comparison, no I/O) -- safe to call every loop() pass
// even with no serial monitor attached. Reported (as a debug line to the
// M7) only once, at the moment of a BLE disconnect, to help track down what
// the loop was doing right before a drop without spamming during normal
// operation.
void recordLoopDuration(unsigned long durationMicros);

#endif