#ifndef REOLINK_MODULE_H
#define REOLINK_MODULE_H

#include <Arduino.h>

#include "remote/remote_types.h"

// Outcome of muting the cameras
enum class ReolinkMuteResult : uint8_t {
	Muted,
	PartiallyMuted, // Some settings were refused, the others are muted
	Failed,
};

// Outcome of restoring the cameras
enum class ReolinkRestoreResult : uint8_t {
	Restored,
	PartiallyRestored, // Some muted cameras are offline and still need to be restored
	Failed,
};

// Load the NVR login (the connection is checked once the Wi-Fi is up, with testReolinkConnection)
void setupReolink();

// Drop the session and the login pause, after the login changed in the web app
void resetReolinkSession();

// Log in and count the online cameras, showing the outcome on the LCD
NvrStatus testReolinkConnection();

// Get the last known state of the NVR connection
NvrStatus getReolinkStatus();

// Get the online cameras at the last check
uint8_t getReolinkCameraCount();

// Disable notifications and recording for all active channels
ReolinkMuteResult disableReolinkNotificationsAndRecording();

// Restore notifications and recording for the muted settings of the online channels
ReolinkRestoreResult restoreReolinkNotificationsAndRecording();

// Check whether some settings are still muted (a failed or partial restore, or a reboot while muted)
bool hasPendingReolinkRestore();

#endif