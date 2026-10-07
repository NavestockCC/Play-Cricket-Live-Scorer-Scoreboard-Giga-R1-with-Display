#include "display_controller.h"
#include "scoreboard_config/pins_and_constants.h"
#include "shift_register.h"



//Calculates the Max number for any display
int MaxValue (int numSegments){
  int maxV = 1;
  for (int i = 0; i < numSegments; i++) maxV *= 10;
  maxV -= 1;
  return maxV;
}




// Calculates segment bytes for 'number', using exactly 'numSegments' digits.
// Physical wiring: Arduino -> Chip 1 (nearest, highest place value) -> ... -> Chip N (farthest, units)
// values[0] = units digit (sent first, ends up in farthest chip)
// values[numSegments-1] = highest place value digit (sent last, ends up in Chip 1, nearest Arduino)
// Leading zeros are blanked; the units digit is always shown, even when it's 0.
// Slots beyond 'count' are left uninitialized -- callers must only read up to .count elements.
DigitToBytes calculateNumberToBytes(int number, int numSegments) {
  DigitToBytes result;
  numSegments = constrain(numSegments, 1, MAX_DISPLAY_NUMBER);
  result.digitLength = numSegments;

  number = constrain(number, 0, MaxValue(numSegments));

  const byte* digitTable = getActiveScoreboardConfig().digitTable;

  bool leadingZero = true;

  for (int placeExponent = numSegments - 1; placeExponent >= 0; placeExponent--) {
    int placeValue = 1;
    for (int p = 0; p < placeExponent; p++) placeValue *= 10;

    int digit = (number / placeValue) % 10;
    bool isUnits = (placeExponent == 0);

    if (digit != 0) leadingZero = false;

    bool blank = leadingZero && !isUnits;
    result.values[placeExponent] = blank ? ALL_SEGMENTS_OFF : digitTable[digit];
  }

  return result;
}

// Copies 'source' (length sourceLength) into 'destination', starting at
// 'destination[startIndex]'. Returns the next free index in destination
// (useful for chaining multiple copies one after another).
int appendBytes(byte destination[], int startIndex, byte source[], int sourceLength) {
  for (int i = 0; i < sourceLength; i++) {
    destination[startIndex + i] = source[i];
  }
  return startIndex + sourceLength;
}


// Update the scoreboard with the latest values stored in the ScoreBoard
// variable, pushing to both physical rows of the active config.
// On-screen, left-to-right: Top row = BatA, Score, BatB. Bottom row =
// Overs, Wickets, Target.
//
// Fields are appended to the output arrays in the REVERSE of that on-screen
// order. This is because the shift-register chain itself reverses whatever
// order the array is sent in -- values[0] ends up in the farthest chip,
// while the last element ends up in the chip nearest the Arduino (see
// pushToShiftRegisters()) -- so appending BatB/Score/BatA (and
// Target/Wickets/Overs) is what makes BatA/Score/BatB (and
// Overs/Wickets/Target) show up correctly left-to-right on the hardware.
void update_Scoreboard (){
    const ScoreboardChainsConfig& activeConfig = getActiveScoreboardConfig();

    byte topValues[MAX_DISPLAY_NUMBER * 3];
    int topIndex = 0;
    topIndex = appendBytes(topValues, topIndex, ScoreBoard.BatB.values, ScoreBoard.BatB.digitLength);
    topIndex = appendBytes(topValues, topIndex, ScoreBoard.Score.values, ScoreBoard.Score.digitLength);
    topIndex = appendBytes(topValues, topIndex, ScoreBoard.BatA.values, ScoreBoard.BatA.digitLength);
    pushToShiftRegisters(activeConfig.top, topValues);

    byte bottomValues[MAX_DISPLAY_NUMBER * 3];
    int bottomIndex = 0;
    bottomIndex = appendBytes(bottomValues, bottomIndex, ScoreBoard.Target.values, ScoreBoard.Target.digitLength);
    bottomIndex = appendBytes(bottomValues, bottomIndex, ScoreBoard.Wickets.values, ScoreBoard.Wickets.digitLength);
    bottomIndex = appendBytes(bottomValues, bottomIndex, ScoreBoard.Overs.values, ScoreBoard.Overs.digitLength);
    pushToShiftRegisters(activeConfig.bottom, bottomValues);
  }

