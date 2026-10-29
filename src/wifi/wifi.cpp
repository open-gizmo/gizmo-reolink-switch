#include "wifi.h"
#include <WiFi.h>
#include "config/config.h"
#include "display/display.h"
#include "serial/serial.h"

namespace {
	// Last time the connection was up or a reconnect was attempted
	uint32_t lastReconnectAttemptTimeMs = 0;

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

	// Wait for the connection to be established (bounded, so the device still boots when the router is down)
	const uint32_t connectStartTimeMs = millis();
	while (WiFi.status() != WL_CONNECTED) {
		// Give up waiting and let the main loop keep retrying in the background
		if (millis() - connectStartTimeMs >= config::WIFI_CONNECT_TIMEOUT_MS) {
			printLogln("");
			printLogln("WiFi connection timed out, retrying in the background.");
			displayShowScreen("Wi-Fi Offline", "Will retry");
			lastReconnectAttemptTimeMs = millis();
			return WiFiSetupResult::ConnectTimeout;
		}

		// Wait a bit before checking again
		delay(config::WIFI_CONNECT_RETRY_DELAY_MS);
		printLog(".");
	}

	// Print the successful connection message with the assigned IP address
	printLogln("");
	printLogfln("WiFi connected. IP: %s", WiFi.localIP().toString().c_str());
	displayShowScreen("Wi-Fi Ready!", WiFi.localIP().toString().c_str());
	return WiFiSetupResult::Connected;
}

// Force a reconnect when the connection has been down for a while
void manageWiFiConnection() {
	const uint32_t currentTimeMs = millis();

	// Keep the timer fresh while connected (so a drop gets a grace period for the core's own auto-reconnect)
	if (WiFi.status() == WL_CONNECTED || !hasWiFiCredentials()) {
		lastReconnectAttemptTimeMs = currentTimeMs;
		return;
	}

	// Wait until the reconnect interval has passed
	if (currentTimeMs - lastReconnectAttemptTimeMs < config::WIFI_RECONNECT_INTERVAL_MS) return;

	// Attempt to reconnect
	lastReconnectAttemptTimeMs = currentTimeMs;
	printLogln("WiFi is down, reconnecting...");
	WiFi.reconnect();
}

// Check whether WiFi is currently connected
bool isWiFiConnected() {
	return WiFi.status() == WL_CONNECTED;
}