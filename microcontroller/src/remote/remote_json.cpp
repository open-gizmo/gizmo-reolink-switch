#include "remote_json.h"

#include <string.h>

#include "config/config.h"

// Get the web API name of an NVR status
const char *getNvrStatusName(NvrStatus status) {
	switch (status) {
		case NvrStatus::NotConfigured:
			return "not_configured";
		case NvrStatus::Connected:
			return "connected";
		case NvrStatus::Unreachable:
			return "unreachable";
		case NvrStatus::LoginRefused:
			return "login_refused";
		case NvrStatus::LoginPaused:
			return "login_paused";
		case NvrStatus::Unknown:
		default:
			return "unknown";
	}
}

// Write the switch state into a JSON object
void writeSwitchStatusToJson(JsonObject json, const SwitchStatus &status) {
	// Add the timer
	json["timerRunning"] = status.isTimerRunning;
	json["remainingSeconds"] = status.remainingSeconds;
	json["selectedSeconds"] = status.selectedSeconds;
	json["maxMinutes"] = config::TIMER_MAX_DURATION_SECONDS / 60;

	// Add the alerts and the NVR
	json["alertsStillOff"] = status.hasPendingRestore;
	json["busy"] = status.isBusy;
	json["nvrStatus"] = getNvrStatusName(status.nvrStatus);
	json["cameraCount"] = status.cameraCount;
}

// Read a quiet mode command ({ action: "start", minutes } or { action: "cancel" } or { action: "retry" }; returns the name of the first invalid field, or nullptr)
const char *parseQuietModeCommand(JsonObjectConst json, RemoteCommand &command) {
	JsonVariantConst action = json["action"];
	if (!action.is<const char *>()) {
		return "action";
	}
	const char *actionName = action.as<const char *>();

	// Start for a number of whole minutes, up to the maximum
	if (strcmp(actionName, "start") == 0) {
		JsonVariantConst minutes = json["minutes"];
		const long maxMinutes = config::TIMER_MAX_DURATION_SECONDS / 60;
		if (!minutes.is<long>() || minutes.as<long>() < 1 || minutes.as<long>() > maxMinutes) {
			return "minutes";
		}
		command = { RemoteCommandType::StartQuietMode, (uint32_t)minutes.as<long>() * 60 };
		return nullptr;
	}

	// Cancel the running timer
	if (strcmp(actionName, "cancel") == 0) {
		command = { RemoteCommandType::CancelQuietMode, 0 };
		return nullptr;
	}

	// Retry restoring the alerts that are still off
	if (strcmp(actionName, "retry") == 0) {
		command = { RemoteCommandType::RetryRestore, 0 };
		return nullptr;
	}

	// Unknown action
	return "action";
}