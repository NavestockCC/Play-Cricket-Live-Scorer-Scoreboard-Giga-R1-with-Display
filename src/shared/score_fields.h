#ifndef SCORE_FIELDS_H
#define SCORE_FIELDS_H

// The six score fields, shared by both cores: the M4 maps incoming BLE
// messages to a field and drives its digits, the M7 maps the same messages
// (forwarded over the core link) to that field's score card.
//
// Every BLE message is <3-letter code><value>, e.g. "BTR142" or "OVB12.3".

enum ScoreFieldId {
  FIELD_BAT_A,
  FIELD_SCORE,
  FIELD_BAT_B,
  FIELD_OVERS,
  FIELD_WICKETS,
  FIELD_TARGET,
  FIELD_COUNT
};

struct ScoreFieldInfo {
  const char* code;  // 3-letter BLE code
  const char* label; // heading on the M7's score card
  int digits;        // physical 7-segment digits on the scoreboard
};

// Indexed by ScoreFieldId.
extern const ScoreFieldInfo SCORE_FIELDS[FIELD_COUNT];

const int SCORE_CODE_LENGTH = 3;

// Largest number of digits any single field uses.
const int MAX_FIELD_DIGITS = 3;

// Returns the field whose code starts 'message', or FIELD_COUNT if the
// message is too short (code + at least one value character) or the code
// is unknown.
ScoreFieldId findScoreField(const char* message);

// The whole-number value of a message: the digits after the code, stopping
// at a decimal point or any other non-digit ("OVB12.3" -> 12, "BTR142" ->
// 142). The physical board shows whole overs only.
int parseScoreValue(const char* message);

#endif
