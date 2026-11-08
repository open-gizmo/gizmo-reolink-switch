#ifndef REMOTE_JSON_H
#define REMOTE_JSON_H

#include <ArduinoJson.h>

#include "remote/remote_types.h"

// Mapping between the switch state and the web API JSON, free of hardware headers so it can be tested on the host

// Get the web API name of an NVR status
const char *getNvrStatusName(NvrStatus status);

// Write the switch state into a JSON object
void writeSwitchStatusToJson(JsonObject json, const SwitchStatus &status);

#endif