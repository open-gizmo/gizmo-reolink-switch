#ifndef WIFI_MODULE_H
#define WIFI_MODULE_H

#include <Arduino.h>

enum class WiFiSetupResult : uint8_t {
	Connected,
	MissingCredentials,
};

WiFiSetupResult setupWiFi();
bool isWiFiConnected();

#endif