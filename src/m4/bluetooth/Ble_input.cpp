#include "Ble_input.h"
#include <ArduinoBLE.h>
#include "scoreboard_config/pins_and_constants.h"
#include "scoreboard/display_controller.h"
#include "shared/core_link.h" // display updates go to the M7 over the core link

// This runs on the M4, which has no USB serial: everything meant for the
// screen or the serial monitor is sent to the M7 over the core link
// (see shared/core_link.h) -- 'V'/'L' lines for the display, 'D' lines for
// debug text the M7 prints to USB Serial.
//
// DEBUG_BLE (per-message debug logging) is defined only by the
// "giga_r1_m4_debug" build in platformio.ini -- use it only while actively
// watching the serial monitor on a computer. Without a monitor attached the
// M7's Serial writes can stall; that no longer delays BLE (it's on the other
// core) but it would freeze the touchscreen. The default "giga_r1_m4"
// (release) build leaves it off for real matches.

// BLE service/characteristic UUIDs matching the original Raspberry Pi
const char* SCOREBOARD_SERVICE_UUID = "5a0d6a15-b664-4304-8530-3a0ec53e5bc1";
const char* SCOREBOARD_CHAR_UUID    = "df531f62-fc0b-40ce-81b2-32a6262ea440";

// Max incoming message length.
const int MAX_MESSAGE_LENGTH = 8;

BLEService scoreboardService(SCOREBOARD_SERVICE_UUID);
BLECharacteristic scoreboardChar(SCOREBOARD_CHAR_UUID, BLEWrite, MAX_MESSAGE_LENGTH);

// Disconnect diagnostics -- reset on connect, reported once on disconnect.
// See recordLoopDuration() / the disconnect branch in handleBleInput().
static unsigned long maxLoopDurationMicros = 0;
static unsigned long messagesSinceConnect = 0;
static unsigned long messagesDroppedSinceConnect = 0;

// Incoming-message queue. The characteristic only holds its latest value,
// so polling written() once per loop() would lose a write that arrives
// before the previous one is read. Instead, onScoreboardWritten() copies
// every write in here as it arrives (it runs inside BLE.poll(), on the main
// loop thread, so no locking is needed) and handleBleInput() drains it.
// If the queue is ever full, the OLDEST message is dropped: each message
// carries a field's full current value, so newer ones supersede older ones.
const int MESSAGE_QUEUE_SIZE = 16;
static char messageQueue[MESSAGE_QUEUE_SIZE][MAX_MESSAGE_LENGTH];
static int messageQueueHead = 0;  // next slot to read
static int messageQueueCount = 0;

static void onScoreboardWritten(BLEDevice central, BLECharacteristic characteristic) {
  (void)central;
  if (messageQueueCount == MESSAGE_QUEUE_SIZE) {
    messageQueueHead = (messageQueueHead + 1) % MESSAGE_QUEUE_SIZE;
    messageQueueCount--;
    messagesDroppedSinceConnect++;
  }

  int length = characteristic.valueLength();
  if (length > MAX_MESSAGE_LENGTH - 1) length = MAX_MESSAGE_LENGTH - 1;

  char* slot = messageQueue[(messageQueueHead + messageQueueCount) % MESSAGE_QUEUE_SIZE];
  memcpy(slot, characteristic.value(), length);
  slot[length] = '\0';
  messageQueueCount++;
}

void recordLoopDuration(unsigned long durationMicros) {
  if (durationMicros > maxLoopDurationMicros) maxLoopDurationMicros = durationMicros;
}

bool initBleInput() {
  if (!BLE.begin()) {
    return false;
  }

  BLE.setLocalName("PlayCricketScorer");
  BLE.setDeviceName("PlayCricketScorer");
  BLE.setAdvertisedService(scoreboardService);

  // Widen the connection interval range we advertise (units of 1.25ms --
  // this is 30ms-50ms) so the central has more slack per connection event.
  // Doesn't fix an OS-level background suspend or an RF/interference issue,
  // but gives more headroom against transient timing hiccups that could
  // otherwise trip its supervision timeout.
  BLE.setConnectionInterval(24, 40);

  scoreboardChar.setEventHandler(BLEWritten, onScoreboardWritten);
  scoreboardService.addCharacteristic(scoreboardChar);
  BLE.addService(scoreboardService);

  BLE.advertise();
  
  // Initialize the sidebar log with a clean status message
  coreLinkSend(LINK_MSG_LOG, "BLE Waiting...");
  return true;
}

// Parses only the digits before a decimal point (if any)
int parseWholeNumberBeforeDecimal(const char* text) {
  int result = 0;
  for (int i = 0; text[i] != '\0' && text[i] != '.'; i++) {
    if (text[i] < '0' || text[i] > '9') break;
    result = (result * 10) + (text[i] - '0');
  }
  return result;
}

