// M7 side of the M4 BLE test: boots the M4 core and relays everything it
// prints over RPC to the USB serial monitor (the M4 has no USB serial).
#include <Arduino.h>
#include <RPC.h>

void setup() {
  Serial.begin(115200);
  RPC.begin(); // boots the M4 firmware
}

void loop() {
  while (RPC.available()) {
    Serial.write(RPC.read());
  }
}
