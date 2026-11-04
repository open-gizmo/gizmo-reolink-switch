#ifndef NVR_SETTINGS_H
#define NVR_SETTINGS_H

#include <stddef.h>
#include <stdint.h>

#include "config/config.h"

// Login of the Reolink NVR, set from the web app and saved in NVS
struct NvrSettings {
	char host[config::NVR_HOST_SIZE]; // Address of the NVR, for example "192.168.1.10" or "nvr.local:443"
	char username[config::NVR_USERNAME_SIZE];
	char password[config::NVR_PASSWORD_SIZE]; // Never sent back by the web API
};

// Check whether a login is complete enough to reach the NVR
inline bool isNvrConfigured(const NvrSettings &settings) {
	return settings.host[0] != '\0' && settings.username[0] != '\0' && settings.password[0] != '\0';
}

// Load the saved login (call once in setup, before the web server starts)
void loadNvrSettings();

// Get a copy of the current login (safe from any task)
NvrSettings getNvrSettings();

// Get the revision of the login, which changes on every update (so the NVR client drops its old session)
uint32_t getNvrSettingsRevision();

// Save a new login and make it current (safe from any task; false, and nothing changes, if it couldn't be saved)
bool updateNvrSettings(const NvrSettings &settings);

#endif