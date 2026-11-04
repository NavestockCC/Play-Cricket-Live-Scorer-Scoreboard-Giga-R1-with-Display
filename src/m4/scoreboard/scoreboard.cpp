#include "scoreboard.h"
#include <Arduino.h>
#include <mbed.h>
#include <string.h>
#include "shift_register.h"
#include "shared/core_link.h"
#include "shared/scoreboard_configs.h"

// Fields on each row, in on-screen (left-to-right) order.
const int FIELDS_PER_ROW = 3;
static const ScoreFieldId TOP_ROW[FIELDS_PER_ROW]    = {FIELD_BAT_A, FIELD_SCORE, FIELD_BAT_B};
static const ScoreFieldId BOTTOM_ROW[FIELDS_PER_ROW] = {FIELD_OVERS, FIELD_WICKETS, FIELD_TARGET};

// Requests from other threads, guarded by 'stateMutex'. The output thread
// copies them out under the lock and does the slow hardware work without it.
static rtos::Mutex stateMutex;
static int fieldValues[FIELD_COUNT] = {0};
static int requestedConfigIndex = 0; // valid while CONFIG_FLAG is set

static rtos::EventFlags outputFlags;
const uint32_t REFRESH_FLAG = 0x01;
const uint32_t CONFIG_FLAG  = 0x02;

// Released once the boot rig is up (see scoreboardStart()).
static rtos::Semaphore startedSemaphore(0, 1);

// Owned by the output thread only.
static rtos::Thread outputThread(osPriorityAboveNormal, 4096, nullptr, "scoreboard");
static ShiftRegisterChain topChain;
static ShiftRegisterChain bottomChain;
static int activeConfigIndex = 0;

// Renders 'value' as 'digits' segment bytes into 'out', units digit first
// (it has to be shifted out first to end up in the farthest chip -- see
// ShiftRegisterChain::push()). Leading zeros are blanked; the units digit
// is always shown, even when it's 0. Returns 'digits'.
static int renderNumber(int value, int digits, const byte digitTable[10], byte out[]) {
  int maxValue = 1;
  for (int i = 0; i < digits; i++) maxValue *= 10;
  value = constrain(value, 0, maxValue - 1);

  for (int i = 0; i < digits; i++) {
    bool blank = (i > 0 && value == 0);
    out[i] = blank ? ALL_SEGMENTS_OFF : digitTable[value % 10];
    value /= 10;
  }
  return digits;
}

// Renders one row into 'out' and returns the byte count. The chain reverses
// the order it's fed (first byte -> farthest chip), so fields are rendered
// right-to-left and each field units-first: the rightmost field's units
// digit goes first, the leftmost field's highest digit goes last and lands
// in Chip 1, nearest the Giga.
static int renderRow(const ScoreFieldId row[FIELDS_PER_ROW], const int values[FIELD_COUNT],
                     const byte digitTable[10], byte out[]) {
  int count = 0;
  for (int i = FIELDS_PER_ROW - 1; i >= 0; i--) {
    ScoreFieldId field = row[i];
    count += renderNumber(values[field], SCORE_FIELDS[field].digits, digitTable, out + count);
  }
  return count;
}

static void pushValues(const int values[FIELD_COUNT]) {
  const ScoreboardChainsConfig& config = SCOREBOARD_CONFIGS[activeConfigIndex];
  byte rowBytes[FIELDS_PER_ROW * MAX_FIELD_DIGITS];

  int count = renderRow(TOP_ROW, values, config.digitTable, rowBytes);
  topChain.push(rowBytes, count);

  count = renderRow(BOTTOM_ROW, values, config.digitTable, rowBytes);
  bottomChain.push(rowBytes, count);
}

// Takes over the active rig's pins and shows 'values' on it.
static void startActiveRig(const int values[FIELD_COUNT]) {
  const ScoreboardChainsConfig& config = SCOREBOARD_CONFIGS[activeConfigIndex];
  topChain.attach(config.top);
  bottomChain.attach(config.bottom);
  pushValues(values);
  topChain.enableOutputs();
  bottomChain.enableOutputs();
}

static int rowDigits(const ScoreFieldId row[FIELDS_PER_ROW]) {
  int digits = 0;
  for (int i = 0; i < FIELDS_PER_ROW; i++) digits += SCORE_FIELDS[row[i]].digits;
  return digits;
}

// Tells the M7 which rig is active (so its picker shows it), and warns on
// its log if that rig's chip counts don't match the field layout -- nothing
// overruns, but digits would land on the wrong chips.
static void reportActiveConfig() {
  const ScoreboardChainsConfig& config = SCOREBOARD_CONFIGS[activeConfigIndex];
  coreLinkSendInt(LINK_MSG_CONFIG, activeConfigIndex);

  char message[CORE_LINK_MAX_LINE];
  snprintf(message, sizeof(message), "Config: %s", config.name);
  coreLinkSend(LINK_MSG_LOG, message);

  if (rowDigits(TOP_ROW) != config.top.numSegments ||
      rowDigits(BOTTOM_ROW) != config.bottom.numSegments) {
    coreLinkSend(LINK_MSG_LOG, "WARN: digit layout mismatch");
  }
}

static void outputThreadMain() {
  int values[FIELD_COUNT] = {0};
  activeConfigIndex = defaultScoreboardConfigIndex();
  startActiveRig(values);
  reportActiveConfig();
  startedSemaphore.release();

  while (true) {
    uint32_t flags = outputFlags.wait_any(REFRESH_FLAG | CONFIG_FLAG);

    stateMutex.lock();
    memcpy(values, fieldValues, sizeof(values));
    int requested = requestedConfigIndex;
    stateMutex.unlock();

    if (flags & CONFIG_FLAG) {
      if (requested >= 0 && requested < SCOREBOARD_CONFIG_COUNT) {
        topChain.release();
        bottomChain.release();
        activeConfigIndex = requested;
        startActiveRig(values); // also shows the latest values
      }
      reportActiveConfig();
    } else {
      pushValues(values);
    }
  }
}

void scoreboardStart() {
  outputThread.start(outputThreadMain);
  startedSemaphore.acquire();
}

void scoreboardSetField(ScoreFieldId field, int value) {
  if (field >= FIELD_COUNT) return;
  stateMutex.lock();
  fieldValues[field] = value;
  stateMutex.unlock();
  outputFlags.set(REFRESH_FLAG);
}

void scoreboardRequestConfig(int index) {
  stateMutex.lock();
  requestedConfigIndex = index;
  stateMutex.unlock();
  outputFlags.set(CONFIG_FLAG);
}
