#include "Ble_input.h"
#include <ArduinoBLE.h>
#include "Ble_hci.h"
#include "shared/core_link.h" // scores go to the M4 over the core link
#include "shared/score_fields.h"

// BLE runs on the M7: the Giga core's M4 build has no Bluetooth radio pins
// (its mbed target leaves the CYW4343W UART/power pins unconnected), so
// BLE.begin() can't work there.
//
// Everything this thread wants shown or printed is posted to the UI inbox
// and handled on the loop() thread -- LVGL and Serial are never touched
// from here, so a stalled Serial write can't hold up BLE.
//
// DEBUG_BLE (per-message debug logging) is defined only by the
// "giga_r1_m7_debug" build in platformio.ini -- use it only while actively
// watching the serial monitor on a computer. Without a monitor attached the
// M7's Serial writes can stall, which would freeze the touchscreen. The
// default "giga_r1_m7" (release) build leaves it off for real matches.

// BLE service/characteristic UUIDs matching the original Raspberry Pi
const char* SCOREBOARD_SERVICE_UUID = "5a0d6a15-b664-4304-8530-3a0ec53e5bc1";
const char* SCOREBOARD_CHAR_UUID    = "df531f62-fc0b-40ce-81b2-32a6262ea440";

// Max incoming message length.
const int MAX_MESSAGE_LENGTH = 8;

// Longest the BLE thread sleeps between checks when nothing arrives. Any
// BLE event (write, connect, disconnect) wakes it immediately.
const uint32_t BLE_WAIT_TIMEOUT_MS = 50;

// How often the connected phone's signal strength is posted to the UI's
// signal chart (one chart point per post).
const uint32_t RSSI_REPORT_INTERVAL_MS = 1000;

BLEService scoreboardService(SCOREBOARD_SERVICE_UUID);
BLECharacteristic scoreboardChar(SCOREBOARD_CHAR_UUID, BLEWrite, MAX_MESSAGE_LENGTH);

// Same priority as the UI (loop()) thread. When both are busy the RTOS gives
// each a 5 ms turn, so ArduinoBLE's busy-waits (it has several, e.g. while
// the radio answers a command) can't freeze the screen, and LVGL rendering
// can't hold up BLE for long.
static rtos::Thread bleThread(osPriorityNormal, 8192, nullptr, "ble");
static LineQueue* uiQueue = nullptr;

// Disconnect diagnostics -- reset on connect, reported once on disconnect.
// See the disconnect branch in handleBleInput().
static unsigned long maxWorkDurationMicros = 0;
static unsigned long messagesSinceConnect = 0;
static unsigned long messagesDroppedSinceConnect = 0;
static unsigned long flushedAtConnect = 0;  // bleHciFlushedPackets() then
static unsigned long timedOutAtConnect = 0; // bleHciTimedOutPackets() then

// Incoming-message queue. The characteristic only holds its latest value,
// so polling written() once per pass would lose a write that arrives
// before the previous one is read. Instead, onScoreboardWritten() copies
// every write in here as it arrives (it runs inside BLE.poll(), on the BLE
// thread like handleBleInput(), so no locking is needed) and
// handleBleInput() drains it. If the queue is ever full, the OLDEST message
// is dropped: each message carries a field's full current value, so newer
// ones supersede older ones.
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

static bool initBle() {
  if (!BLE.begin()) {
    return false;
  }

  BLE.setLocalName("PlayCricketScorer");
  BLE.setDeviceName("PlayCricketScorer");
  BLE.setAdvertisedService(scoreboardService);

  // Ask the phone for a 15 ms connection interval (units of 1.25 ms). Each
  // score message takes about two connection events (the phone's write, then
  // our reply), so this roughly halves how long a ball's updates take at a
  // phone's usual 30 ms. Exactly 15 ms is one of the settings iPhones accept.
  // (ArduinoBLE only asks if the phone's interval is outside this range.)
  BLE.setConnectionInterval(12, 12);

  scoreboardChar.setEventHandler(BLEWritten, onScoreboardWritten);
  scoreboardService.addCharacteristic(scoreboardChar);
  BLE.addService(scoreboardService);

  BLE.advertise();
  return true;
}

#ifdef DEBUG_BLE
// Tracks the distinct 3-letter codes seen so far across all received BLE
// messages. Logs the raw message every time, and the growing unique-code
// list whenever a new code shows up -- useful for discovering codes the
// app sends that this firmware doesn't yet recognize/handle.
#define MAX_UNIQUE_CODES 32
static char uniqueCodes[MAX_UNIQUE_CODES][SCORE_CODE_LENGTH + 1];
static int uniqueCodeCount = 0;

