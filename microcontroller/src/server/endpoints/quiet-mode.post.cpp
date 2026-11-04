#include "../server.h"

#include "nvr/nvr_settings.h"
#include "remote/remote_control.h"
#include "remote/remote_json.h"
#include "serial/serial.h"

namespace {
	// Check a command against what the switch is doing (returns the error to send, or nullptr; the main loop checks again when it runs it)
	const char *checkQuietModeCommand(const RemoteCommand &command, const SwitchStatus &status) {
		if (status.isBusy || hasPendingRemoteCommand()) {
			return "device_busy";
		}
		switch (command.type) {
			case RemoteCommandType::StartQuietMode:
				if (status.isTimerRunning) {
					return "timer_running";
				}
				return isNvrConfigured(getNvrSettings()) ? nullptr : "nvr_not_configured";
			case RemoteCommandType::CancelQuietMode:
				return status.isTimerRunning ? nullptr : "timer_not_running";
			case RemoteCommandType::RetryRestore:
				return status.hasPendingRestore && !status.isTimerRunning ? nullptr : "nothing_to_restore";
			default:
				return nullptr;
		}
	}

	// Start, cancel or retry the quiet mode from the JSON body (the main loop runs it, the web app follows it with /api/status)
	void handleQuietModeBody(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
		// Read the JSON body (gizmo-common checks CORS and the body size)
		JsonDocument doc;
		if (!gizmo::readJsonBody(request, data, len, index, total, doc)) {
			return;
		}

		// Read the command
		RemoteCommand command = { RemoteCommandType::None, 0 };
		const char *invalidField = parseQuietModeCommand(doc.as<JsonObjectConst>(), command);
		if (invalidField != nullptr) {
			sendErrorResponse(request, 400, "invalid_quiet_mode_field", invalidField);
			return;
		}

		// Check it can run now
		const char *errorId = checkQuietModeCommand(command, getSwitchStatus());
		if (errorId != nullptr) {
			sendErrorResponse(request, 409, errorId, errorId);
			return;
		}

		// Hand it to the main loop
		if (!submitRemoteCommand(command)) {
			sendErrorResponse(request, 409, "device_busy", "device_busy");
			return;
		}
		printLogfln("Quiet mode command from the web app: %s", doc["action"].as<const char *>());

		// Answer with the state (busy until the main loop has run the command)
		JsonDocument state = buildStatusData();
		sendSuccessResponse(request, 202, &state);
	}
}

// Start, cancel or retry the quiet mode
void setupQuietModePostRoute() {
	// POST a command (the answer is sent by the body handler)
	server.on("/api/quiet-mode", HTTP_POST, [](AsyncWebServerRequest *request) {}, nullptr, handleQuietModeBody);

	// OPTIONS request for preflighted requests
	server.on("/api/quiet-mode", HTTP_OPTIONS, [](AsyncWebServerRequest *request) {
		sendCorsPreflightResponse(request, "POST, OPTIONS");
	});
}