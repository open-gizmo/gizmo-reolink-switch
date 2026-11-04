#include <Arduino.h>
#include <unity.h>

#include "channels/channels.h"

namespace {
	// Masks with nothing muted
	const MutedChannelMasks NO_MUTED_CHANNELS = { 0, 0 };

	// Get the bit of a channel in a mask
	uint32_t channelBit(uint8_t channel) {
		return 1UL << channel;
	}
}

// Nothing to prepare before each test
void setUp() {}

// Nothing to clean up after each test
void tearDown() {}

/******************************** Building commands ********************************/

// Muting sends one push and one recording command per channel
void test_mute_commands_cover_both_settings_of_each_channel() {
	const uint8_t channels[] = { 0, 3 };
	ChannelCommand commands[MAX_CHANNEL_COMMAND_COUNT];

	// Build the mute commands
	const uint8_t commandCount = buildMuteCommands(channels, 2, commands);

	// Expect push then recording for each channel, in order
	TEST_ASSERT_EQUAL_UINT8(4, commandCount);
	TEST_ASSERT_EQUAL_UINT8(0, commands[0].channel);
	TEST_ASSERT_EQUAL(ChannelSetting::Push, commands[0].setting);
	TEST_ASSERT_EQUAL(ChannelSetting::Recording, commands[1].setting);
	TEST_ASSERT_EQUAL_UINT8(3, commands[2].channel);
	TEST_ASSERT_EQUAL(ChannelSetting::Recording, commands[3].setting);
}

// Channels that don't fit in the masks are skipped instead of overflowing the shift
void test_mute_commands_skip_untrackable_channels() {
	const uint8_t channels[] = { 31, 32 };
	ChannelCommand commands[MAX_CHANNEL_COMMAND_COUNT];

	// Build the mute commands
	const uint8_t commandCount = buildMuteCommands(channels, 2, commands);

	// Expect only channel 31
	TEST_ASSERT_EQUAL_UINT8(2, commandCount);
	TEST_ASSERT_EQUAL_UINT8(31, commands[0].channel);
}

// Restoring only sends the settings that were actually muted, on online channels
void test_restore_commands_only_cover_muted_settings_of_online_channels() {
	const uint8_t onlineChannels[] = { 0, 1, 2 };
	const MutedChannelMasks masks = { channelBit(0) | channelBit(1), channelBit(1) | channelBit(5) };
	ChannelCommand commands[MAX_CHANNEL_COMMAND_COUNT];

	// Build the restore commands (channel 5 is offline, channel 2 was never muted)
	const uint8_t commandCount = buildRestoreCommands(onlineChannels, 3, masks, commands);

	// Expect push on 0, then push and recording on 1
	TEST_ASSERT_EQUAL_UINT8(3, commandCount);
	TEST_ASSERT_EQUAL_UINT8(0, commands[0].channel);
	TEST_ASSERT_EQUAL(ChannelSetting::Push, commands[0].setting);
	TEST_ASSERT_EQUAL_UINT8(1, commands[1].channel);
	TEST_ASSERT_EQUAL(ChannelSetting::Push, commands[1].setting);
	TEST_ASSERT_EQUAL_UINT8(1, commands[2].channel);
	TEST_ASSERT_EQUAL(ChannelSetting::Recording, commands[2].setting);
}

/******************************** Mute bookkeeping ********************************/

// Settings are marked as muted before the request, so an interrupted request can still be undone
void test_commands_are_added_to_the_masks_before_muting() {
	const uint8_t channels[] = { 2 };
	ChannelCommand commands[MAX_CHANNEL_COMMAND_COUNT];
	const uint8_t commandCount = buildMuteCommands(channels, 1, commands);

	// Add the commands to empty masks
	const MutedChannelMasks masks = addCommandsToMasks(NO_MUTED_CHANNELS, commands, commandCount);

	// Expect both settings of channel 2 to be marked
	TEST_ASSERT_EQUAL_HEX32(channelBit(2), masks.pushChannelMask);
	TEST_ASSERT_EQUAL_HEX32(channelBit(2), masks.recordingChannelMask);
}

