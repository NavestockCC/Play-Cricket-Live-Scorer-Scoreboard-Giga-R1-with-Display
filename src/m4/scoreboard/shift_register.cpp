#include "shift_register.h"

void setupShiftRegisterChain(const ShiftRegisterChainConfig& chain) {
  // G/OE is active-low and pulled up to Rail L (outputs disabled). Write
  // HIGH before AND after pinMode(): on the Giga core pinMode() re-creates
  // the pin, so this keeps OE from glitching low while it becomes an output.
  digitalWrite(chain.oePin, HIGH);
  pinMode(chain.oePin, OUTPUT);
  digitalWrite(chain.oePin, HIGH);

  pinMode(chain.serPin, OUTPUT);
  pinMode(chain.srckPin, OUTPUT);
  pinMode(chain.rckPin, OUTPUT);
  digitalWrite(chain.rckPin, LOW);
}

void enableShiftRegisterOutputs(const ShiftRegisterChainConfig& chain) {
  digitalWrite(chain.oePin, LOW);
}

void releaseShiftRegisterChain(const ShiftRegisterChainConfig& chain) {
  digitalWrite(chain.oePin, HIGH); // blank the displays first
  pinMode(chain.oePin, INPUT);     // external pull-up keeps OE high
  pinMode(chain.serPin, INPUT);
  pinMode(chain.srckPin, INPUT);
  pinMode(chain.rckPin, INPUT);
}

void pushToShiftRegisters(const ShiftRegisterChainConfig& chain, const byte values[], int count) {
  digitalWrite(chain.rckPin, LOW);
  for (int i = count; i < chain.numSegments; i++) {
    shiftOut(chain.serPin, chain.srckPin, MSBFIRST, ALL_SEGMENTS_OFF);
  }
  for (int i = 0; i < count; i++) {
    shiftOut(chain.serPin, chain.srckPin, MSBFIRST, values[i]);
  }
  digitalWrite(chain.rckPin, HIGH);
}
