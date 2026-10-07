#include "shift_register.h"

void setupShiftRegisterChain(const ShiftRegisterChainConfig& chain) {
  pinMode(chain.serPin, OUTPUT);
  pinMode(chain.srckPin, OUTPUT);
  pinMode(chain.rckPin, OUTPUT);
  digitalWrite(chain.rckPin, LOW);

  // G/OE is active-low and pulled up to Rail L by default (outputs disabled).
  // Drive it LOW so the shift register outputs are enabled.
  pinMode(chain.oePin, OUTPUT);
  digitalWrite(chain.oePin, LOW);
}

void pushToShiftRegisters(const ShiftRegisterChainConfig& chain, byte values[]) {
  digitalWrite(chain.rckPin, LOW);
  for (int i = 0; i < chain.numSegments; i++) {
    shiftOut(chain.serPin, chain.srckPin, MSBFIRST, values[i]);
  }
  digitalWrite(chain.rckPin, HIGH);
}
