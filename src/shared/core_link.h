#ifndef CORE_LINK_H
#define CORE_LINK_H

// Text-line link between the Giga's two cores, over the RPC library's raw
// stream (OpenAMP shared memory). Used by both firmwares:
//
//   M4 (BLE input + scoreboard output)  <---- lines ---->  M7 (Giga display)
//
// Every message is one line: a single type character, then text, then '\n'.
// Lines longer than CORE_LINK_MAX_LINE - 1 characters are truncated.
//
// M4 -> M7
//   'V' + BLE message  A score field changed. The text is the raw BLE message
//                      (3-letter code + value, e.g. "BTR142" or "OVB12.3");
//                      the M7 shows it on that field's card and in the log.
//   'L' + text         Line for the on-screen log (e.g. "BLE Connected").
//   'C' + index        The active rig is SCOREBOARD_CONFIGS[index]. Sent at
//                      boot and in reply to every M7 'C' request (also when
//                      a request was rejected, so the picker snaps back).
//   'D' + text         Debug text; the M7 prints it to USB Serial (the M4
//                      has no USB serial of its own).
//
// M7 -> M4
//   'C' + index        Request a switch to SCOREBOARD_CONFIGS[index].

#include <Arduino.h>

const char LINK_MSG_SCORE  = 'V';
const char LINK_MSG_LOG    = 'L';
const char LINK_MSG_CONFIG = 'C';
const char LINK_MSG_DEBUG  = 'D';

const int CORE_LINK_MAX_LINE = 48; // incl. type char, excl. '\n', incl. '\0'

// Starts the link. On the M7 this also boots the M4 firmware and waits up
// to 5s for it to answer; returns false if it doesn't (e.g. no M4 firmware
// flashed). On the M4 it waits for the M7. Call once from setup().
bool coreLinkBegin();

// Sends one line: 'type' followed by 'text'.
void coreLinkSend(char type, const char* text);

// Sends 'type' followed by the decimal 'value' (e.g. coreLinkSendInt('C', 1)).
void coreLinkSendInt(char type, int value);

// Copies the oldest complete received line (type char + text, no '\n',
// '\0'-terminated) into 'out' and returns true; returns false if no
// complete line is waiting. Call from loop() -- received lines are queued
// by the RPC thread, so callers never touch them from another thread.
bool coreLinkReadLine(char* out, size_t outSize);

// Lines discarded because the receive queue was full (oldest dropped).
unsigned long coreLinkDroppedLines();

#endif
