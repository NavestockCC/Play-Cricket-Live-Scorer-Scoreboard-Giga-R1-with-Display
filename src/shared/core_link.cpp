#include "core_link.h"
#include <RPC.h>
#include <string.h>

// Where received lines go. Lines are assembled and pushed on the RPC event
// thread; the LineQueue makes them safe to read from the consumer thread.
// (The RPC library's own rx buffer isn't safe to share between those two
// threads, which is why the link attaches its own receive callback instead
// of using RPC.read().)
static LineQueue* inboxQueue = nullptr;

// Line currently being assembled from incoming bytes. Only touched by the
// RPC thread.
static char partialLine[CORE_LINK_MAX_LINE];
static int partialLength = 0;

// Runs on the RPC event thread for every chunk received on the raw endpoint.
static void onRawReceive(const uint8_t* buf, size_t len) {
  for (size_t i = 0; i < len; i++) {
    char c = (char)buf[i];
    if (c == '\r') continue;
    if (c == '\n') {
      partialLine[partialLength] = '\0';
      if (partialLength > 0 && inboxQueue != nullptr) inboxQueue->push(partialLine);
      partialLength = 0;
    } else if (partialLength < CORE_LINK_MAX_LINE - 1) {
      partialLine[partialLength++] = c;
    } // else: over-long line, extra characters dropped
  }
}

bool coreLinkBegin(LineQueue& inbox) {
  inboxQueue = &inbox;
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
