// M4 side of the BLE test: advertises exactly like the real scoreboard
// (same name, service and characteristic UUIDs) so the Play Cricket app can
// connect and send scores. Everything is reported to the M7 over RPC.
#include <Arduino.h>
#include <RPC.h>
#include <ArduinoBLE.h>

const char* SCOREBOARD_SERVICE_UUID = "5a0d6a15-b664-4304-8530-3a0ec53e5bc1";
const char* SCOREBOARD_CHAR_UUID    = "df531f62-fc0b-40ce-81b2-32a6262ea440";
const int MAX_MESSAGE_LENGTH = 8;

BLEService scoreboardService(SCOREBOARD_SERVICE_UUID);
BLECharacteristic scoreboardChar(SCOREBOARD_CHAR_UUID, BLEWrite, MAX_MESSAGE_LENGTH);

static unsigned long messageCount = 0;

static void onScoreboardWritten(BLEDevice central, BLECharacteristic characteristic) {
  (void)central;
  char buffer[MAX_MESSAGE_LENGTH];
  int length = characteristic.valueLength();
  if (length > MAX_MESSAGE_LENGTH - 1) length = MAX_MESSAGE_LENGTH - 1;
  memcpy(buffer, characteristic.value(), length);
  buffer[length] = '\0';

  messageCount++;
  RPC.print("M4: message ");
  RPC.print(messageCount);
  RPC.print(": ");
  RPC.println(buffer);
}

void setup() {
  RPC.begin();
  delay(1000); // give the M7 time to start relaying before the first report

  RPC.println("M4: starting BLE...");
  if (!BLE.begin()) {
    RPC.println("M4: BLE.begin() FAILED -- BLE does not start on the M4");
    while (true) {
      delay(5000);
      RPC.println("M4: (BLE failed, idle)");
    }
  }

  BLE.setLocalName("PlayCricketScorer");
  BLE.setDeviceName("PlayCricketScorer");
  BLE.setAdvertisedService(scoreboardService);
  BLE.setConnectionInterval(24, 40);
  scoreboardChar.setEventHandler(BLEWritten, onScoreboardWritten);
  scoreboardService.addCharacteristic(scoreboardChar);
  BLE.addService(scoreboardService);
  BLE.advertise();

  RPC.print("M4: BLE OK, advertising as PlayCricketScorer, address ");
  RPC.println(BLE.address());
}

void loop() {
  static bool wasConnected = false;
  static unsigned long lastHeartbeat = 0;

  BLEDevice central = BLE.central(); // also polls the BLE stack
  bool isConnected = central && central.connected();

  if (isConnected && !wasConnected) {
    RPC.print("M4: connected to ");
    RPC.println(central.address());
  } else if (!isConnected && wasConnected) {
    RPC.println("M4: disconnected, advertising again");
    BLE.advertise();
  }
  wasConnected = isConnected;

  if (millis() - lastHeartbeat >= 10000) {
    lastHeartbeat = millis();
    RPC.print("M4: alive, ");
    RPC.print(isConnected ? "connected" : "advertising");
    RPC.print(", messages so far: ");
    RPC.println(messageCount);
  }
}
