#ifndef REOLINK_MODULE_H
#define REOLINK_MODULE_H

#include <Arduino.h>

enum class ReolinkSetupResult : uint8_t {
	Ready,
	MissingCredentials,
};

ReolinkSetupResult setupReolink();
bool disableReolinkNotificationsAndRecording();
bool restoreReolinkNotificationsAndRecording();

#endif