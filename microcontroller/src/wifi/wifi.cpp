#include "wifi.h"

#include <WiFi.h>
#include <gizmo/wifi_setup.h>

#include "config/config.h"
#include "serial/serial.h"
#include "server/server.h"

namespace {
	// Whether the address was logged since the last connection
	bool isAddressLogged = false;
}

// Join the saved network, or open the setup hotspot (call before setupServer)
void setupWiFi() {
	gizmo::setupWifi(server, { config::ACCESS_POINT_NAME, config::HOSTNAME, config::SETUP_PAGE_PATH, config::ASSET_PATH_PREFIX });
}

// Keep the Wi-Fi connected, and log the address once connected (call on every loop iteration)
void manageWiFiConnection() {
	gizmo::manageWifi();

	// Log the address once per connection
	const bool isConnected = gizmo::isWifiConnected();
	if (isConnected && !isAddressLogged) {
		printLogfln("Connected, open http://%s.local or http://%s", config::HOSTNAME, WiFi.localIP().toString().c_str());
	}
	isAddressLogged = isConnected;
}

// Check whether the switch is connected to the Wi-Fi
bool isWiFiConnected() {
	return gizmo::isWifiConnected();
}

// Check whether the setup hotspot is open
bool isSetupHotspotActive() {
	return gizmo::isAccessPointActive();
}