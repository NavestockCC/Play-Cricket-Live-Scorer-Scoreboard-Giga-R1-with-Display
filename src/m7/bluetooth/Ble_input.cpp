#include "Ble_input.h"
#include <ArduinoBLE.h>
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

BLEService scoreboardService(SCOREBOARD_SERVICE_UUID);
BLECharacteristic scoreboardChar(SCOREBOARD_CHAR_UUID, BLEWrite, MAX_MESSAGE_LENGTH);

// Above the UI thread, so LVGL rendering never delays BLE.
static rtos::Thread bleThread(osPriorityAboveNormal, 8192, nullptr, "ble");
static LineQueue* uiQueue = nullptr;

// Disconnect diagnostics -- reset on connect, reported once on disconnect.
// See the disconnect branch in handleBleInput().
static unsigned long maxWorkDurationMicros = 0;
static unsigned long messagesSinceConnect = 0;
static unsigned long messagesDroppedSinceConnect = 0;

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
  } else if (!isConnected && wasConnected) {
    uiQueue->post(LINK_MSG_LOG, "BLE Disconnected");

    // One-off diagnostic (not per-message, so safe even with no monitor
    // attached) to help spot what the thread was doing right before a drop.
    char line[CORE_LINK_MAX_LINE];
    snprintf(line, sizeof(line), "BLE drop: %lu msgs, %lu lost, slowest %lu us",
             messagesSinceConnect, messagesDroppedSinceConnect, maxWorkDurationMicros);
    uiQueue->post(LINK_MSG_DEBUG, line);

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
  }
}

void bleStart(LineQueue& uiInbox) {
  uiQueue = &uiInbox;
  bleThread.start(bleThreadMain);
}
