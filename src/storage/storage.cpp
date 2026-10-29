#include "storage.h"

#include <Preferences.h>

#include "serial/serial.h"

namespace {
	// NVS namespace and key (keys are limited to 15 characters)
	constexpr char STORAGE_NAMESPACE[] = "reolink";
	constexpr char MUTED_CHANNELS_KEY[] = "mutedChannels";

	// Keep the NVS handle open and cache the stored value in RAM
	Preferences preferences;
	bool storageReady = false;
	uint32_t mutedChannelMask = 0;
}

// Open the NVS namespace and load the saved muted channels
void setupStorage() {
	storageReady = preferences.begin(STORAGE_NAMESPACE, false);

	// If NVS is not available, keep working with the RAM copy only
	if (!storageReady) {
		printLogln("Storage: failed to open NVS, muted channels will not survive a reboot.");
		return;
	}

	// Load the channels that were still muted before the last reboot
	mutedChannelMask = preferences.getULong(MUTED_CHANNELS_KEY, 0);
}

// Get the channels that are currently muted (one bit per channel)
uint32_t getMutedChannelMask() {
	return mutedChannelMask;
}

// Save the channels that are currently muted, writing to flash only when the value changes
bool saveMutedChannelMask(uint32_t channelMask) {
	// Nothing to write if the value is unchanged
	if (channelMask == mutedChannelMask) return true;

	// Update the RAM copy first, so the firmware keeps the right state even if the flash write fails
	mutedChannelMask = channelMask;
	if (!storageReady) return false;

	// Write the new value to NVS
	return preferences.putULong(MUTED_CHANNELS_KEY, channelMask) == sizeof(uint32_t);
}