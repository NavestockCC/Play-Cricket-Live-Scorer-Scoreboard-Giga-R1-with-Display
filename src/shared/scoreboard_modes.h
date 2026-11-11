#ifndef SCOREBOARD_MODES_H
#define SCOREBOARD_MODES_H

// Operating modes of the physical scoreboard, shared by both cores: the M4
// runs the active mode, the M7 lists the names in the mode picker on the
// touchscreen. Both firmwares compile this same list, so a mode index means
// the same mode on either core.
//
// Each test mode loops its pattern (one step a second) until another mode
// is picked. Scores keep arriving meanwhile; the board shows the latest
// ones as soon as it's back in MODE_SCOREBOARD.

enum ScoreboardMode {
  MODE_SCOREBOARD,      // the live score from the Play-Cricket app (boot mode)
  MODE_TEST_DIGITS,     // every digit counts 0-9 together
  MODE_TEST_SEGMENTS,   // one segment at a time (A-G, then DP) on every digit
  MODE_TEST_CHASE,      // a lit 8 moves along each row, chip 1 to the last
  MODE_TEST_ALL_ON_OFF, // every segment on, then all off
  MODE_COUNT
};

// Indexed by ScoreboardMode.
extern const char* const SCOREBOARD_MODE_NAMES[MODE_COUNT];

#endif
