#include "scoreboard.h"
#include <Arduino.h>
#include <mbed.h>
#include <string.h>
#include "shift_register.h"
#include "test_patterns.h"
#include "shared/core_link.h"
#include "shared/scoreboard_configs.h"
#include "shared/scoreboard_modes.h"

// Requests from other threads, guarded by 'stateMutex'. The output thread
// copies them out under the lock and does the slow hardware work without it.
static rtos::Mutex stateMutex;
static int fieldValues[FIELD_COUNT] = {0};
static int requestedConfigIndex = 0;             // valid while CONFIG_FLAG is set
static int requestedModeIndex = MODE_SCOREBOARD; // valid while MODE_FLAG is set

static rtos::EventFlags outputFlags;
const uint32_t REFRESH_FLAG = 0x01;
const uint32_t CONFIG_FLAG  = 0x02;
const uint32_t MODE_FLAG    = 0x04;
const uint32_t ALL_FLAGS    = REFRESH_FLAG | CONFIG_FLAG | MODE_FLAG;

// How long each step of a test pattern stays on the board.
const rtos::Kernel::Clock::duration TEST_STEP_TIME = std::chrono::seconds(1);

// Bytes per row: one per digit.
const int ROW_BUFFER_SIZE = FIELDS_PER_ROW * MAX_FIELD_DIGITS;

// Released once the boot rig is up (see scoreboardStart()).
static rtos::Semaphore startedSemaphore(0, 1);

// Owned by the output thread only.
static rtos::Thread outputThread(osPriorityAboveNormal, 4096, nullptr, "scoreboard");
static ShiftRegisterChain topChain;
static ShiftRegisterChain bottomChain;
static int activeConfigIndex = 0;
static ScoreboardMode activeMode = MODE_SCOREBOARD;
static int testStep = 0; // the active test's current step

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
  byte rowBytes[ROW_BUFFER_SIZE];

  int count = renderRow(config.top.fields, values, config.digitTable, rowBytes);
  topChain.push(rowBytes, count);

  count = renderRow(config.bottom.fields, values, config.digitTable, rowBytes);
  bottomChain.push(rowBytes, count);
}

// Shows the active test's current step on every chip of both rows (up to
// ROW_BUFFER_SIZE chips a row; push() blanks any beyond).
static void pushTestStep() {
  const ScoreboardChainsConfig& config = SCOREBOARD_CONFIGS[activeConfigIndex];
  byte rowBytes[ROW_BUFFER_SIZE];

  int chips = min(config.top.numSegments, ROW_BUFFER_SIZE);
  int count = renderTestRow(activeMode, testStep, config, chips, rowBytes);
  topChain.push(rowBytes, count);

  chips = min(config.bottom.numSegments, ROW_BUFFER_SIZE);
  count = renderTestRow(activeMode, testStep, config, chips, rowBytes);
  bottomChain.push(rowBytes, count);
}

// Shows the active mode: the scores, or the active test's current step.
static void showActiveMode(const int values[FIELD_COUNT]) {
  if (activeMode == MODE_SCOREBOARD) pushValues(values);
  else pushTestStep();
}

// Takes over the active rig's pins and shows the active mode on it.
static void startActiveRig(const int values[FIELD_COUNT]) {
  const ScoreboardChainsConfig& config = SCOREBOARD_CONFIGS[activeConfigIndex];
  topChain.attach(config.top);
  bottomChain.attach(config.bottom);
  showActiveMode(values);
  topChain.enableOutputs();
  bottomChain.enableOutputs();
}

static int rowDigits(const ShiftRegisterChainConfig& row) {
  int digits = 0;
  for (int i = 0; i < FIELDS_PER_ROW; i++) digits += SCORE_FIELDS[row.fields[i]].digits;
  return digits;
}

// Tells the M7 which rig is active (so its picker shows it), and warns on
// its log if that rig's chip counts don't match its field layout -- nothing
// overruns, but digits would land on the wrong chips.
static void reportActiveConfig() {
  const ScoreboardChainsConfig& config = SCOREBOARD_CONFIGS[activeConfigIndex];
  coreLinkSendInt(LINK_MSG_CONFIG, activeConfigIndex);

  char message[CORE_LINK_MAX_LINE];
  snprintf(message, sizeof(message), "Config: %s", config.name);
  coreLinkSend(LINK_MSG_LOG, message);

  if (rowDigits(config.top) != config.top.numSegments ||
      rowDigits(config.bottom) != config.bottom.numSegments) {
    coreLinkSend(LINK_MSG_LOG, "WARN: digit layout mismatch");
  }
}

// Tells the M7 which mode is active (so its picker shows it) and logs it.
static void reportActiveMode() {
  coreLinkSendInt(LINK_MSG_MODE, activeMode);

  char message[CORE_LINK_MAX_LINE];
  snprintf(message, sizeof(message), "Mode: %s", SCOREBOARD_MODE_NAMES[activeMode]);
  coreLinkSend(LINK_MSG_LOG, message);
}

static void outputThreadMain() {
  int values[FIELD_COUNT] = {0};
  activeConfigIndex = defaultScoreboardConfigIndex();
  startActiveRig(values);
  reportActiveConfig();
  reportActiveMode();
  startedSemaphore.release();

  rtos::Kernel::Clock::time_point nextTestStep;
  while (true) {
    // While a test runs, also wake up when its next step is due. A timeout
    // comes back as an error code (top bit set), not as flags.
    uint32_t flags = (activeMode == MODE_SCOREBOARD)
        ? outputFlags.wait_any(ALL_FLAGS)
        : outputFlags.wait_any_until(ALL_FLAGS, nextTestStep);
    if (flags & osFlagsError) flags = 0;

    stateMutex.lock();
    memcpy(values, fieldValues, sizeof(values));
    int configRequest = requestedConfigIndex;
    int modeRequest = requestedModeIndex;
    stateMutex.unlock();

    // Scores that arrive during a test are kept in 'values' and shown as
    // soon as the board is back in MODE_SCOREBOARD.
    rtos::Kernel::Clock::time_point now = rtos::Kernel::Clock::now();
    if (flags & MODE_FLAG) {
      if (modeRequest >= 0 && modeRequest < MODE_COUNT) {
        activeMode = (ScoreboardMode)modeRequest;
        testStep = 0;
        nextTestStep = now + TEST_STEP_TIME;
      }
      reportActiveMode();
    } else if (activeMode != MODE_SCOREBOARD && now >= nextTestStep) {
      const ScoreboardChainsConfig& config = SCOREBOARD_CONFIGS[activeConfigIndex];
      testStep = (testStep + 1) % testPatternSteps(activeMode, config);
      nextTestStep = now + TEST_STEP_TIME;
    }

    bool switchRig = (flags & CONFIG_FLAG) &&
                     configRequest >= 0 && configRequest < SCOREBOARD_CONFIG_COUNT;
    if (switchRig) {
      topChain.release();
      bottomChain.release();
      activeConfigIndex = configRequest;
      testStep = 0;           // the new rig's test may have fewer steps
      startActiveRig(values); // also shows the active mode
    } else {
      showActiveMode(values);
    }
    if (flags & CONFIG_FLAG) reportActiveConfig();
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

void scoreboardRequestMode(int mode) {
  stateMutex.lock();
  requestedModeIndex = mode;
  stateMutex.unlock();
  outputFlags.set(MODE_FLAG);
}
