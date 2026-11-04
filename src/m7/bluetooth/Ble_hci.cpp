#include "Ble_hci.h"
#include <ArduinoBLE.h>
#include "utility/ATT.h"
#include "utility/HCI.h"
#include "utility/HCITransport.h"

// ArduinoBLE's HCI object, with the flow control for outgoing data fixed.
// The library lets an application supply its own: platformio.ini builds it
// with FAKE_HCI, so it doesn't define one, and 'HCI' below is used instead.
//
// The bug (ArduinoBLE issue #45, still open): ArduinoBLE counts the packets
// it hands to the radio until the radio reports them sent, and while the
// radio's buffers are full it waits -- in an endless busy loop. But when a
// connection ends, the radio may drop the packets it hadn't sent yet without
// ever reporting them (the Bluetooth spec allows this; the host is meant to
// assume they're gone). ArduinoBLE never resets its count, so every drop in
// the middle of a transfer leaks a little, until the next reply to a phone
// waits forever: the BLE thread hangs, and no more scores get through until
// a reset. A weak signal makes such drops common.
//
// This keeps its own count, resets it as soon as no phone is connected, and
// gives up on a packet after SEND_TIMEOUT_MS instead of waiting forever.

// Longest a packet waits for room in the radio. It normally takes a few ms,
// and a dead connection times out and resets the count well before this --
// so this only guards against the radio never reporting at all.
const unsigned long SEND_TIMEOUT_MS = 2000;

const uint8_t HCI_ACLDATA_PKT = 0x02; // H4 packet type for data, as in ArduinoBLE

class FlowControlledHCI : public HCIClass {
public:
  using HCIClass::poll;

  // Called once, from BLE.begin(): the radio says how many packets it holds.
  int readLeBufferSize(uint16_t& pktLen, uint8_t& maxPkt) override {
    int result = HCIClass::readLeBufferSize(pktLen, maxPkt);
    if (result == 0 && maxPkt > 0) maxPending = maxPkt;
    return result;
  }

  // Every BLE.poll() ends up here, including the waits in sendAclPkt().
  void poll(unsigned long timeout) override {
    HCIClass::poll(timeout);

    // No connection left: the radio has dropped whatever it hadn't sent.
    if (pending > 0 && !ATT.connected()) {
      flushed += pending;
      pending = 0;
    }
  }

  int sendAclPkt(uint16_t handle, uint8_t cid, uint8_t plen, void* data) override {
    unsigned long start = millis();
    while (pending >= maxPending) {
      if (millis() - start >= SEND_TIMEOUT_MS) {
        timedOut++;
        return -1; // dropped
      }
      poll();
    }

    // The same packet HCIClass::sendAclPkt() builds.
    struct __attribute__ ((packed)) HCIACLHdr {
      uint8_t pktType;
      uint16_t handle;
      uint16_t dlen;
      uint16_t plen;
      uint16_t cid;
    } aclHdr = { HCI_ACLDATA_PKT, handle, uint16_t(plen + 4), plen, cid };

    uint8_t txBuffer[sizeof(aclHdr) + UINT8_MAX]; // plen is a uint8_t
    memcpy(txBuffer, &aclHdr, sizeof(aclHdr));
    memcpy(&txBuffer[sizeof(aclHdr)], data, plen);

    pending++;
    HCITransport.write(txBuffer, sizeof(aclHdr) + plen);
    return 0;
  }

  unsigned long flushed = 0;
  unsigned long timedOut = 0;

private:
  // The radio reporting packets it has sent.
  void handleNumCompPkts(uint16_t handle, uint16_t numPkts) override {
    (void)handle;
    pending = (numPkts < pending) ? pending - numPkts : 0;
  }

  uint8_t maxPending = 1; // until the radio says otherwise
  uint8_t pending = 0;    // handed to the radio, not yet reported sent
};

static FlowControlledHCI flowControlledHci;
HCIClass& HCI = flowControlledHci;

unsigned long bleHciFlushedPackets() { return flowControlledHci.flushed; }
unsigned long bleHciTimedOutPackets() { return flowControlledHci.timedOut; }
