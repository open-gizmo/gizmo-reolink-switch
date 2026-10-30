#include "reolink_protocol.h"

namespace {
	// Refresh the token this long before the NVR expires it
	constexpr uint32_t TOKEN_REFRESH_MARGIN_SECONDS = 60;
}

// Compute when a new session token should be refreshed, a minute before the NVR expires it
uint32_t computeTokenExpiryMs(uint32_t loginTimeMs, uint32_t leaseTimeSeconds) {
	// Keep the lease in the range the expiry check can handle
	const uint32_t cappedLeaseSeconds = leaseTimeSeconds > MAX_TOKEN_LEASE_SECONDS ? MAX_TOKEN_LEASE_SECONDS : leaseTimeSeconds;

	// Refresh a minute early, unless the lease is too short for that
	const uint32_t usableLeaseSeconds = cappedLeaseSeconds > TOKEN_REFRESH_MARGIN_SECONDS ? cappedLeaseSeconds - TOKEN_REFRESH_MARGIN_SECONDS : cappedLeaseSeconds;
	return loginTimeMs + (usableLeaseSeconds * 1000UL);
}

// Return whether the token expiry time has been reached (works across millis() rollover)
bool isTokenExpired(uint32_t currentTimeMs, uint32_t expiryTimeMs) {
	return static_cast<int32_t>(currentTimeMs - expiryTimeMs) >= 0;
}

// Check the results of a request: an expired session always fails it, a refused command only if every command must succeed
ReplyCheck checkReplyResults(JsonArrayConst results, bool requireAllCommandsSucceeded) {
	// No result is an unreadable answer
	if (results.isNull() || results.size() == 0) {
		return { ReplyStatus::Empty, 0, nullptr, nullptr };
	}

	// Look for the first command that matters
	for (JsonObjectConst result : results) {
		const int resultCode = result["code"] | -1;
		if (resultCode == 0) continue;

		// Read what the NVR said about the failed command
		const int errorCode = result["error"]["rspCode"] | resultCode;
		const char *failedCommand = result["cmd"] | static_cast<const char *>(nullptr);
		const char *detail = result["error"]["detail"] | static_cast<const char *>(nullptr);

		// An expired session fails the whole request, so the caller can log in again and retry
		if (errorCode == SESSION_EXPIRED_CODE) {
			return { ReplyStatus::SessionExpired, errorCode, failedCommand, detail };
		}

		// Fail the whole request unless the caller reads the result of each command itself
		if (requireAllCommandsSucceeded) {
			return { ReplyStatus::CommandRefused, errorCode, failedCommand, detail };
		}
	}

	// The NVR answered every command
	return { ReplyStatus::Ok, 0, nullptr, nullptr };
}

// Read the outcome of each command of a batch (the NVR answers in request order)
void readCommandOutcomes(JsonArrayConst results, const ChannelCommand *commands, uint8_t commandCount, CommandOutcome *outcomes) {
	// Every outcome is unknown until the NVR answers it
	for (uint8_t index = 0; index < commandCount; index += 1) {
		outcomes[index] = CommandOutcome::Unknown;
	}

	// Read the answers, one per command
	uint8_t index = 0;
	for (JsonObjectConst result : results) {
		if (index >= commandCount) break;

		// Keep the outcome unknown if the answer is for another command (the setting may or may not have changed)
		const char *expectedCommand = getChannelSettingCommand(commands[index].setting);
		const char *answeredCommand = result["cmd"] | expectedCommand;
		if (strcmp(answeredCommand, expectedCommand) == 0) {
			outcomes[index] = (result["code"] | -1) == 0 ? CommandOutcome::Succeeded : CommandOutcome::Failed;
		}
		index += 1;
	}
}