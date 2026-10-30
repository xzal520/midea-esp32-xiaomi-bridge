#pragma once

// Copy this file to secrets.h and fill in per-device credentials.
static const char *WIFI_SSID = "midea";
static const char *WIFI_PASSWORD = "123456";
static const char *DEFAULT_WEB_PASSWORD = "123456";
static const char *FALLBACK_AP_PASSWORD = "midea8266";
// Use a unique HomeKit code and setup ID for this S3 device.
static const char *HOMEKIT_SETUP_CODE = "32165487";
static const char *HOMEKIT_SETUP_ID = "S3EX";
// AUTO detects C20, MA2, M4/M6; optional explicit C20, MA2, M4_M6.
static const char *MIOT_PROFILE_OVERRIDE = "AUTO";