#ifdef DEBUG_BLE
// Tracks the distinct 3-letter codes seen so far across all received BLE
// messages. Logs the raw message every time, and the growing unique-code
// list whenever a new code shows up -- useful for discovering codes the
// app sends that this firmware doesn't yet recognize/handle.
#define MAX_UNIQUE_CODES 32
static char uniqueCodes[MAX_UNIQUE_CODES][4];
static int uniqueCodeCount = 0;

void logRawMessage(const char* message, const char* code) {
  char line[CORE_LINK_MAX_LINE];
  snprintf(line, sizeof(line), "BLE raw: %s", message);
  coreLinkSend(LINK_MSG_DEBUG, line);

  for (int i = 0; i < uniqueCodeCount; i++) {
    if (strcmp(uniqueCodes[i], code) == 0) return; // already seen
  }
  if (uniqueCodeCount >= MAX_UNIQUE_CODES) return;

  strcpy(uniqueCodes[uniqueCodeCount], code);
  uniqueCodeCount++;

  // One code per line keeps every line well under CORE_LINK_MAX_LINE.
  snprintf(line, sizeof(line), "New BLE code #%d: %s", uniqueCodeCount, code);
  coreLinkSend(LINK_MSG_DEBUG, line);
}
#endif

// Parses one incoming message of the form <3-letter code><value>, updates
// the matching ScoreBoard field and forwards the message to the M7 for the
// on-screen card and log. Returns true if the message was recognized (the
// caller then refreshes the physical display).
bool processMessage(const char* message) {
  if (strlen(message) < 4) return false; // need at least a 3-letter code + 1 digit

  char code[4];
  memcpy(code, message, 3);
  code[3] = '\0'; // null-terminate so strcmp() reads exactly these 3 characters

#ifdef DEBUG_BLE
  logRawMessage(message, code);
#endif

  const char* valueText = message + 3; // everything after the 3-letter code

  if (strcmp(code, "B1S") == 0) { // BatA
    int value = atoi(valueText);
    ScoreBoard.BatA.value = value;

  } else if (strcmp(code, "B2S") == 0) { // BatB
    int value = atoi(valueText);
    ScoreBoard.BatB.value = value;

  } else if (strcmp(code, "BTR") == 0) { // Score
    int value = atoi(valueText);
    ScoreBoard.Score.value = value;

  } else if (strcmp(code, "OVB") == 0) { // Overs -- value may include a decimal (e.g. "12.3")
    int value = parseWholeNumberBeforeDecimal(valueText);
    ScoreBoard.Overs.value = value;

  } else if (strcmp(code, "BTW") == 0) { // Wickets
    int value = atoi(valueText);
    ScoreBoard.Wickets.value = value;

  } else if (strcmp(code, "BTT") == 0) { // Target
    int value = atoi(valueText);
    ScoreBoard.Target.value = value;

  } else {
    return false; // unrecognized code, ignore entirely
  }

  // The M7 shows the full text on the field's card (e.g. overs "12.3") and
  // logs the raw message, e.g. "• BTR142".
  coreLinkSend(LINK_MSG_SCORE, message);
  return true;
}


// Non-blocking: checked once per loop() pass. Drains every queued message,
// then refreshes the physical display once. Returns true if any BLE message
// was processed this call.
bool handleBleInput() {
  static bool wasConnected = false;

  BLEDevice central = BLE.central();
  bool isConnected = central && central.connected();

  if (isConnected && !wasConnected) {
    // Append a connection notification alert to your display ledger
    coreLinkSend(LINK_MSG_LOG, "BLE Connected");
    maxLoopDurationMicros = 0;
    messagesSinceConnect = 0;
    messagesDroppedSinceConnect = 0;
  } else if (!isConnected && wasConnected) {
    // Append a disconnection alert notice when the device goes out of range
    coreLinkSend(LINK_MSG_LOG, "BLE Disconnected");

    // One-off diagnostic (not per-message, so safe even with no monitor
    // attached) to help spot what the loop was doing right before a drop.
    char line[CORE_LINK_MAX_LINE];
    snprintf(line, sizeof(line), "BLE drop: %lu msgs, %lu lost, slowest %lu us",
             messagesSinceConnect, messagesDroppedSinceConnect, maxLoopDurationMicros);
    coreLinkSend(LINK_MSG_DEBUG, line);

    // Defensive: explicitly resume advertising so the board is always
    // reconnectable right away, without relying on the BLE stack resuming
    // advertising on its own after a disconnect.
    BLE.advertise();
  }
  wasConnected = isConnected;

  if (messageQueueCount == 0) return false;

  bool scoreChanged = false;
  while (messageQueueCount > 0) {
    if (processMessage(messageQueue[messageQueueHead])) scoreChanged = true;
    messageQueueHead = (messageQueueHead + 1) % MESSAGE_QUEUE_SIZE;
    messageQueueCount--;
    messagesSinceConnect++;
  }

  // Refresh physical hardware shift register 7-segment output segments
  if (scoreChanged) update_Scoreboard();
  return true;
}
