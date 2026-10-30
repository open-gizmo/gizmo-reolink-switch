#ifndef REOLINK_PROTOCOL_H
#define REOLINK_PROTOCOL_H

#include <Arduino.h>
#include <ArduinoJson.h>

#include "channels/channels.h"

// Hardware-free parts of the Reolink API (reading the replies and the session lease), so the native tests can check them

// Reolink error code for a missing or expired session
constexpr int SESSION_EXPIRED_CODE = -6;

// Lease used when the NVR doesn't send one, and the longest one trusted (the expiry check works up to 24 days ahead)
constexpr uint32_t DEFAULT_TOKEN_LEASE_SECONDS = 3600;
constexpr uint32_t MAX_TOKEN_LEASE_SECONDS = 24UL * 3600UL;

// How the NVR answered a whole request
enum class ReplyStatus : uint8_t {
	Ok, // Every command answered (in batch mode, some may still have been refused)
	Empty, // No result at all
	SessionExpired, // The session token is missing or expired, so log in again and retry
	CommandRefused, // A command was refused (only when every command must succeed)
};

// The reply status and, for a refused command, what the NVR said about it
struct ReplyCheck {
	ReplyStatus status;
	int errorCode; // Reolink error of the refused command (0 if none)
	const char *failedCommand; // Name of the refused command (nullptr if none or unnamed)
	const char *detail; // Error detail from the NVR (nullptr if none)
};

uint32_t computeTokenExpiryMs(uint32_t loginTimeMs, uint32_t leaseTimeSeconds);
bool isTokenExpired(uint32_t currentTimeMs, uint32_t expiryTimeMs);
ReplyCheck checkReplyResults(JsonArrayConst results, bool requireAllCommandsSucceeded);
void readCommandOutcomes(JsonArrayConst results, const ChannelCommand *commands, uint8_t commandCount, CommandOutcome *outcomes);

#endif