// A setting the NVR refused to mute was never changed, so it must not wait for a restore forever
void test_refused_mute_is_forgotten() {
	const uint8_t channels[] = { 3 };
	ChannelCommand commands[MAX_CHANNEL_COMMAND_COUNT];
	const uint8_t commandCount = buildMuteCommands(channels, 1, commands);
	const MutedChannelMasks attemptedMasks = addCommandsToMasks(NO_MUTED_CHANNELS, commands, commandCount);

	// Push muted, recording refused
	const CommandOutcome outcomes[] = { CommandOutcome::Succeeded, CommandOutcome::Failed };
	const MutedChannelMasks masks = applyMuteOutcomes(attemptedMasks, commands, outcomes, commandCount);

	// Expect only push to stay muted
	TEST_ASSERT_EQUAL_HEX32(channelBit(3), masks.pushChannelMask);
	TEST_ASSERT_EQUAL_HEX32(0, masks.recordingChannelMask);
}

// An unknown mute answer may have changed the setting, so it stays muted to be safe
void test_unknown_mute_answer_stays_muted() {
	const uint8_t channels[] = { 4 };
	ChannelCommand commands[MAX_CHANNEL_COMMAND_COUNT];
	const uint8_t commandCount = buildMuteCommands(channels, 1, commands);
	const MutedChannelMasks attemptedMasks = addCommandsToMasks(NO_MUTED_CHANNELS, commands, commandCount);

	// Both answers unknown
	const CommandOutcome outcomes[] = { CommandOutcome::Unknown, CommandOutcome::Unknown };
	const MutedChannelMasks masks = applyMuteOutcomes(attemptedMasks, commands, outcomes, commandCount);

	// Expect both settings to stay muted
	TEST_ASSERT_TRUE(hasMutedChannels(masks));
	TEST_ASSERT_EQUAL_HEX32(channelBit(4), masks.recordingChannelMask);
}

/******************************** Restore bookkeeping ********************************/

// Only confirmed restores are forgotten; refused and unknown ones are retried
void test_only_confirmed_restores_are_forgotten() {
	const uint8_t onlineChannels[] = { 1, 2 };
	const MutedChannelMasks mutedMasks = { channelBit(1) | channelBit(2), channelBit(1) };
	ChannelCommand commands[MAX_CHANNEL_COMMAND_COUNT];
	const uint8_t commandCount = buildRestoreCommands(onlineChannels, 2, mutedMasks, commands);

	// Commands: push 1 (ok), recording 1 (refused), push 2 (unknown)
	const CommandOutcome outcomes[] = { CommandOutcome::Succeeded, CommandOutcome::Failed, CommandOutcome::Unknown };
	const MutedChannelMasks masks = applyRestoreOutcomes(mutedMasks, commands, outcomes, commandCount);

	// Expect push 2 and recording 1 to stay muted
	TEST_ASSERT_EQUAL_HEX32(channelBit(2), masks.pushChannelMask);
	TEST_ASSERT_EQUAL_HEX32(channelBit(1), masks.recordingChannelMask);
}

// One channel refusing its restore doesn't block the other channels (the old all-or-nothing bug)
void test_one_refused_channel_does_not_block_the_others() {
	const uint8_t onlineChannels[] = { 0, 1, 2 };
	const uint8_t channels[] = { 0, 1, 2 };
	ChannelCommand commands[MAX_CHANNEL_COMMAND_COUNT];
	const uint8_t commandCount = buildMuteCommands(channels, 3, commands);
	const MutedChannelMasks mutedMasks = addCommandsToMasks(NO_MUTED_CHANNELS, commands, commandCount);

	// Restore everything, with channel 1's recording refused
	ChannelCommand restoreCommands[MAX_CHANNEL_COMMAND_COUNT];
	const uint8_t restoreCount = buildRestoreCommands(onlineChannels, 3, mutedMasks, restoreCommands);
	const CommandOutcome outcomes[] = {
		CommandOutcome::Succeeded, CommandOutcome::Succeeded,
		CommandOutcome::Succeeded, CommandOutcome::Failed,
		CommandOutcome::Succeeded, CommandOutcome::Succeeded
	};
	const MutedChannelMasks masks = applyRestoreOutcomes(mutedMasks, restoreCommands, outcomes, restoreCount);

	// Expect only channel 1's recording to stay muted
	TEST_ASSERT_EQUAL_HEX32(0, masks.pushChannelMask);
	TEST_ASSERT_EQUAL_HEX32(channelBit(1), masks.recordingChannelMask);
}

