#include "storage.h"

#include <Preferences.h>

#include "serial/serial.h"

namespace {
	// NVS namespace and keys (keys are limited to 15 characters)
	constexpr char STORAGE_NAMESPACE[] = "reolink";
	constexpr char MUTED_PUSH_KEY[] = "mutedPush";
	constexpr char MUTED_RECORDING_KEY[] = "mutedRec";

	// Keep the NVS handle open and cache the stored values in RAM
	Preferences preferences;
	bool storageReady = false;
	MutedChannelMasks mutedChannelMasks = { 0, 0 };

	// Write one mask to NVS only when its value changes
	bool saveMaskIfChanged(const char *key, uint32_t storedMask, uint32_t newMask) {
		if (storedMask == newMask) return true;
		return preferences.putULong(key, newMask) == sizeof(uint32_t);
	}
}

// Open the NVS namespace and load the saved muted channels
void setupStorage() {
	storageReady = preferences.begin(STORAGE_NAMESPACE, false);

	// If NVS is not available, keep working with the RAM copy only
	if (!storageReady) {
		printLogln("Storage: failed to open NVS, muted channels will not survive a reboot.");
		return;
	}

	// Load the settings that were still muted before the last reboot
	mutedChannelMasks.pushChannelMask = preferences.getULong(MUTED_PUSH_KEY, 0);
	mutedChannelMasks.recordingChannelMask = preferences.getULong(MUTED_RECORDING_KEY, 0);
}

// Get the channels whose push notifications and recording are currently muted
MutedChannelMasks getMutedChannelMasks() {
	return mutedChannelMasks;
}

// Save the muted channels, writing to flash only the masks that changed
bool saveMutedChannelMasks(const MutedChannelMasks &masks) {
	const MutedChannelMasks storedMasks = mutedChannelMasks;

	// Update the RAM copy first, so the firmware keeps the right state even if the flash write fails
	mutedChannelMasks = masks;
	if (!storageReady) return false;

	// Write the masks that changed
	const bool isPushSaved = saveMaskIfChanged(MUTED_PUSH_KEY, storedMasks.pushChannelMask, masks.pushChannelMask);
	const bool isRecordingSaved = saveMaskIfChanged(MUTED_RECORDING_KEY, storedMasks.recordingChannelMask, masks.recordingChannelMask);
	return isPushSaved && isRecordingSaved;
}