static void logRawMessage(const char* message) {
  char line[CORE_LINK_MAX_LINE];
  snprintf(line, sizeof(line), "BLE raw: %s", message);
  uiQueue->post(LINK_MSG_DEBUG, line);

  if (strlen(message) < (size_t)SCORE_CODE_LENGTH) return;
  char code[SCORE_CODE_LENGTH + 1];
  memcpy(code, message, SCORE_CODE_LENGTH);
  code[SCORE_CODE_LENGTH] = '\0';

  for (int i = 0; i < uniqueCodeCount; i++) {
    if (strcmp(uniqueCodes[i], code) == 0) return; // already seen
  }
  if (uniqueCodeCount >= MAX_UNIQUE_CODES) return;

  strcpy(uniqueCodes[uniqueCodeCount], code);
  uniqueCodeCount++;

  // One code per line keeps every line well under CORE_LINK_MAX_LINE.
  snprintf(line, sizeof(line), "New BLE code #%d: %s", uniqueCodeCount, code);
  uiQueue->post(LINK_MSG_DEBUG, line);
}
#endif

// Sends one recognised message of the form <3-letter code><value> to the
// M4 for the physical scoreboard and to the UI for the card and log.
// Unrecognized messages are ignored.
static void processMessage(const char* message) {
#ifdef DEBUG_BLE
  logRawMessage(message);
#endif

  if (findScoreField(message) == FIELD_COUNT) return;

  coreLinkSend(LINK_MSG_SCORE, message);
  uiQueue->post(LINK_MSG_SCORE, message);
}

// Handles a connect/disconnect and drains every queued message. Called once
// per pass of the BLE thread, right after BLE.poll().
static void handleBleInput() {
  static bool wasConnected = false;

  BLEDevice central = BLE.central();
  bool isConnected = central && central.connected();

  if (isConnected && !wasConnected) {
    uiQueue->post(LINK_MSG_LOG, "BLE Connected");
    maxWorkDurationMicros = 0;
    messagesSinceConnect = 0;
    messagesDroppedSinceConnect = 0;
    flushedAtConnect = bleHciFlushedPackets();
    timedOutAtConnect = bleHciTimedOutPackets();
  } else if (!isConnected && wasConnected) {
    uiQueue->post(LINK_MSG_LOG, "BLE Disconnected");

    // One-off diagnostic (not per-message, so safe even with no monitor
    // attached) to help spot what the thread was doing right before a drop.
    char line[CORE_LINK_MAX_LINE];
    snprintf(line, sizeof(line), "BLE drop: %lu msgs, %lu lost, slowest %lu us",
             messagesSinceConnect, messagesDroppedSinceConnect, maxWorkDurationMicros);
    uiQueue->post(LINK_MSG_DEBUG, line);

    // Replies to the phone that never went out -- each one would have
    // eventually hung unpatched ArduinoBLE (see Ble_hci.cpp).
    unsigned long unsent = bleHciFlushedPackets() - flushedAtConnect;
    unsigned long timedOut = bleHciTimedOutPackets() - timedOutAtConnect;
    if (unsent > 0 || timedOut > 0) {
      snprintf(line, sizeof(line), "BLE drop: %lu replies unsent, %lu timed out",
               unsent, timedOut);
      uiQueue->post(LINK_MSG_DEBUG, line);
    }

    // Defensive: explicitly resume advertising so the board is always
    // reconnectable right away, without relying on the BLE stack resuming
    // advertising on its own after a disconnect.
    BLE.advertise();
  }
  wasConnected = isConnected;

  while (messageQueueCount > 0) {
    processMessage(messageQueue[messageQueueHead]);
    messageQueueHead = (messageQueueHead + 1) % MESSAGE_QUEUE_SIZE;
    messageQueueCount--;
    messagesSinceConnect++;
  }
}

// Posts the connected phone's signal strength (RSSI, in dBm) for the UI's
// signal chart, every RSSI_REPORT_INTERVAL_MS. Keeps posting BLE_RSSI_NONE
// while nothing is connected, so the chart keeps moving and shows the gap.
static void reportSignalStrength() {
  static uint32_t lastReportMs = 0;
  uint32_t now = millis();
  if (now - lastReportMs < RSSI_REPORT_INTERVAL_MS) return;
  lastReportMs = now;

  // Asks the radio for the connection's RSSI and waits for the answer (about
  // 2 ms). BLE_RSSI_NONE straight away if there's no connection.
  char text[8];
  snprintf(text, sizeof(text), "%d", BLE.rssi());
  uiQueue->post(LINK_MSG_RSSI, text);
}

static void bleThreadMain() {
  if (!initBle()) {
    // The board still runs (display + scoreboard), just without BLE input.
    uiQueue->post(LINK_MSG_LOG, "BLE failed to start");
    return;
  }
  uiQueue->post(LINK_MSG_LOG, "BLE Waiting...");
  uiQueue->post(LINK_MSG_DEBUG, "BLE started, advertising as PlayCricketScorer");

  while (true) {
    // Sleeps on the Cordio HCI transport's event flags until the BLE
    // controller sends something (or the timeout passes), then processes it.
    BLE.poll(BLE_WAIT_TIMEOUT_MS);

    unsigned long workStartMicros = micros();
    handleBleInput();
    unsigned long workMicros = micros() - workStartMicros;
    if (workMicros > maxWorkDurationMicros) maxWorkDurationMicros = workMicros;

    // Not timed above: it waits on the radio, while "slowest" is meant to
    // catch slow message handling.
    reportSignalStrength();
  }
}

void bleStart(LineQueue& uiInbox) {
  uiQueue = &uiInbox;
  bleThread.start(bleThreadMain);
}
