#ifndef GIGA_DISPLAY_H
#define GIGA_DISPLAY_H

#include <Arduino_H7_Video.h>
#include "lvgl.h"

// Expose the global hardware driver object
extern Arduino_H7_Video Display;

// Expose card label hooks for external file modification loops
extern lv_obj_t *lbl_batA_val;
extern lv_obj_t *lbl_score_val;
extern lv_obj_t *lbl_batB_val;
extern lv_obj_t *lbl_wickets_val;
extern lv_obj_t *lbl_overs_val;
extern lv_obj_t *lbl_target_val;

// ADDED: Expose the global right-panel sidebar log label hook
extern lv_obj_t *lbl_sidebar_log;

// Core initialization and grid builder functions
void display_setup();
void create_scoreboard_grid();

// ADDED: Expose the rolling history update function
void update_display_card(lv_obj_t *label, const char *new_value);

// Sets a score card label to a single current value (no history), for use
// with lbl_batA_val, lbl_score_val, lbl_batB_val, lbl_wickets_val,
// lbl_overs_val, lbl_target_val. Use update_display_card() only for the
// rolling sidebar log.
void set_card_value(lv_obj_t *label, const char *new_value);

// Applies a score message forwarded by the M4 (raw BLE text: 3-letter
// field code + value, e.g. "BTR142" or "OVB12.3"): sets that field's card
// and logs the message. Unknown codes are ignored.
void show_score_message(const char *message);

// Shows SCOREBOARD_CONFIGS[index] as the selected rig in the picker, as
// reported by the M4 (which owns the active rig).
void show_active_config(int index);

#endif // GIGA_DISPLAY_H

