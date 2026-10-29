#ifndef REOLINK_MODULE_H
#define REOLINK_MODULE_H

#include <Arduino.h>

enum class ReolinkSetupResult : uint8_t {
	Ready,
	MissingCredentials,
};

enum class ReolinkMuteResult : uint8_t {
	Muted,
	PartiallyMuted, // Some settings were refused, the others are muted
	Failed,
};

enum class ReolinkRestoreResult : uint8_t {
	Restored,
	PartiallyRestored, // Some muted cameras are offline and still need to be restored
	Failed,
};

ReolinkSetupResult setupReolink();
ReolinkMuteResult disableReolinkNotificationsAndRecording();
ReolinkRestoreResult restoreReolinkNotificationsAndRecording();
bool hasPendingReolinkRestore();

#endif