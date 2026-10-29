#ifndef REOLINK_MODULE_H
#define REOLINK_MODULE_H

#include <Arduino.h>

enum class ReolinkSetupResult : uint8_t {
	Ready,
	MissingCredentials,
};

enum class ReolinkRestoreResult : uint8_t {
	Restored,
	PartiallyRestored, // Some muted cameras are offline and still need to be restored
	Failed,
};

ReolinkSetupResult setupReolink();
bool disableReolinkNotificationsAndRecording();
ReolinkRestoreResult restoreReolinkNotificationsAndRecording();
bool hasPendingReolinkRestore();

#endif