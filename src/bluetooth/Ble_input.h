#ifndef BLE_INPUT_H
#define BLE_INPUT_H

#include <Arduino.h>

// Starts BLE advertising as "BTScoreboard" and sets up the writable
// scoreboard characteristic. Call once from setup().
// Returns false if the onboard BLE module failed to initialize.
bool initBleInput();

// Checks for an incoming BLE connection/write and, if a new message
// arrived, parses it and updates the matching ScoreBoard field, then
// refreshes the physical display. Call this repeatedly from loop().
// Returns true if a message was processed this call (see loop()'s use of
// this to skip that pass's LVGL redraw and prioritize BLE draining).
bool handleBleInput();

// Feeds the "slowest loop() iteration since last BLE connect" diagnostic.
// Cheap (a single comparison, no I/O) -- safe to call every loop() pass
// even with no serial monitor attached. Reported to Serial only once, at
// the moment of a BLE disconnect, to help track down what the loop was
// doing right before a drop (e.g. a slow screen redraw) without spamming
// Serial during normal operation.
void recordLoopDuration(unsigned long durationMicros);

#endif