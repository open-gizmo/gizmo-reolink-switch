#include "server.h"

#include <gizmo/device.h>

#include "config/config.h"
#include "nvr/nvr_settings.h"
#include "nvr/nvr_settings_json.h"
#include "remote/remote_control.h"
#include "remote/remote_json.h"

// Web server shared by the web app, its API and the Wi-Fi setup
AsyncWebServer server(80);

// Build the state the web app shows: the timer, the alerts and the NVR
JsonDocument buildStatusData() {
	JsonDocument doc;
	JsonObject data = doc.to<JsonObject>();

	// Add what the main loop published, busy too while a command waits for it
	SwitchStatus status = getSwitchStatus();
	status.isBusy = status.isBusy || hasPendingRemoteCommand();
	writeSwitchStatusToJson(data, status);

	// Return the built JSON document
	return doc;
}

// Build the NVR login the web app shows (never the password)
JsonDocument buildNvrData() {
	JsonDocument doc;
	writeNvrSettingsToJson(doc.to<JsonObject>(), getNvrSettings());
	return doc;
}

// Register the routes and start the server (after the Wi-Fi setup registered its own)
void setupServer() {
	gizmo::setupDeviceApi(server, { config::PROJECT_NAME, config::FIRMWARE_VERSION });
	setupStatusGetRoute();
	setupQuietModePostRoute();
	setupNvrGetRoute();
	setupNvrPostRoute();
	setupNvrTestPostRoute();
	setupWebAppRoute();
	server.begin();
}