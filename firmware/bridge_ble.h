#pragma once
#include <Arduino.h>

// JSON lines over an encrypted, bonded BLE link. UUIDs are shared with Android.
#define BRIDGE_BLE_SERVICE "6d696465-6100-4000-8000-000000000001"
#define BRIDGE_BLE_RX      "6d696465-6100-4000-8000-000000000002"
#define BRIDGE_BLE_TX      "6d696465-6100-4000-8000-000000000003"
#define BRIDGE_VERSION "2.5.54"

bool bridgeBleBegin();
void bridgeBlePoll();
bool bridgeBleTakeLine(String &line);
void bridgeBleSend(const String &line, bool statusMessage = false);
bool bridgeBleConnected();
uint32_t bridgeBleSession();
const String &bridgeBleName();
