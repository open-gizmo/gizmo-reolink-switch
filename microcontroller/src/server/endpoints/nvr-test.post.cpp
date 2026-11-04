#include "../server.h"

#include "nvr/nvr_settings.h"
#include "remote/remote_control.h"

// Check the connection to the NVR (the main loop logs in, the web app follows it with /api/status)
void setupNvrTestPostRoute() {
	server.on("/api/nvr-test", HTTP_POST, [](AsyncWebServerRequest *request) {
		// Validate CORS
		if (!ensureCorsAllowed(request)) {
			return;
		}

		// Nothing to test without a login
		if (!isNvrConfigured(getNvrSettings())) {
			sendErrorResponse(request, 409, "nvr_not_configured", "nvr_not_configured");
			return;
		}

		// Hand the test to the main loop
		if (getSwitchStatus().isBusy || !submitRemoteCommand({ RemoteCommandType::TestNvr, 0 })) {
			sendErrorResponse(request, 409, "device_busy", "device_busy");
			return;
		}

		// Answer with the state (busy until the main loop has run the test)
		JsonDocument state = buildStatusData();
		sendSuccessResponse(request, 202, &state);
	});

	// OPTIONS request for preflighted requests
	server.on("/api/nvr-test", HTTP_OPTIONS, [](AsyncWebServerRequest *request) {
		sendCorsPreflightResponse(request, "POST, OPTIONS");
	});
}