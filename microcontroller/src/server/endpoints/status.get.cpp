#include "../server.h"

// Get the timer, the alerts and the NVR state
void setupStatusGetRoute() {
	server.on("/api/status", HTTP_GET, [](AsyncWebServerRequest *request) {
		// Validate CORS
		if (!ensureCorsAllowed(request)) {
			return;
		}

		// Send the state
		JsonDocument doc = buildStatusData();
		sendSuccessResponse(request, 200, &doc);
	});
}