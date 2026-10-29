#include "channels.h"

namespace {
	// Get the bit of a channel in a mask
	uint32_t getChannelBit(uint8_t channel) {
		return 1UL << channel;
	}

	// Get the mask that tracks a setting
	uint32_t &getSettingMask(MutedChannelMasks &masks, ChannelSetting setting) {
		return setting == ChannelSetting::Push ? masks.pushChannelMask : masks.recordingChannelMask;
	}

	// Check whether a setting of a channel is muted
	bool isSettingMuted(const MutedChannelMasks &masks, uint8_t channel, ChannelSetting setting) {
		const uint32_t settingMask = setting == ChannelSetting::Push ? masks.pushChannelMask : masks.recordingChannelMask;
		return (settingMask & getChannelBit(channel)) != 0;
	}

	// Remove the commands with the given outcome from the masks
	MutedChannelMasks removeCommandsWithOutcome(const MutedChannelMasks &masks, const ChannelCommand *commands, const CommandOutcome *outcomes, uint8_t commandCount, CommandOutcome outcomeToRemove) {
		MutedChannelMasks updatedMasks = masks;

		// Clear the bit of each command with the matching outcome
		for (uint8_t index = 0; index < commandCount; index += 1) {
			if (outcomes[index] != outcomeToRemove) continue;
			getSettingMask(updatedMasks, commands[index].setting) &= ~getChannelBit(commands[index].channel);
		}

		// Return the updated masks
		return updatedMasks;
	}
}

// Get the Reolink API command that changes a setting
const char *getChannelSettingCommand(ChannelSetting setting) {
	return setting == ChannelSetting::Push ? "SetPushV20" : "SetRecV20";
}

// Check whether any setting of any channel is still muted
bool hasMutedChannels(const MutedChannelMasks &masks) {
	return masks.pushChannelMask != 0 || masks.recordingChannelMask != 0;
}

// Build the commands that mute push notifications and recording on the given channels
uint8_t buildMuteCommands(const uint8_t *channels, uint8_t channelCount, ChannelCommand *commands) {
	uint8_t commandCount = 0;

	// Add one command per setting for each trackable channel
	for (uint8_t index = 0; index < channelCount; index += 1) {
		if (channels[index] >= MAX_TRACKED_CHANNEL_COUNT) continue; // Skip channels that don't fit in the masks
		commands[commandCount] = { channels[index], ChannelSetting::Push };
		commands[commandCount + 1] = { channels[index], ChannelSetting::Recording };
		commandCount += 2;
	}

	// Return the number of commands
	return commandCount;
}

// Build the commands that restore the muted settings of the online channels
uint8_t buildRestoreCommands(const uint8_t *onlineChannels, uint8_t onlineChannelCount, const MutedChannelMasks &masks, ChannelCommand *commands) {
	uint8_t commandCount = 0;

	// Add a command for each muted setting of each online channel
	for (uint8_t index = 0; index < onlineChannelCount; index += 1) {
		const uint8_t channel = onlineChannels[index];
		if (channel >= MAX_TRACKED_CHANNEL_COUNT) continue; // Skip channels that don't fit in the masks

		// Restore push notifications if they were muted
		if (isSettingMuted(masks, channel, ChannelSetting::Push)) {
			commands[commandCount] = { channel, ChannelSetting::Push };
			commandCount += 1;
		}

		// Restore recording if it was muted
		if (isSettingMuted(masks, channel, ChannelSetting::Recording)) {
			commands[commandCount] = { channel, ChannelSetting::Recording };
			commandCount += 1;
		}
	}

	// Return the number of commands
	return commandCount;
}

// Mark the settings of the commands as muted (saved before muting, so an interrupted request can still be undone)
MutedChannelMasks addCommandsToMasks(const MutedChannelMasks &masks, const ChannelCommand *commands, uint8_t commandCount) {
	MutedChannelMasks updatedMasks = masks;

	// Set the bit of each command
	for (uint8_t index = 0; index < commandCount; index += 1) {
		getSettingMask(updatedMasks, commands[index].setting) |= getChannelBit(commands[index].channel);
	}

	// Return the updated masks
	return updatedMasks;
}

// Forget the settings the NVR refused to mute (unknown answers stay muted, to be safe)
MutedChannelMasks applyMuteOutcomes(const MutedChannelMasks &masks, const ChannelCommand *commands, const CommandOutcome *outcomes, uint8_t commandCount) {
	return removeCommandsWithOutcome(masks, commands, outcomes, commandCount, CommandOutcome::Failed);
}

// Forget the settings the NVR confirmed as restored (failed and unknown answers stay muted, to retry)
MutedChannelMasks applyRestoreOutcomes(const MutedChannelMasks &masks, const ChannelCommand *commands, const CommandOutcome *outcomes, uint8_t commandCount) {
	return removeCommandsWithOutcome(masks, commands, outcomes, commandCount, CommandOutcome::Succeeded);
}

// Count the commands with a given outcome
uint8_t countCommandOutcomes(const CommandOutcome *outcomes, uint8_t commandCount, CommandOutcome outcome) {
	uint8_t matchingCount = 0;

	// Count each matching outcome
	for (uint8_t index = 0; index < commandCount; index += 1) {
		if (outcomes[index] == outcome) matchingCount += 1;
	}

	// Return the count
	return matchingCount;
}