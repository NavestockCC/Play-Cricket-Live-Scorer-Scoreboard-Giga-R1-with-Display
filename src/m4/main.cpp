/*
  Scoreboard firmware (M4 core) - main entry point

  The Giga runs two firmwares, one per core, talking over the core link
  (shared/core_link.h):
  - M4 (this firmware): the physical 7-segment scoreboard (shift
    registers). Shows the scores the M7 forwards and owns the active rig.
  - M7 (src/m7/): BLE input from the Play Cricket app + the Giga Display
    Shield UI -- score cards, log, rig picker. It also boots this core
    (RPC.begin()) and prints our debug lines to USB Serial, since the M4
    has no USB serial of its own. (BLE can't run here: the Giga core's M4
    build has no Bluetooth radio pins.)

  Hardware: two boards, connected one at a time to the same Giga pins, each
  with a preset in shared/scoreboard_configs.cpp (picked on the M7's rig
  picker). Both have TPIC6B595 shift registers, one per digit, chained per
  row (chip1 -> chip2 -> ...), in two independent rows/chains (top and
  bottom), each with its own SER/SRCK/RCK lines.
  - "Breadboard Test" (the project's own board): Wurth 157136V12801
    common-anode 7-segment displays (one per chip), OE wired -- see below.
  - "Play-Cricket" (built to buildyourownscoreboard.wordpress.com): 12V LED
    modules, shift-register outputs 0-6 = segments A-G, G tied to ground
    (no OE wire: leave the Giga's OE wires unconnected), SRCLR tied to 5V.

  Power rails (Breadboard Test):
  - Rail H: external supply for LED anodes (pins 3 & 8 on each display).
    Currently 5V, will be upgraded to 12V later.
  - Rail L: independent 5V+ supply (not derived from Arduino or Rail H).
    Powers TPIC6B595N Vcc (pin 2), SRCLR (pin 8), and pulls up G/OE (pin 9).

  Wiring: pins for each row/chain (OE optional), along with digit counts,
  field order and the segment lookup table, live per-config in
  shared/scoreboard_configs.h/.cpp
  (see ShiftRegisterChainConfig / ScoreboardChainsConfig / SCOREBOARD_CONFIGS).
  Multiple physical rigs can be defined there and switched between at
  runtime from the M7's rig picker -- see scoreboardRequestConfig().

  Threads:
  - loop() thread: sleeps until the M7 sends a line, then applies it.
  - "scoreboard" thread (m4/scoreboard/scoreboard.cpp): renders the digits
    and shifts them out whenever a field or the rig changes.
  - RPC thread (RPC library): queues lines from the M7 for the loop.

  Breadboard Test per-chip wiring (same for both rows):
  - SER IN (Chip 1 only, pin 3)      -> row's SER pin
  - G/OE (pin 9, all chips)          -> row's OE pin via 10k resistor, pulled
                                         up to Rail L (active-low; must be
                                         driven LOW to enable outputs)
  - RCK  (pin 12, all chips)         -> row's RCK pin
  - SRCK (pin 13, all chips)         -> row's SRCK pin
  - Chip N SEROUT (pin 18) -> Chip N+1 SER IN (pin 3)
  - SRCLR (pin 8, all chips)         -> Rail L (tied high; never cleared)
  - GND (pins 10, 11, 19, all chips) -> Star ground

  Breadboard Test segment bit mapping (custom wiring, per digit;
  Play-Cricket uses bit0-6 = A-G):
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
  - shared/scoreboard_configs.h/.cpp    -> per-rig pins, field order, digit tables (both cores)
  - shared/score_fields.h/.cpp          -> the six fields and their BLE codes (both cores)
  - shared/core_link.h/.cpp             -> M4 <-> M7 message link (both cores)
  - shared/line_queue.h/.cpp            -> thread-safe line queue (both cores)
  - m4/scoreboard/shift_register.h/.cpp -> one shift-register chain (mbed GPIO)
  - m4/scoreboard/scoreboard.h/.cpp     -> field values, active rig, output thread
  - m4/main.cpp                         -> setup() and loop() (this file)
  - m7/main.cpp, m7/GigaDisplay/        -> display firmware (M7 core)
*/

#include <Arduino.h>
#include <mbed.h>
#include "scoreboard/scoreboard.h"
#include "shared/core_link.h"
#include "shared/line_queue.h"
#include "shared/score_fields.h"

// Lines received from the M7.
static LineQueue fromM7;

// Applies every line the M7 has sent: score updates from BLE, and rig
// switches from the touchscreen picker.
static void handleCoreLinkInput() {
  char line[CORE_LINK_MAX_LINE];
  while (fromM7.pop(line, sizeof(line))) {
    const char* text = line + 1;
    switch (line[0]) {
      case LINK_MSG_SCORE: {
        ScoreFieldId field = findScoreField(text);
        if (field != FIELD_COUNT) scoreboardSetField(field, parseScoreValue(text));
        break;
      }
      case LINK_MSG_CONFIG:
        // The scoreboard thread always answers with the (possibly
        // unchanged) active rig, so the picker snaps back if the request
        // was rejected.
        scoreboardRequestConfig(atoi(text));
        break;
      default:
        break; // unknown message type, ignore
    }
  }
}

void setup() {
  // Waits for the M7, which boots this core and is ready to receive.
  coreLinkBegin(fromM7);
  coreLinkSend(LINK_MSG_DEBUG, "M4 scoreboard firmware started");

  // Starts the scoreboard thread on the boot rig, showing zeros, and
  // reports the rig to the M7.
  scoreboardStart();
}

void loop() {
  fromM7.waitForLine(osWaitForever);
  handleCoreLinkInput();
}
