/*
  Display firmware (M7 core) - main entry point

  The Giga runs two firmwares, one per core, talking over the core link
  (shared/core_link.h):
  - M7 (this firmware): BLE input from the Play Cricket app, and the Giga
    Display Shield UI -- score cards, rolling log, BLE signal chart and rig
    picker (LVGL + touch). Forwards every score to the M4, boots the M4 and
    prints the M4's debug lines to USB Serial.
  - M4 (src/m4/): the physical 7-segment scoreboard (shift registers). Owns
    the active rig; this core shows what the M4 reports.

  BLE has to live on this core: the Giga core's M4 build has no Bluetooth
  radio pins, so BLE.begin() can't work there.

  File layout:
  - m7/bluetooth/Ble_input.h/.cpp       -> BLE "PlayCricketScorer" input thread
  - m7/bluetooth/Ble_hci.h/.cpp         -> fix for a hang in ArduinoBLE
  - m7/GigaDisplay/Giga_Display.h/.cpp   -> LVGL UI, incl. the rig picker
  - m7/GigaDisplay/scoreboard_lv_conf.h  -> LVGL configuration
  - shared/                              -> rig presets, fields, core link (both cores)
  - m7/main.cpp                          -> setup() and loop() (this file)

  Threads:
  - loop() thread: everything LVGL (UI updates and lv_timer_handler()) and
    Serial. Sleeps on 'uiInbox' between LVGL timers, so it wakes as soon as
    the M4 or the BLE thread posts something.
  - "ble" thread (m7/bluetooth/Ble_input.cpp): all ArduinoBLE calls. Same
    priority as loop(): the RTOS time-slices them, so neither can freeze
    the other.
  - RPC thread (RPC library): queues lines from the M4 into 'uiInbox'.
  - Arduino_H7_Video's thread: feeds LVGL's tick.
*/

#include <Arduino.h>
#include "lvgl.h"
#include "GigaDisplay/Giga_Display.h"
#include "bluetooth/Ble_input.h"
#include "shared/core_link.h"
#include "shared/line_queue.h"

// Bounds on how long loop() sleeps between LVGL passes: at least 1ms so a
// busy UI can't starve other threads, at most 30ms so touch input (polled
// by an LVGL timer) stays responsive.
const uint32_t MIN_UI_SLEEP_MS = 1;
const uint32_t MAX_UI_SLEEP_MS = 30;

// Everything for the UI: lines from the M4 (core link) and from the BLE
// thread, in the core link's message format.
static LineQueue uiInbox;

// Applies every line posted since the last pass. Runs on the loop thread,
// so it's safe to touch LVGL objects and Serial here.
static void handleUiInbox() {
  char line[CORE_LINK_MAX_LINE];
  while (uiInbox.pop(line, sizeof(line))) {
    const char* text = line + 1;
    switch (line[0]) {
      case LINK_MSG_SCORE:  show_score_message(text); break;
      case LINK_MSG_LOG:    append_log_line(text); break;
      case LINK_MSG_CONFIG: show_active_config(atoi(text)); break;
      case LINK_MSG_RSSI:   show_ble_signal(atoi(text)); break;
      case LINK_MSG_DEBUG:
        Serial.println(text);
        break;
      default:
        break; // unknown message type, ignore
    }
  }
}

void setup() {
  Serial.begin(115200);

  // Initialise Giga Display (Sets up hardware and hooks up the LVGL canvas)
  display_setup();

  // Boots the M4 (scoreboard) and waits up to 5s for it to answer. BLE and
  // the display still run without it.
  if (!coreLinkBegin(uiInbox)) {
    append_log_line("WARN: M4 not responding");
    Serial.println("M4 did not start -- is the M4 firmware flashed?");
  }

  bleStart(uiInbox);
}

void loop() {
  // 1. Scores, log lines and rig updates from the BLE thread and the M4.
  handleUiInbox();

  // 2. Run LVGL's due timers (rendering, touch input); it returns how long
  // until the next one is due.
  uint32_t untilNextTimerMs = lv_timer_handler();

  // 3. Sleep until then, or until something is posted to the inbox.
  uiInbox.waitForLine(constrain(untilNextTimerMs, MIN_UI_SLEEP_MS, MAX_UI_SLEEP_MS));
}
