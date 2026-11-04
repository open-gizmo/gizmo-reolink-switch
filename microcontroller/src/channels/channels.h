#ifndef CHANNELS_H
#define CHANNELS_H

#include <Arduino.h>

// Muted channels are tracked as 32-bit masks, one bit per channel
constexpr uint8_t MAX_TRACKED_CHANNEL_COUNT = 32;

// Each channel takes at most one command per setting
constexpr uint8_t MAX_CHANNEL_COMMAND_COUNT = MAX_TRACKED_CHANNEL_COUNT * 2;

// Camera settings the switch mutes and restores
enum class ChannelSetting : uint8_t {
	Push,
	Recording,
};

// One command for one setting of one channel
struct ChannelCommand {
	uint8_t channel;
	ChannelSetting setting;
};

// What the NVR answered for one command
enum class CommandOutcome : uint8_t {
	Succeeded,
	Failed,
	Unknown, // Missing or unexpected answer, so the setting may or may not have changed
};

// Channels whose push notifications and recording are currently muted
struct MutedChannelMasks {
	uint32_t pushChannelMask;
	uint32_t recordingChannelMask;
};

const char *getChannelSettingCommand(ChannelSetting setting);
bool hasMutedChannels(const MutedChannelMasks &masks);
uint8_t buildMuteCommands(const uint8_t *channels, uint8_t channelCount, ChannelCommand *commands);
uint8_t buildRestoreCommands(const uint8_t *onlineChannels, uint8_t onlineChannelCount, const MutedChannelMasks &masks, ChannelCommand *commands);
MutedChannelMasks addCommandsToMasks(const MutedChannelMasks &masks, const ChannelCommand *commands, uint8_t commandCount);
MutedChannelMasks applyMuteOutcomes(const MutedChannelMasks &masks, const ChannelCommand *commands, const CommandOutcome *outcomes, uint8_t commandCount);
MutedChannelMasks applyRestoreOutcomes(const MutedChannelMasks &masks, const ChannelCommand *commands, const CommandOutcome *outcomes, uint8_t commandCount);
uint8_t countCommandOutcomes(const CommandOutcome *outcomes, uint8_t commandCount, CommandOutcome outcome);

#endif