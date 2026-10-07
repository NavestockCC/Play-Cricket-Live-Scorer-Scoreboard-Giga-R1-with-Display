#include "ble_input.h"
#include <ArduinoBLE.h>
#include "scoreboard_config/pins_and_constants.h"
#include "scoreboard/display_controller.h"
#include "GigaDisplay/Giga_Display.h" // ADDED: Required to access display labels and update_display_card

// Enabled only while actively watching the serial monitor on a computer.
// Left on during standalone/USB-power-only operation, every BLE message
// triggers a Serial.print() with nothing to drain the USB-CDC buffer --
// on this board that can stall the loop long enough to blow through the
// BLE supervision timeout, which shows up as random disconnects during
// active scoring. Turn this back off before a real match.
#define DEBUG_BLE

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

  scoreboardService.addCharacteristic(scoreboardChar);
  BLE.addService(scoreboardService);

  BLE.advertise();
  
  // Initialize the sidebar log with a clean status message
  update_display_card(lbl_sidebar_log, "BLE Waiting...");
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
  Serial.print("BLE raw: ");
  Serial.println(message);

  for (int i = 0; i < uniqueCodeCount; i++) {
    if (strcmp(uniqueCodes[i], code) == 0) return; // already seen
  }
  if (uniqueCodeCount >= MAX_UNIQUE_CODES) return;

  strcpy(uniqueCodes[uniqueCodeCount], code);
  uniqueCodeCount++;

  Serial.print("Unique codes so far (");
  Serial.print(uniqueCodeCount);
  Serial.print("): ");
  for (int i = 0; i < uniqueCodeCount; i++) {
    Serial.print(uniqueCodes[i]);
    if (i < uniqueCodeCount - 1) Serial.print(", ");
  }
  Serial.println();
}
#endif

// Parses one incoming message of the form <3-letter code><value>
void processMessage(const char* message, int length) {
  if (length < 4) return; // need at least a 3-letter code + 1 digit

  char code[4];
  memcpy(code, message, 3);
  code[3] = '\0'; // null-terminate so strcmp() reads exactly these 3 characters

#ifdef DEBUG_BLE
  logRawMessage(message, code);
#endif

  const char* valueText = message + 3; // everything after the 3-letter code

  if (strcmp(code, "B1S") == 0) { // BatA
    int value = atoi(valueText);
    ScoreBoard.BatA = calculateNumberToBytes(value, ScoreBoard.BatA.digitLength);
    set_card_value(lbl_batA_val, valueText);

  } else if (strcmp(code, "B2S") == 0) { // BatB
    int value = atoi(valueText);
    ScoreBoard.BatB = calculateNumberToBytes(value, ScoreBoard.BatB.digitLength);
    set_card_value(lbl_batB_val, valueText);

  } else if (strcmp(code, "BTR") == 0) { // Score
    int value = atoi(valueText);
    ScoreBoard.Score = calculateNumberToBytes(value, ScoreBoard.Score.digitLength);
    set_card_value(lbl_score_val, valueText);

  } else if (strcmp(code, "OVB") == 0) { // Overs -- value may include a decimal (e.g. "12.3")
    int value = parseWholeNumberBeforeDecimal(valueText);
    ScoreBoard.Overs = calculateNumberToBytes(value, ScoreBoard.Overs.digitLength);
    set_card_value(lbl_overs_val, valueText); // full text incl. decimal, e.g. "12.3"

  } else if (strcmp(code, "BTW") == 0) { // Wickets
    int value = atoi(valueText);
    ScoreBoard.Wickets = calculateNumberToBytes(value, ScoreBoard.Wickets.digitLength);
    set_card_value(lbl_wickets_val, valueText);

  } else if (strcmp(code, "BTT") == 0) { // Target
    int value = atoi(valueText);
    ScoreBoard.Target = calculateNumberToBytes(value, ScoreBoard.Target.digitLength);
    set_card_value(lbl_target_val, valueText);

  } else {
    return; // unrecognized code, ignore entirely
  }

  // UPDATED: Push the raw, entire Bluetooth message string directly to the log console
  // Instead of logging "Score: 142", it logs the actual packet stream, e.g., "• BTR142"
  update_display_card(lbl_sidebar_log, message);

  // Refresh physical hardware shift register 7-segment output segments
  update_Scoreboard();
}


// Non-blocking: checked once per loop() pass. Returns true if a BLE message
// was processed this call, so the caller can skip that pass's LVGL redraw
// and prioritize draining any further incoming BLE traffic instead (see
// scoreboard_sketch.ino's loop()).
bool handleBleInput() {
  static bool wasConnected = false;

  BLEDevice central = BLE.central();
  bool isConnected = central && central.connected();

  if (isConnected && !wasConnected) {
    // Append a connection notification alert to your display ledger
    update_display_card(lbl_sidebar_log, "BLE Connected");
    maxLoopDurationMicros = 0;
    messagesSinceConnect = 0;
  } else if (!isConnected && wasConnected) {
    // Append a disconnection alert notice when the device goes out of range
    update_display_card(lbl_sidebar_log, "BLE Disconnected");

    // One-off diagnostic (not per-message, so safe even with no monitor
    // attached) to help spot what the loop was doing right before a drop.
    Serial.print("BLE Disconnected. Messages this session: ");
    Serial.print(messagesSinceConnect);
    Serial.print(", slowest loop() iteration: ");
    Serial.print(maxLoopDurationMicros);
    Serial.println(" us");

    // Defensive: explicitly resume advertising so the board is always
    // reconnectable right away, without relying on the BLE stack resuming
    // advertising on its own after a disconnect.
    BLE.advertise();
  }
  wasConnected = isConnected;

  if (isConnected && scoreboardChar.written()) {
    int length = scoreboardChar.valueLength();
    if (length > MAX_MESSAGE_LENGTH - 1) length = MAX_MESSAGE_LENGTH - 1;

    char buffer[MAX_MESSAGE_LENGTH];
    memcpy(buffer, scoreboardChar.value(), length);
    buffer[length] = '\0';

    processMessage(buffer, length);
    messagesSinceConnect++;
    return true;
  }

  return false;
}
