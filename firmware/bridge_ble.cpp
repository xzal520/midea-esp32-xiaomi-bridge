#include "bridge_ble.h"
#include <NimBLEDevice.h>
#include <WiFi.h>
#include <esp_mac.h>
#include <esp_system.h>
// arduino-esp32 3.3.8 releases BT controller memory at boot unless a linked BT
// library registers itself via this header's constructor; without it,
// esp_bt_controller_init() later fails and panics.
#if __has_include("esp32-hal-alloc-ble-mem.h")
#include <esp32-hal-alloc-ble-mem.h>
#elif __has_include("esp32-hal-bt-mem.h")
#include <esp32-hal-bt-mem.h>
#endif
#include <atomic>
#include <deque>

namespace {
struct RxChunk { uint32_t session; uint16_t length; uint8_t data[180]; };
struct TxMessage { String data; bool status; };
QueueHandle_t rxQueue = nullptr;
NimBLECharacteristic *tx = nullptr;
std::atomic<bool> connected{false};
std::atomic<bool> overflow{false};
std::atomic<uint32_t> session{0};
uint32_t observedSession = 0;
bool droppingLine = false;
String deviceName;
String incoming;
std::deque<String> lines;
std::deque<TxMessage> outgoing;
size_t txOffset = 0;
uint32_t lastTxAt = 0;
ble_gap_event_listener securityEventListener{};

int logSecurityEvent(ble_gap_event *event, void *) {
  if (event->type == BLE_GAP_EVENT_ENC_CHANGE) {
    Serial.printf("[BLE] encryption event: status=%d bonds=%d\n",
                  event->enc_change.status, NimBLEDevice::getNumBonds());
  }
  return 0;
}

class ServerCallbacks : public NimBLEServerCallbacks {
  void onConnect(NimBLEServer *) override { connected.store(true); session.fetch_add(1); Serial.println("[BLE] client connected"); }
  void onConnect(NimBLEServer *server, ble_gap_conn_desc *description) override {
    server->updateConnParams(description->conn_handle, 24, 40, 0, 600);
    // The phone initiates bonding; encrypted RX access also triggers security.
    // Starting a second SMP procedure here can race Windows/Android pairing.
  }
  void onDisconnect(NimBLEServer *server) override {
    connected.store(false);
    session.fetch_add(1);
    Serial.println("[BLE] client disconnected");
    server->startAdvertising();
  }
  void onAuthenticationComplete(ble_gap_conn_desc *description) override {
    Serial.printf("[BLE] security: encrypted=%u bonded=%u\n", description->sec_state.encrypted, description->sec_state.bonded);
  }
};

class RxCallbacks : public NimBLECharacteristicCallbacks {
  void onWrite(NimBLECharacteristic *characteristic) override {
    const std::string value = characteristic->getValue();
    // Callbacks run on the Bluetooth task. Main-loop code owns parsing and UARTs.
    if (!rxQueue || value.empty()) return;
    if (value.size() > sizeof(RxChunk::data)) { overflow.store(true); return; }
    RxChunk chunk;
    chunk.session = session.load();
    chunk.length = value.size();
    memcpy(chunk.data, value.data(), value.size());
    if (xQueueSend(rxQueue, &chunk, 0) != pdTRUE) overflow.store(true);
  }
};

}

bool bridgeBleBegin() {
  rxQueue = xQueueCreate(12, sizeof(RxChunk));
  if (!rxQueue) return false;
  uint8_t mac[6];
  esp_read_mac(mac, ESP_MAC_WIFI_STA);
  char suffix[6];
  snprintf(suffix, sizeof(suffix), "%02X%02X", mac[4], mac[5]);
  deviceName = "Midea-S3-Bridge-" + String(suffix);
  incoming.reserve(512);
  NimBLEDevice::init(deviceName.c_str());
  const int listenerResult = ble_gap_event_listener_register(&securityEventListener, logSecurityEvent, nullptr);
  if (listenerResult != 0 && listenerResult != BLE_HS_EALREADY) {
    Serial.printf("[BLE] security diagnostics unavailable: %d\n", listenerResult);
  }
  NimBLEDevice::setMTU(185);
  NimBLEDevice::setSecurityAuth(true, false, true);
  NimBLEDevice::setSecurityIOCap(BLE_HS_IO_NO_INPUT_OUTPUT);
  auto *server = NimBLEDevice::createServer();
  server->setCallbacks(new ServerCallbacks());
  auto *service = server->createService(BRIDGE_BLE_SERVICE);
  auto *rx = service->createCharacteristic(BRIDGE_BLE_RX, NIMBLE_PROPERTY::WRITE);
  rx->setCallbacks(new RxCallbacks());
  tx = service->createCharacteristic(BRIDGE_BLE_TX, NIMBLE_PROPERTY::NOTIFY);
  service->start();
  auto *advertising = NimBLEDevice::getAdvertising();
  advertising->addServiceUUID(BRIDGE_BLE_SERVICE);
  advertising->setScanResponse(true);
  NimBLEDevice::startAdvertising();
  return true;
}

void bridgeBlePoll() {
  const uint32_t currentSession = session.load();
  const bool isConnected = connected.load();
  if (observedSession != currentSession) {
    observedSession = currentSession;
    incoming = "";
    droppingLine = false;
    lines.clear();
    outgoing.clear();
    txOffset = 0;
    overflow.store(false);
  }
  if (!isConnected || !rxQueue) return;
  if (overflow.exchange(false)) {
    incoming = "";
    // Discard through the next newline so a partial command is never executed.
    droppingLine = true;
    bridgeBleSend("{\"type\":\"error\",\"code\":\"busy\",\"message\":\"请稍后重试\"}");
  }
  RxChunk chunk;
  while (xQueueReceive(rxQueue, &chunk, 0) == pdTRUE) {
    if (chunk.session != currentSession) continue;
    for (size_t i = 0; i < chunk.length; ++i) {
      const char c = chunk.data[i];
      if (c == '\n') {
        if (!droppingLine && incoming.length() && lines.size() < 4) lines.push_back(incoming);
        incoming = "";
        droppingLine = false;
      } else if (c != '\r' && !droppingLine) {
        if (incoming.length() < 511) incoming += c;
        else { incoming = ""; droppingLine = true; }
      }
    }
  }
  if (!outgoing.empty() && tx && static_cast<uint32_t>(millis() - lastTxAt) >= 12) {
    TxMessage &message = outgoing.front();
    const size_t count = min(static_cast<size_t>(20), message.data.length() - txOffset);
    // Twenty bytes also works when the phone leaves ATT MTU at its default 23.
    tx->setValue(reinterpret_cast<uint8_t *>(const_cast<char *>(message.data.c_str() + txOffset)), count);
    tx->notify();
    txOffset += count;
    lastTxAt = millis();
    if (txOffset == message.data.length()) { outgoing.pop_front(); txOffset = 0; }
  }
}

bool bridgeBleTakeLine(String &line) {
  if (lines.empty()) return false;
  line = lines.front();
  lines.pop_front();
  return true;
}

void bridgeBleSend(const String &line, bool statusMessage) {
  if (!connected.load()) return;
  if (statusMessage) {
    for (size_t i = 0; i < outgoing.size(); ++i) {
      if (outgoing[i].status && (i != 0 || txOffset == 0)) {
        outgoing[i].data = line + '\n';
        return;
      }
    }
  }
  if (outgoing.size() < 8) outgoing.push_back({line + '\n', statusMessage});
}

bool bridgeBleConnected() { return connected.load(); }
uint32_t bridgeBleSession() { return session.load(); }
const String &bridgeBleName() { return deviceName; }
