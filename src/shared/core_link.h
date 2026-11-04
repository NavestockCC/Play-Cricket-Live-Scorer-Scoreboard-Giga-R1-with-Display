#ifndef CORE_LINK_H
#define CORE_LINK_H

// Text-line link between the Giga's two cores, over the RPC library's raw
// stream (OpenAMP shared memory). Used by both firmwares:
//
//   M7 (BLE input + Giga display)  <---- lines ---->  M4 (scoreboard output)
//
// Every message is one line: a single type character, then text, then '\n'.
// Lines longer than CORE_LINK_MAX_LINE - 1 characters are truncated.
//
// M7 -> M4
//   'V' + BLE message  A score field changed. The text is the raw BLE message
//                      (3-letter code + value, e.g. "BTR142" or "OVB12.3");
//                      the M4 shows it on the physical scoreboard.
//   'C' + index        Request a switch to SCOREBOARD_CONFIGS[index].
//
// M4 -> M7
//   'C' + index        The active rig is SCOREBOARD_CONFIGS[index]. Sent at
//                      boot and in reply to every M7 'C' request (also when
//                      a request was rejected, so the picker snaps back).
//   'L' + text         Line for the on-screen log (e.g. "Config: ...").
//   'D' + text         Debug text; the M7 prints it to USB Serial (the M4
//                      has no USB serial of its own).
//
// The M7 also uses these types for lines its BLE thread posts to the UI
// (see m7/main.cpp): 'V' for scores, 'L' for log lines, 'D' for debug. Plus
// one UI-only type that never crosses the link:
//   'R' + dBm          Signal strength (RSSI) of the connected phone, once a
//                      second, for the signal chart. BLE_RSSI_NONE when
//                      nothing is connected.

#include <Arduino.h>
#include "line_queue.h"

const char LINK_MSG_SCORE  = 'V';
const char LINK_MSG_LOG    = 'L';
const char LINK_MSG_CONFIG = 'C';
const char LINK_MSG_DEBUG  = 'D';
const char LINK_MSG_RSSI   = 'R'; // M7 only, see above

// 'R' value for "no reading" -- ArduinoBLE's own, what BLE.rssi() returns
// when nothing is connected.
const int BLE_RSSI_NONE = 127;

const int CORE_LINK_MAX_LINE = MAX_LINE_LENGTH; // incl. type char, excl. '\n', incl. '\0'

// Starts the link; every complete line received from the other core is
// pushed to 'inbox' (from the RPC thread). On the M7 this also boots the M4
// firmware and waits up to 5s for it to answer; returns false if it doesn't
// (e.g. no M4 firmware flashed). On the M4 it waits for the M7. Call once
// from setup().
bool coreLinkBegin(LineQueue& inbox);

// Sends one line: 'type' followed by 'text'. Safe from any thread (the
// RPC library serialises writes).
void coreLinkSend(char type, const char* text);

// Sends 'type' followed by the decimal 'value' (e.g. coreLinkSendInt('C', 1)).
void coreLinkSendInt(char type, int value);

#endif