// A full mute and restore cycle leaves nothing muted
void test_full_cycle_leaves_nothing_muted() {
	const uint8_t channels[] = { 0, 7 };
	ChannelCommand commands[MAX_CHANNEL_COMMAND_COUNT];
	const CommandOutcome allSucceeded[] = { CommandOutcome::Succeeded, CommandOutcome::Succeeded, CommandOutcome::Succeeded, CommandOutcome::Succeeded };

	// Mute both channels
	const uint8_t commandCount = buildMuteCommands(channels, 2, commands);
	MutedChannelMasks masks = addCommandsToMasks(NO_MUTED_CHANNELS, commands, commandCount);
	masks = applyMuteOutcomes(masks, commands, allSucceeded, commandCount);

	// Restore both channels
	ChannelCommand restoreCommands[MAX_CHANNEL_COMMAND_COUNT];
	const uint8_t restoreCount = buildRestoreCommands(channels, 2, masks, restoreCommands);
	masks = applyRestoreOutcomes(masks, restoreCommands, allSucceeded, restoreCount);

	// Expect nothing left to restore
	TEST_ASSERT_FALSE(hasMutedChannels(masks));
}

// Counting outcomes drives the "partly muted" and "restore failed" messages
void test_count_command_outcomes() {
	const CommandOutcome outcomes[] = { CommandOutcome::Succeeded, CommandOutcome::Failed, CommandOutcome::Succeeded, CommandOutcome::Unknown };

	// Expect each outcome to be counted
	TEST_ASSERT_EQUAL_UINT8(2, countCommandOutcomes(outcomes, 4, CommandOutcome::Succeeded));
	TEST_ASSERT_EQUAL_UINT8(1, countCommandOutcomes(outcomes, 4, CommandOutcome::Failed));
	TEST_ASSERT_EQUAL_UINT8(1, countCommandOutcomes(outcomes, 4, CommandOutcome::Unknown));
}

// Each setting maps to its Reolink API command
void test_setting_commands() {
	TEST_ASSERT_EQUAL_STRING("SetPushV20", getChannelSettingCommand(ChannelSetting::Push));
	TEST_ASSERT_EQUAL_STRING("SetRecV20", getChannelSettingCommand(ChannelSetting::Recording));
}

// Run all the channel tests
int main() {
	UNITY_BEGIN();

	// Building commands
	RUN_TEST(test_mute_commands_cover_both_settings_of_each_channel);
	RUN_TEST(test_mute_commands_skip_untrackable_channels);
	RUN_TEST(test_restore_commands_only_cover_muted_settings_of_online_channels);

	// Mute bookkeeping
	RUN_TEST(test_commands_are_added_to_the_masks_before_muting);
	RUN_TEST(test_refused_mute_is_forgotten);
	RUN_TEST(test_unknown_mute_answer_stays_muted);

	// Restore bookkeeping
	RUN_TEST(test_only_confirmed_restores_are_forgotten);
	RUN_TEST(test_one_refused_channel_does_not_block_the_others);
	RUN_TEST(test_full_cycle_leaves_nothing_muted);
	RUN_TEST(test_count_command_outcomes);
	RUN_TEST(test_setting_commands);

	return UNITY_END();
}