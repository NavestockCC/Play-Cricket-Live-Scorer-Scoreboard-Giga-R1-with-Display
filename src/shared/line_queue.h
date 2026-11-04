#ifndef LINE_QUEUE_H
#define LINE_QUEUE_H

// Thread-safe queue of short text lines, used to hand messages from
// producer threads (the RPC thread, the BLE thread) to one consumer thread
// that sleeps in waitForLine() until there's something to do.
//
// Lines use the core link format (see core_link.h): one type character,
// then text, no '\n'. If the queue is full the OLDEST line is dropped:
// every score message carries a field's full current value, so newer
// lines supersede older ones.

#include <Arduino.h>
#include <mbed.h>

const int MAX_LINE_LENGTH = 48; // incl. type char and '\0'

class LineQueue {
public:
  // Queues 'line' (truncated to MAX_LINE_LENGTH - 1 characters).
  void push(const char* line);

  // Queues 'type' followed by 'text'.
  void post(char type, const char* text);

  // Copies the oldest line into 'out' and returns true; returns false if
  // the queue is empty.
  bool pop(char* out, size_t outSize);

  // Blocks the calling thread (other threads keep running) until a line is
  // waiting or 'timeoutMs' passes. Returns true if a line is waiting.
  bool waitForLine(uint32_t timeoutMs);

  // Lines discarded because the queue was full.
  unsigned long droppedLines();

private:
  static const int QUEUE_SIZE = 32;
  static const uint32_t LINE_WAITING_FLAG = 0x01;

  rtos::Mutex mutex; // guards everything below
  rtos::EventFlags flags;
  char lines[QUEUE_SIZE][MAX_LINE_LENGTH];
  int head = 0; // next line to read
  int count = 0;
  unsigned long dropped = 0;
};

#endif
