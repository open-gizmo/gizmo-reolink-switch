#include "../server.h"

// Get the NVR login (never the password)
void setupNvrGetRoute() {
	server.on("/api/nvr", HTTP_GET, [](AsyncWebServerRequest *request) {
		// Validate CORS
		if (!ensureCorsAllowed(request)) {
			return;
		}

		// Send the login
		JsonDocument doc = buildNvrData();
		sendSuccessResponse(request, 200, &doc);
	});
}