#ifndef BLE_HCI_H
#define BLE_HCI_H

// Replaces ArduinoBLE's HCI layer to fix a hang in how it waits for the
// radio before sending data -- see Ble_hci.cpp. Call only from the BLE
// thread, like the rest of ArduinoBLE.

// Replies to the phone that the radio dropped, unsent, when a connection
// ended. Total since boot.
unsigned long bleHciFlushedPackets();

// Replies given up on because the radio stopped making room for them.
// Total since boot.
unsigned long bleHciTimedOutPackets();

#endif
