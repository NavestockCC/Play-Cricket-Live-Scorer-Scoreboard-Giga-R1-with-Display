#include "test_patterns.h"
#include "shift_register.h"

// The board's segments in segmentTable order, skipping a missing decimal
// point (entry 0).
static int segmentCount(const ScoreboardChainsConfig& config) {
  int count = 0;
  for (int i = 0; i < SEGMENTS_PER_DIGIT; i++) {
    if (config.segmentTable[i] != 0) count++;
  }
  return count;
}

static byte nthSegment(const ScoreboardChainsConfig& config, int n) {
  for (int i = 0; i < SEGMENTS_PER_DIGIT; i++) {
    if (config.segmentTable[i] == 0) continue;
    if (n-- == 0) return config.segmentTable[i];
  }
  return ALL_SEGMENTS_OFF;
}

static byte allSegments(const ScoreboardChainsConfig& config) {
  byte bits = ALL_SEGMENTS_OFF;
  for (int i = 0; i < SEGMENTS_PER_DIGIT; i++) bits |= config.segmentTable[i];
  return bits;
}

int testPatternSteps(ScoreboardMode mode, const ScoreboardChainsConfig& config) {
  int steps = 1;
  switch (mode) {
    case MODE_TEST_DIGITS:     steps = 10; break;
    case MODE_TEST_SEGMENTS:   steps = segmentCount(config); break;
    case MODE_TEST_CHASE:      steps = max(config.top.numSegments, config.bottom.numSegments); break;
    case MODE_TEST_ALL_ON_OFF: steps = 2; break;
    default:                   break;
  }
  return max(steps, 1);
}

int renderTestRow(ScoreboardMode mode, int step, const ScoreboardChainsConfig& config,
                  int chips, byte out[]) {
  for (int i = 0; i < chips; i++) {
    int chip = chips - i; // out[i] lands in this chip (1 = nearest the Giga)
    byte bits = ALL_SEGMENTS_OFF;
    switch (mode) {
      case MODE_TEST_DIGITS:     bits = config.digitTable[step % 10]; break;
      case MODE_TEST_SEGMENTS:   bits = nthSegment(config, step); break;
      // Both rows chase in step, so chip n lights on both at once; the
      // shorter row stays dark while the longer one finishes.
      case MODE_TEST_CHASE:      if (chip == step + 1) bits = config.digitTable[8]; break;
      case MODE_TEST_ALL_ON_OFF: if (step == 0) bits = allSegments(config); break;
      default:                   break;
    }
    out[i] = bits;
  }
  return chips;
}
