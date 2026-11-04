#include "score_fields.h"
#include <string.h>

// Same order as ScoreFieldId.
const ScoreFieldInfo SCORE_FIELDS[FIELD_COUNT] = {
  {"B1S", "Bat A",   3}, // FIELD_BAT_A
  {"BTR", "Score",   3}, // FIELD_SCORE
  {"B2S", "Bat B",   3}, // FIELD_BAT_B
  {"OVB", "Overs",   2}, // FIELD_OVERS -- value may include a decimal ("12.3")
  {"BTW", "Wickets", 1}, // FIELD_WICKETS
  {"BTT", "Target",  3}, // FIELD_TARGET
};

ScoreFieldId findScoreField(const char* message) {
  if (message == nullptr || strlen(message) <= (size_t)SCORE_CODE_LENGTH) return FIELD_COUNT;
  for (int i = 0; i < FIELD_COUNT; i++) {
    if (strncmp(message, SCORE_FIELDS[i].code, SCORE_CODE_LENGTH) == 0) return (ScoreFieldId)i;
  }
  return FIELD_COUNT;
}

int parseScoreValue(const char* message) {
  const char* text = message + SCORE_CODE_LENGTH;
  int result = 0;
  for (int i = 0; text[i] >= '0' && text[i] <= '9'; i++) {
    result = (result * 10) + (text[i] - '0');
  }
  return result;
}
