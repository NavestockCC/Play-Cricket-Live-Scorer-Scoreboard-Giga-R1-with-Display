/*
  Scoreboard sketch - main entry point

  Hardware:
  - TPIC6B595N shift registers, one per digit, chained per row (chip1 ->
    chip2 -> ...). The scoreboard has two independent rows/chains (top and
    bottom), each with its own SER/SRCK/RCK/OE lines.
  - Wurth 157136V12801 common-anode 7-segment displays (one per chip)

  Power rails:
  - Rail H: external supply for LED anodes (pins 3 & 8 on each display).
    Currently 5V, will be upgraded to 12V later.
  - Rail L: independent 5V+ supply (not derived from Arduino or Rail H).
    Powers TPIC6B595N Vcc (pin 2), SRCLR (pin 8), and pulls up G/OE (pin 9).

  Wiring: pins for each row/chain, along with digit counts and the segment
  lookup table, live per-config in scoreboard_config/pins_and_constants.h/.cpp
  (see ShiftRegisterChainConfig / ScoreboardChainsConfig / SCOREBOARD_CONFIGS).
  Multiple physical rigs can be defined there and switched between at
  runtime -- see getActiveScoreboardConfig() / setActiveScoreboardConfigByName().

  Per-chip wiring pattern (same for both rows):
  - SER IN (Chip 1 only, pin 3)      -> row's SER pin
  - G/OE (pin 9, all chips)          -> row's OE pin via 10k resistor, pulled
                                         up to Rail L (active-low; must be
                                         driven LOW to enable outputs)
  - RCK  (pin 12, all chips)         -> row's RCK pin
  - SRCK (pin 13, all chips)         -> row's SRCK pin
  - Chip N SEROUT (pin 18) -> Chip N+1 SER IN (pin 3)
  - SRCLR (pin 8, all chips)         -> Rail L (tied high; never cleared)
  - GND (pins 10, 11, 19, all chips) -> Star ground

  Segment bit mapping (custom wiring, per digit):
    bit0 = DP   (DRAIN0)
    bit1 = C    (DRAIN1)
    bit2 = D    (DRAIN2)
    bit3 = E    (DRAIN3)
    bit4 = G    (DRAIN4)
    bit5 = F    (DRAIN5)
    bit6 = A    (DRAIN6)
    bit7 = B    (DRAIN7)

  Since the TPIC6B595 outputs sink current (active-high bit = segment ON
  for a common-anode display), a bit value of 1 turns that segment on.

  File layout:
  - scoreboard_config/pins_and_constants.h/.cpp -> per-rig pin/digit-table configs, ScoreBoard struct
  - scoreboard/shift_register.h/.cpp            -> low-level shift register push, per chain
  - scoreboard/display_controller.h/.cpp        -> number-to-segment-byte calculation, update_Scoreboard()
  - bluetooth/ble_input.h/.cpp                  -> BLE "BTScoreboard" input, parses incoming scores
  - GigaDisplay/Giga_Display.h/.cpp             -> LVGL UI, incl. the config picker dropdown
  - scoreboard_sketch.ino                       -> setup() and loop() (this file)
*/

#include "scoreboard_config/pins_and_constants.h"
#include "scoreboard/shift_register.h"
#include "scoreboard/display_controller.h"
#include "bluetooth/ble_input.h"
#include "GigaDisplay/Giga_Display.h"
#include "lvgl.h" 

// Tracking variable for non-blocking timing loop
unsigned long last_lvgl_tick = 0;

void setup() {
  Serial.begin(115200);

  // Initialise Giga Display (Sets up hardware and hooks up the LVGL canvas)
  display_setup();

  Serial.print("Starting Setup!!");

  // Configure the pins for whichever scoreboard rig is currently active
  // (defaults to "BuildYourOwnScoreboard" -- see pins_and_constants.cpp).
  const ScoreboardChainsConfig& activeConfig = getActiveScoreboardConfig();
  setupShiftRegisterChain(activeConfig.top);
  setupShiftRegisterChain(activeConfig.bottom);

  // initialise the scoreboard
  initScoreBoard();
  update_Scoreboard();
  delay(2000); // Guard delay for physical hardware initialization

  if (!initBleInput()) {
    Serial.print("BLE failed to start");
    // BLE failed to start -- board will still run, just without
    // Bluetooth input. Consider flashing an error pattern on the
    // display here if you want this to be visible without a serial monitor.
  } else {
    Serial.print("BLE started sucessfully");
  }
  
  last_lvgl_tick = millis();
}

void loop() {
  unsigned long loopStartMicros = micros();

  // 1. Listens for BLE score updates and refreshes the physical shift registers.
  // Prioritized ahead of the display refresh below.
  bool bleMessageProcessed = handleBleInput();

  // 2. Safely feed processing time slices to the LVGL graphical interface engine.
  // Skipped for this pass if a BLE message was just processed, so the loop
  // goes straight back to checking for more incoming BLE data instead of
  // waiting behind a screen redraw -- keeps bursts (e.g. an innings change
  // sending several fields back-to-back) from stalling BLE servicing.
  // Otherwise runs at most every 5 milliseconds to keep rendering responsive.
  if (!bleMessageProcessed && millis() - last_lvgl_tick >= 5) {
    last_lvgl_tick = millis();
    lv_task_handler();
  }

  // Tracks the slowest loop() iteration since the last BLE connect, so a
  // disconnect can be correlated with what the loop was doing (e.g. a slow
  // screen redraw) -- see recordLoopDuration() / handleBleInput().
  recordLoopDuration(micros() - loopStartMicros);
}
