#include "line_queue.h"
#include <string.h>

void LineQueue::push(const char* line) {
  mutex.lock();
  if (count == QUEUE_SIZE) {
    head = (head + 1) % QUEUE_SIZE;
    count--;
    dropped++;
  }
  char* slot = lines[(head + count) % QUEUE_SIZE];
  strncpy(slot, line, MAX_LINE_LENGTH - 1);
  slot[MAX_LINE_LENGTH - 1] = '\0';
  count++;
  flags.set(LINE_WAITING_FLAG);
  mutex.unlock();
}

void LineQueue::post(char type, const char* text) {
  char line[MAX_LINE_LENGTH];
  snprintf(line, sizeof(line), "%c%s", type, text);
  push(line);
}

bool LineQueue::pop(char* out, size_t outSize) {
  if (outSize == 0) return false;
  mutex.lock();
  bool found = count > 0;
  if (found) {
    strncpy(out, lines[head], outSize - 1);
    out[outSize - 1] = '\0';
    head = (head + 1) % QUEUE_SIZE;
    count--;
    if (count == 0) flags.clear(LINE_WAITING_FLAG);
  }
  mutex.unlock();
  return found;
}

bool LineQueue::waitForLine(uint32_t timeoutMs) {
  // Doesn't clear the flag: pop() does, once the queue is empty.
  uint32_t result = flags.wait_any_for(LINE_WAITING_FLAG,
                                       rtos::Kernel::Clock::duration_u32(timeoutMs), false);
  return (result & osFlagsError) == 0;
}

unsigned long LineQueue::droppedLines() {
  mutex.lock();
  unsigned long result = dropped;
  mutex.unlock();
  return result;
}
