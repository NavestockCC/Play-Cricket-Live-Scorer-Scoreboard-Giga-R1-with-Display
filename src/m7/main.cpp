/*
  Display firmware (M7 core) - main entry point

  The Giga runs two firmwares, one per core, talking over the core link
  (shared/core_link.h):
  - M7 (this firmware): Giga Display Shield UI -- score cards, rolling log
    and rig picker (LVGL + touch). Boots the M4 and prints the M4's debug
    lines to USB Serial.
  - M4 (src/m4/): BLE input from the Play Cricket app + the physical
    7-segment scoreboard. Owns the scores and the active rig; this core only
    displays what the M4 reports.

  File layout:
  - m7/GigaDisplay/Giga_Display.h/.cpp   -> LVGL UI, incl. the rig picker
  - m7/GigaDisplay/scoreboard_lv_conf.h  -> LVGL configuration
  - shared/                              -> rig presets + core link (both cores)
  - m7/main.cpp                          -> setup() and loop() (this file)
*/

#include <Arduino.h>
#include "lvgl.h"
#include "GigaDisplay/Giga_Display.h"
#include "shared/core_link.h"

// Tracking variable for non-blocking timing loop
unsigned long last_lvgl_tick = 0;

// Applies every message the M4 has sent since the last pass. Runs on the
// loop thread, so it's safe to touch LVGL objects here.
static void handleCoreLinkInput() {
  char line[CORE_LINK_MAX_LINE];
  while (coreLinkReadLine(line, sizeof(line))) {
    const char* text = line + 1;
    switch (line[0]) {
      case LINK_MSG_SCORE:  show_score_message(text); break;
      case LINK_MSG_LOG:    update_display_card(lbl_sidebar_log, text); break;
      case LINK_MSG_CONFIG: show_active_config(atoi(text)); break;
      case LINK_MSG_DEBUG:
        Serial.print("[M4] ");
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

  // Boots the M4 (BLE + scoreboard) and waits up to 5s for it to answer.
  if (!coreLinkBegin()) {
    update_display_card(lbl_sidebar_log, "WARN: M4 not responding");
    Serial.println("M4 did not start -- is the M4 firmware flashed?");
  }

  last_lvgl_tick = millis();
}

void loop() {
  // 1. Score/log/rig updates from the M4.
  handleCoreLinkInput();

  // 2. Feed processing time slices to the LVGL graphical interface engine,
  // at most every 5 milliseconds to keep rendering responsive.
  if (millis() - last_lvgl_tick >= 5) {
    last_lvgl_tick = millis();
    lv_task_handler();
  }
}
