#include "wifi.h"
#include <WiFi.h>
#include "config/config.h"
#include "display/display.h"
#include "serial/serial.h"

namespace {
	// Check if WiFi credentials are configured
	bool hasWiFiCredentials() {
		return config::WIFI_SSID[0] != '\0';
	}
}

// Setup WiFi connection
WiFiSetupResult setupWiFi() {
	// Check if WiFi is already connected
	if (WiFi.status() == WL_CONNECTED) {
		printLogfln("WiFi already connected: %s", WiFi.localIP().toString().c_str());
		displayShowScreen("Wi-Fi Ready!", WiFi.localIP().toString().c_str());
		return WiFiSetupResult::Connected;
	}

	// Check if WiFi credentials are configured
	if (!hasWiFiCredentials()) {
		printLogln("WiFi credentials are not configured.");
		displayShowScreen("Wi-Fi Needed", "Add credentials!");
		return WiFiSetupResult::MissingCredentials;
	}

	// Attempt to connect to WiFi
	printLogf("Connecting to WiFi SSID: %s", config::WIFI_SSID);
	displayShowScreen("Joining Wi-Fi!", "Please wait...");
	WiFi.mode(WIFI_STA);
	WiFi.begin(config::WIFI_SSID, config::WIFI_PASSWORD);

	// Wait for the connection to be established
	while (WiFi.status() != WL_CONNECTED) {
		delay(config::WIFI_CONNECT_RETRY_DELAY_MS);
		printLog(".");
	}

	// Print the successful connection message with the assigned IP address
	printLogln("");
	printLogfln("WiFi connected. IP: %s", WiFi.localIP().toString().c_str());
	displayShowScreen("Wi-Fi Ready!", WiFi.localIP().toString().c_str());
	return WiFiSetupResult::Connected;
}

// Check whether WiFi is currently connected
bool isWiFiConnected() {
	return WiFi.status() == WL_CONNECTED;
}