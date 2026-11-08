#ifndef NVR_SETTINGS_JSON_H
#define NVR_SETTINGS_JSON_H

#include <ArduinoJson.h>

#include "nvr/nvr_settings.h"

// Mapping between the NVR login and the web API JSON, free of hardware headers so it can be tested on the host

// Write the login into a JSON object (the password only as whether one is set)
void writeNvrSettingsToJson(JsonObject json, const NvrSettings &settings);

// Apply a login from the web app, all or nothing (an empty or missing password keeps the saved one, for the same address and username only: a new address or username needs the password again; returns the name of the first invalid field, or nullptr)
const char *applyNvrSettingsFromJson(JsonObjectConst json, NvrSettings &settings);

#endif