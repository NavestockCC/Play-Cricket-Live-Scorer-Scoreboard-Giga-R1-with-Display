/*
  Scoreboard firmware (M4 core) - main entry point

  The Giga runs two firmwares, one per core, talking over the core link
  (shared/core_link.h):
  - M4 (this firmware): BLE input from the Play Cricket app + the physical
    7-segment scoreboard (shift registers). Owns the scores and the active rig.
  - M7 (src/m7/): Giga Display Shield UI -- score cards, log, rig picker.
    It also boots this core (RPC.begin()) and prints our debug lines to USB
    Serial, since the M4 has no USB serial of its own.

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
  lookup table, live per-config in shared/scoreboard_configs.h/.cpp
  (see ShiftRegisterChainConfig / ScoreboardChainsConfig / SCOREBOARD_CONFIGS).
  Multiple physical rigs can be defined there and switched between at
  runtime from the M7's rig picker -- see switchScoreboardConfig().

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
  - shared/scoreboard_configs.h/.cpp               -> per-rig pin/digit-table presets (both cores)
  - shared/core_link.h/.cpp                        -> M4 <-> M7 message link (both cores)
  - m4/scoreboard_config/pins_and_constants.h/.cpp -> active rig, ScoreBoard struct
  - m4/scoreboard/shift_register.h/.cpp            -> low-level shift register push, per chain
  - m4/scoreboard/display_controller.h/.cpp        -> number-to-segment-byte calculation, update_Scoreboard()
  - m4/bluetooth/Ble_input.h/.cpp                  -> BLE "PlayCricketScorer" input, parses incoming scores
  - m4/main.cpp                                    -> setup() and loop() (this file)
  - m7/main.cpp, m7/GigaDisplay/                   -> display firmware (M7 core)
*/

#include <Arduino.h>
#include "scoreboard_config/pins_and_constants.h"
#include "scoreboard/display_controller.h"
#include "bluetooth/Ble_input.h"
#include "shared/core_link.h"

// Tells the M7 which rig is active (so its picker shows it), and warns on
// its log if that rig's chip counts don't match the digit layout.
static void reportActiveConfig() {
  const ScoreboardChainsConfig& activeConfig = getActiveScoreboardConfig();
  coreLinkSendInt(LINK_MSG_CONFIG, activeScoreboardConfigIndex);

  char message[CORE_LINK_MAX_LINE];
  snprintf(message, sizeof(message), "Config: %s", activeConfig.name);
  coreLinkSend(LINK_MSG_LOG, message);

  if (!scoreboardLayoutMatches(activeConfig)) {
    coreLinkSend(LINK_MSG_LOG, "WARN: digit layout mismatch");
  }
}

// Handles requests from the M7 (currently only rig switches from the picker).
static void handleCoreLinkInput() {
  char line[CORE_LINK_MAX_LINE];
  while (coreLinkReadLine(line, sizeof(line))) {
    if (line[0] == LINK_MSG_CONFIG) {
      // Always answer with the (possibly unchanged) active rig, so the
      // picker snaps back if the request was out of range.
      switchScoreboardConfig(atoi(line + 1));
      reportActiveConfig();
    }
  }
}

void setup() {
  // Waits for the M7, which boots this core and is ready to receive.
  coreLinkBegin();
  coreLinkSend(LINK_MSG_DEBUG, "M4 scoreboard firmware started");

  // initialise the scoreboard values, then bring up whichever rig is
  // currently active (defaults to "BuildYourOwnScoreboard" -- see
  // shared/scoreboard_configs.cpp): pins configured, zeros pushed, and only
  // then outputs enabled, so random power-up latch contents are never shown.
  initScoreBoard();
  startScoreboardHardware();
  reportActiveConfig();
  delay(2000); // Guard delay for physical hardware initialization

  if (!initBleInput()) {
    coreLinkSend(LINK_MSG_LOG, "BLE failed to start");
    // BLE failed to start -- board will still run, just without
    // Bluetooth input.
  } else {
    coreLinkSend(LINK_MSG_DEBUG, "BLE started successfully");
  }
}

void loop() {
  unsigned long loopStartMicros = micros();

  // Listens for BLE score updates and refreshes the physical shift registers.
  handleBleInput();

  // Rig switch requests from the M7's touchscreen picker.
  handleCoreLinkInput();

  // Tracks the slowest loop() iteration since the last BLE connect, so a
  // disconnect can be correlated with what the loop was doing -- see
  // recordLoopDuration() / handleBleInput().
  recordLoopDuration(micros() - loopStartMicros);
}
