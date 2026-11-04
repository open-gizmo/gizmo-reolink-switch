#include "nvr_settings.h"

#include <Arduino.h>
#include <Preferences.h>

namespace {
	// NVS namespace and keys (apart from the muted channels, which keep their own namespace)
	const char NAMESPACE[] = "nvr";
	const char HOST_KEY[] = "host";
	const char USERNAME_KEY[] = "username";
	const char PASSWORD_KEY[] = "password";

	// Current login and its revision (shared by the main loop and the web server task)
	NvrSettings currentSettings = {};
	uint32_t revision = 0;
	SemaphoreHandle_t settingsMutex = nullptr;

	// Save a text, or remove its key when it's empty (putString writes nothing for an empty text)
	bool saveText(Preferences &preferences, const char *key, const char *value) {
		if (value[0] == '\0') {
			return !preferences.isKey(key) || preferences.remove(key);
		}
		return preferences.putString(key, value) > 0;
	}
}

// Load the saved login (call once in setup, before the web server starts)
void loadNvrSettings() {
	settingsMutex = xSemaphoreCreateMutex();
	Preferences preferences;

	// Read the saved login (missing keys stay empty)
	preferences.begin(NAMESPACE, true);
	preferences.getString(HOST_KEY, currentSettings.host, sizeof(currentSettings.host));
	preferences.getString(USERNAME_KEY, currentSettings.username, sizeof(currentSettings.username));
	preferences.getString(PASSWORD_KEY, currentSettings.password, sizeof(currentSettings.password));
	preferences.end();
}

// Get a copy of the current login (safe from any task)
NvrSettings getNvrSettings() {
	xSemaphoreTake(settingsMutex, portMAX_DELAY);
	const NvrSettings settings = currentSettings;
	xSemaphoreGive(settingsMutex);
	return settings;
}

// Get the revision of the login, which changes on every update (so the NVR client drops its old session)
uint32_t getNvrSettingsRevision() {
	xSemaphoreTake(settingsMutex, portMAX_DELAY);
	const uint32_t currentRevision = revision;
	xSemaphoreGive(settingsMutex);
	return currentRevision;
}

// Save a new login and make it current (safe from any task; false, and nothing changes, if it couldn't be saved)
bool updateNvrSettings(const NvrSettings &settings) {
	Preferences preferences;

	// Save the login
	if (!preferences.begin(NAMESPACE, false)) {
		return false;
	}
	const bool isSaved = saveText(preferences, HOST_KEY, settings.host) && saveText(preferences, USERNAME_KEY, settings.username) && saveText(preferences, PASSWORD_KEY, settings.password);
	preferences.end();
	if (!isSaved) {
		return false;
	}

	// Make it current
	xSemaphoreTake(settingsMutex, portMAX_DELAY);
	currentSettings = settings;
	revision += 1;
	xSemaphoreGive(settingsMutex);
	return true;
}