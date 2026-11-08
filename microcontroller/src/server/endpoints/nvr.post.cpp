#include "../server.h"

#include "nvr/nvr_settings.h"
#include "nvr/nvr_settings_json.h"
#include "remote/remote_control.h"
#include "serial/serial.h"

namespace {
	// Save the NVR login from the JSON body, then check it (the main loop logs in, the web app follows it with /api/status)
	void handleNvrBody(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
		// Read the JSON body (gizmo-common checks CORS and the body size)
		JsonDocument doc;
		if (!gizmo::readJsonBody(request, data, len, index, total, doc)) {
			return;
		}

		// Refuse while cameras are muted (they were muted on the saved NVR, which must restore them)
		const SwitchStatus status = getSwitchStatus();
		if (status.isTimerRunning || status.hasPendingRestore) {
			sendErrorResponse(request, 409, "nvr_change_while_muted", "nvr_change_while_muted");
			return;
		}

		// Check and apply the fields to a copy of the login
		NvrSettings settings = getNvrSettings();
		const char *invalidField = applyNvrSettingsFromJson(doc.as<JsonObjectConst>(), settings);
		if (invalidField != nullptr) {
			sendErrorResponse(request, 400, "invalid_nvr_field", invalidField);
			return;
		}

		// Save it and make it current
		if (!updateNvrSettings(settings)) {
			sendErrorResponse(request, 500, "nvr_save_failed", "Failed to save the NVR login");
			return;
		}
		printLogfln("NVR login changed from the web app: %s", settings.host);

		// Check the new login (skipped if another command is waiting: the web app can test it again)
		submitRemoteCommand({ RemoteCommandType::TestNvr });

		// Answer with the saved login
		JsonDocument nvr = buildNvrData();
		sendSuccessResponse(request, 200, &nvr);
	}
}

// Change the NVR login
void setupNvrPostRoute() {
	// POST a new login (the answer is sent by the body handler)
	server.on("/api/nvr", HTTP_POST, [](AsyncWebServerRequest *request) {}, nullptr, handleNvrBody);

	// OPTIONS request for preflighted requests
	server.on("/api/nvr", HTTP_OPTIONS, [](AsyncWebServerRequest *request) {
		sendCorsPreflightResponse(request, "POST, OPTIONS");
	});
}