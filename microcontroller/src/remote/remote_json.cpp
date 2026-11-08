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

	// Add the alerts and the NVR
	json["alertsStillOff"] = status.hasPendingRestore;
	json["busy"] = status.isBusy;
	json["nvrStatus"] = getNvrStatusName(status.nvrStatus);
	json["cameraCount"] = status.cameraCount;
}
