#ifndef GIGA_DISPLAY_H
#define GIGA_DISPLAY_H

// Giga Display Shield UI (LVGL): score cards, rolling log, BLE signal chart
// and rig picker.
// LVGL isn't thread-safe: call these only from the thread that runs
// lv_timer_handler() (loop() in m7/main.cpp).

// Starts the display and touch controller and builds the UI.
void display_setup();

// Adds a line to the top of the rolling sidebar log.
void append_log_line(const char *new_value);

// Applies a score message forwarded by the M4 (raw BLE text: 3-letter
// field code + value, e.g. "BTR142" or "OVB12.3"): sets that field's card
// and logs the message. Unknown codes are ignored.
void show_score_message(const char *message);

// Adds a point to the BLE signal chart: the connected phone's signal
// strength (RSSI) in dBm, or a gap for BLE_RSSI_NONE (nothing connected).
// Also shows the reading above the chart -- or, while nothing is
// connected, a red "BLE not connected" block over the chart.
void show_ble_signal(int rssi);

// Shows SCOREBOARD_CONFIGS[index] as the selected rig in the picker, as
// reported by the M4 (which owns the active rig).
void show_active_config(int index);

#endif // GIGA_DISPLAY_H
