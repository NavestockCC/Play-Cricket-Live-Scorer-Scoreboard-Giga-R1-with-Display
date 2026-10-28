#include "core_link.h"
#include <RPC.h>
#include <mbed.h>
#include <string.h>

// Received lines, oldest first. Filled by onRawReceive() on the RPC event
// thread, drained by coreLinkReadLine() on the loop thread -- 'queueMutex'
// guards everything below. (The RPC library's own rx buffer isn't safe to
// share between those two threads, which is why the link attaches its own
// receive callback instead of using RPC.read().)
const int LINE_QUEUE_SIZE = 32;
static char lineQueue[LINE_QUEUE_SIZE][CORE_LINK_MAX_LINE];
static int lineQueueHead = 0;  // next line to read
static int lineQueueCount = 0;
static unsigned long droppedLines = 0;

// Line currently being assembled from incoming bytes.
static char partialLine[CORE_LINK_MAX_LINE];
static int partialLength = 0;

static rtos::Mutex queueMutex;

static void pushLine(const char* line) {
  if (lineQueueCount == LINE_QUEUE_SIZE) {
    lineQueueHead = (lineQueueHead + 1) % LINE_QUEUE_SIZE;
    lineQueueCount--;
    droppedLines++;
  }
  char* slot = lineQueue[(lineQueueHead + lineQueueCount) % LINE_QUEUE_SIZE];
  strncpy(slot, line, CORE_LINK_MAX_LINE - 1);
  slot[CORE_LINK_MAX_LINE - 1] = '\0';
  lineQueueCount++;
}

// Runs on the RPC event thread for every chunk received on the raw endpoint.
static void onRawReceive(const uint8_t* buf, size_t len) {
  queueMutex.lock();
  for (size_t i = 0; i < len; i++) {
    char c = (char)buf[i];
    if (c == '\r') continue;
    if (c == '\n') {
      partialLine[partialLength] = '\0';
      if (partialLength > 0) pushLine(partialLine);
      partialLength = 0;
    } else if (partialLength < CORE_LINK_MAX_LINE - 1) {
      partialLine[partialLength++] = c;
    } // else: over-long line, extra characters dropped
  }
  queueMutex.unlock();
}

bool coreLinkBegin() {
  RPC.attach(onRawReceive);
  return RPC.begin() != 0;
}

void coreLinkSend(char type, const char* text) {
  char line[CORE_LINK_MAX_LINE + 1];
  int length = snprintf(line, CORE_LINK_MAX_LINE, "%c%s", type, text);
  if (length < 0) return;
  if (length > CORE_LINK_MAX_LINE - 1) length = CORE_LINK_MAX_LINE - 1;
  line[length++] = '\n';
  RPC.write((const uint8_t*)line, length);
}

void coreLinkSendInt(char type, int value) {
  char text[12];
  snprintf(text, sizeof(text), "%d", value);
  coreLinkSend(type, text);
}

bool coreLinkReadLine(char* out, size_t outSize) {
  if (outSize == 0) return false;
  queueMutex.lock();
  bool found = lineQueueCount > 0;
  if (found) {
    strncpy(out, lineQueue[lineQueueHead], outSize - 1);
    out[outSize - 1] = '\0';
    lineQueueHead = (lineQueueHead + 1) % LINE_QUEUE_SIZE;
    lineQueueCount--;
  }
  queueMutex.unlock();
  return found;
}

unsigned long coreLinkDroppedLines() {
  queueMutex.lock();
  unsigned long dropped = droppedLines;
  queueMutex.unlock();
  return dropped;
}
