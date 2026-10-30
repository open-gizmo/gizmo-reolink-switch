#include "reolink.h"

#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>

#include "config/config.h"
#include "display/display.h"
#include "channels/channels.h"
#include "reolink_protocol.h"
#include "serial/serial.h"
#include "storage/storage.h"
#include "wifi/wifi.h"

namespace {
	// How a request to the NVR ended
	enum class RequestResult : uint8_t {
		Ok,
		NetworkError, // The NVR could not be reached or sent an unreadable answer
		CommandError, // The NVR answered but refused a command
	};

	// Keep the current session token and its expiry time in memory
	String sessionToken;
	uint32_t tokenExpiryTimeMs = 0;

	// Login pause after the NVR refuses a login (repeated wrong-password logins can lock the account)
	bool isLoginPaused = false;
	uint32_t loginPausedAtMs = 0;

	// One HTTPS connection kept open for all the requests of a mute or restore, so they share a single TLS handshake
	WiFiClientSecure secureClient;
	HTTPClient http;

	// Collects a reply body in a String, refusing anything past its size limit (so a broken reply can't eat the heap TLS needs)
	class BoundedStringStream : public Stream {
	public:
		BoundedStringStream(String &output, size_t maxLength) : output(output), maxLength(maxLength) {}

		size_t write(uint8_t value) override {
			return write(&value, 1);
		}

		size_t write(const uint8_t *buffer, size_t size) override {
			// Refuse the bytes past the limit (the HTTP client then stops the read and closes the connection)
			if (output.length() + size > maxLength) {
				setWriteError();
				return 0;
			}

			// Append the bytes
			output.concat(reinterpret_cast<const char *>(buffer), size);
			return size;
		}

		// Nothing to read back (the body is read from the String)
		int available() override { return 0; }
		int read() override { return -1; }
		int peek() override { return -1; }

	private:
		String &output;
		size_t maxLength;
	};

	// Close the connection to the NVR (at the end of each mute or restore, so no idle TLS session holds memory)
	void closeReolinkConnection() {
		http.end();
		secureClient.stop();
	}

	// Closes the NVR connection when a mute or restore returns, whatever path it takes
	struct ReolinkConnectionScope {
		~ReolinkConnectionScope() {
			closeReolinkConnection();
		}
	};

	// Show a short user-friendly NVR status on the LCD
	void showReolinkStatus(const char *line1, const char *line2) {
		displayShowScreen(line1, line2);
	}

	// Check whether all required Reolink credentials are configured
	bool hasReolinkCredentials() {
		return config::REOLINK_HOST[0] != '\0' && config::REOLINK_USERNAME[0] != '\0' && config::REOLINK_PASSWORD[0] != '\0';
	}

	// Encode credentials so they are safe to include in the query string
	String urlEncode(const char *value) {
		String encoded;

		// Encode each character in the input string
		while (*value != '\0') {
			const unsigned char current = static_cast<unsigned char>(*value);

			// Encode alphanumeric characters and a few safe symbols directly, otherwise percent-encode
			if ((current >= 'a' && current <= 'z') || (current >= 'A' && current <= 'Z') || (current >= '0' && current <= '9') || current == '-' || current == '_' || current == '.' || current == '~') {
				encoded += static_cast<char>(current);
			} else {
				static const char hexDigits[] = "0123456789ABCDEF";
				encoded += '%';
				encoded += hexDigits[(current >> 4) & 0x0F];
				encoded += hexDigits[current & 0x0F];
			}

			// Move to the next character in the input string
			value += 1;
		}

		// Return the encoded string
		return encoded;
	}

	// Build the HTTPS API endpoint with one authentication query parameter
	String buildApiUrl(const char *command, const char *authKey, const char *authValue) {
		String url = "https://";
		url += config::REOLINK_HOST;
		url += "/api.cgi?cmd=";
		url += command;
		url += "&";
		url += authKey;
		url += "=";
		url += urlEncode(authValue);
		return url;
	}

	// Return whether the current token is missing or about to expire
	bool tokenNeedsRefresh() {
		// If there is no token, it needs to be refreshed
		if (sessionToken.length() == 0) {
			return true;
		}

		// If the token has expired or is about to expire, it needs to be refreshed
		return isTokenExpired(millis(), tokenExpiryTimeMs);
	}

	// Append one API command to a request array
	void appendApiRequest(JsonArray requestArray, const char *command, JsonVariantConst params) {
		JsonObject requestObject = requestArray.add<JsonObject>();
		requestObject["cmd"] = command;
		requestObject["action"] = 0;
		requestObject["param"] = params;
	}

	// Read the reply body, up to the size limit (an oversized or broken reply closes the connection)
	bool readReplyBody(String &responseBody) {
		// Refuse a reply announced as too large without reading it
		const int announcedSize = http.getSize();
		if (announcedSize > static_cast<int>(config::REOLINK_MAX_REPLY_BYTES)) {
			printLogfln("Reolink: reply too large (%d bytes)", announcedSize);
			secureClient.stop();
			return false;
		}

		// Read the body (chunked replies are decoded by the HTTP client)
		if (announcedSize > 0) responseBody.reserve(announcedSize);
		BoundedStringStream bodyStream(responseBody, config::REOLINK_MAX_REPLY_BYTES);
		const int readResult = http.writeToStream(&bodyStream);
		if (readResult < 0) {
			printLogfln("Reolink: failed to read the reply (%s)", HTTPClient::errorToString(readResult).c_str());
			return false;
		}

		// Body read
		return true;
	}

	// Check whether a request failed before any answer, as when the NVR closed the kept connection meanwhile
	bool isClosedConnectionError(int statusCode) {
		return statusCode == HTTPC_ERROR_SEND_HEADER_FAILED || statusCode == HTTPC_ERROR_SEND_PAYLOAD_FAILED || statusCode == HTTPC_ERROR_CONNECTION_LOST;
	}

	// Send the POST request once, on the kept connection if there is one
	int sendPostOnce(const String &url, const char *command, const String &requestBody) {
		// Open the HTTP connection to the Reolink API endpoint (or reuse the open one)
		if (!http.begin(secureClient, url)) {
			printLogfln("Reolink: failed to open HTTP client for %s", command);
			return HTTPC_ERROR_CONNECTION_REFUSED;
		}

		// Set the HTTP timeout and content type for the request, and send it
		http.setTimeout(config::REOLINK_HTTP_TIMEOUT_MS);
		http.addHeader("Content-Type", "application/json");
		return http.POST(requestBody);
	}

	// Send the POST request, again on a new connection if the NVR closed the kept one (the commands are safe to send twice)
	int postReolinkRequest(const String &url, const char *command, const String &requestBody) {
		// Reolink NVRs commonly use self-signed HTTPS certificates
		secureClient.setInsecure();

		// Send the request, and stop unless the kept connection was closed before the NVR answered
		const bool isReusingConnection = secureClient.connected();
		const int statusCode = sendPostOnce(url, command, requestBody);
		if (!isReusingConnection || !isClosedConnectionError(statusCode)) {
			return statusCode;
		}

		// Send it again on a new connection
		printLogfln("Reolink: the NVR closed the connection, reconnecting for %s", command);
		http.end();
		secureClient.stop();
		return sendPostOnce(url, command, requestBody);
	}

	// Send one HTTPS API request with a prebuilt JSON array body and parse the standard Reolink envelope
	RequestResult sendReolinkRequest(const String &url, const char *command, JsonDocument &requestDoc, JsonDocument &responseDoc, int &responseCode, bool requireAllCommandsSucceeded = true) {
		// The API expects a JSON array even for a single command
		String requestBody;
		serializeJson(requestDoc, requestBody);

		// Send the HTTP POST request and read the response code and body
		const int statusCode = postReolinkRequest(url, command, requestBody);
		String responseBody;
		const bool isBodyRead = statusCode > 0 && readReplyBody(responseBody);
		http.end();

		// Check for HTTP errors
		if (statusCode <= 0) {
			printLogfln("Reolink: HTTP request failed for %s (code %d)", command, statusCode);
			showReolinkStatus("NVR Link Error", "Check network!");
			responseCode = statusCode;
			return RequestResult::NetworkError;
		}

		// Check for non-successful HTTP status codes
		if (statusCode < 200 || statusCode >= 300) {
			printLogfln("Reolink: HTTP status %d for %s", statusCode, command);
			printLogfln("Reolink response: %s", responseBody.c_str());
			showReolinkStatus("NVR Busy", "Try again!");
			responseCode = statusCode;
			return RequestResult::NetworkError;
		}

		// Check the body was read in full
		if (!isBodyRead) {
			showReolinkStatus("NVR Reply Err!", "Try again!");
			responseCode = -1;
			return RequestResult::NetworkError;
		}

		// Parse the JSON response body into a document
		const DeserializationError jsonError = deserializeJson(responseDoc, responseBody);

		// Check for JSON parsing errors
		if (jsonError) {
			printLogfln("Reolink: failed to parse JSON for %s: %s", command, jsonError.c_str());
			showReolinkStatus("NVR Reply Err!", "Try again!");
			responseCode = -1;
			return RequestResult::NetworkError;
		}

		// Check the results of the commands
		const ReplyCheck replyCheck = checkReplyResults(responseDoc.as<JsonArrayConst>(), requireAllCommandsSucceeded);
		switch (replyCheck.status) {
			// No result at all
			case ReplyStatus::Empty:
				printLogfln("Reolink: empty API result for %s", command);
				showReolinkStatus("NVR Reply Err!", "Empty answer");
				responseCode = -1;
				return RequestResult::NetworkError;

			// An expired session fails the whole request, so the caller can log in again and retry
			case ReplyStatus::SessionExpired:
				printLogfln("Reolink: %s failed, the session expired", replyCheck.failedCommand ? replyCheck.failedCommand : command);
				responseCode = replyCheck.errorCode;
				return RequestResult::CommandError;

			// A refused command fails the whole request
			case ReplyStatus::CommandRefused:
				printLogfln("Reolink: %s failed (%d): %s", replyCheck.failedCommand ? replyCheck.failedCommand : command, replyCheck.errorCode, replyCheck.detail ? replyCheck.detail : "unknown error");
				showReolinkStatus("NVR Command Err!", "Try again!");
				responseCode = replyCheck.errorCode;
				return RequestResult::CommandError;

			// The NVR answered every command
			case ReplyStatus::Ok:
			default:
				responseCode = 0;
				return RequestResult::Ok;
		}
	}

	// Log in once and cache the returned token for later API calls
	bool loginReolink() {
		// Don't retry too soon after the NVR refused a login
		if (isLoginPaused && millis() - loginPausedAtMs < config::REOLINK_LOGIN_PAUSE_MS) {
			printLogln("Reolink: login paused after a refused login.");
			showReolinkStatus("Login Paused", "Check password");
			delay(config::SHOW_DISPLAY_MESSAGE_MS);
			return false;
		}
		isLoginPaused = false;

		// Create a JSON document with the login parameters
		JsonDocument requestDoc;
		JsonArray requestArray = requestDoc.to<JsonArray>();
		JsonObject requestObject = requestArray.add<JsonObject>();
		requestObject["cmd"] = "Login";
		requestObject["action"] = 0;
		JsonObject paramsDoc = requestObject["param"].to<JsonObject>();
		JsonObject userObject = paramsDoc["User"].to<JsonObject>();
		userObject["userName"] = config::REOLINK_USERNAME;
		userObject["password"] = config::REOLINK_PASSWORD;

		// Prepare a JSON document to hold the response and a variable for the response code
		JsonDocument responseDoc;
		int responseCode = 0;
		const String loginUrl = buildApiUrl("Login", "token", "null");

		// Send the login request and check for errors
		const RequestResult loginResult = sendReolinkRequest(loginUrl, "Login", requestDoc, responseDoc, responseCode);
		if (loginResult != RequestResult::Ok) {
			sessionToken = "";
			tokenExpiryTimeMs = 0;

			// Pause logins if the NVR refused this one (wrong credentials, too many sessions...)
			if (loginResult == RequestResult::CommandError) {
				printLogfln("Reolink: login refused (code %d), pausing logins.", responseCode);
				showReolinkStatus("NVR Login Err!", "Check password");
				delay(config::SHOW_DISPLAY_MESSAGE_MS);
				isLoginPaused = true;
				loginPausedAtMs = millis();
			}

			// Login failed
			return false;
		}

		// Extract the token name and lease time from the response, using default values if missing
		const char *tokenName = responseDoc[0]["value"]["Token"]["name"] | "";
		const uint32_t leaseTimeSeconds = responseDoc[0]["value"]["Token"]["leaseTime"] | DEFAULT_TOKEN_LEASE_SECONDS;

		// If the token name is empty, treat it as a failed login and clear the session token
		if (tokenName[0] == '\0') {
			printLogln("Reolink: login succeeded but returned no token.");
			showReolinkStatus("NVR Login Err!", "Token missing!");
			sessionToken = "";
			tokenExpiryTimeMs = 0;
			return false;
		}

		// Cache the session token and when to refresh it (a minute before the NVR expires it)
		sessionToken = tokenName;
		tokenExpiryTimeMs = computeTokenExpiryMs(millis(), leaseTimeSeconds);
		return true;
	}

	// Ensure a valid token exists before making token-protected API calls
	bool ensureReolinkToken() {
		// If the token is still valid, no need to refresh it
		if (!tokenNeedsRefresh()) {
			return true;
		}

		// If the token is missing or expired, attempt to log in again to acquire a new token
		return loginReolink();
	}

	// Send one prepared request document with token auth and retry once on auth failure
	RequestResult executeReolinkRequest(const char *command, JsonDocument &requestDoc, JsonDocument &responseDoc, bool requireAllCommandsSucceeded = true) {
		// Ensure a valid session token is available before making the API call (the login shows its own errors)
		if (!ensureReolinkToken()) {
			printLogln("Reolink: unable to acquire a session token.");
			return RequestResult::NetworkError;
		}

		// Create the API URL
		int responseCode = 0;
		String url = buildApiUrl(command, "token", sessionToken.c_str());

		// Send the API request and return unless the session expired
		const RequestResult result = sendReolinkRequest(url, command, requestDoc, responseDoc, responseCode, requireAllCommandsSucceeded);
		if (result != RequestResult::CommandError || responseCode != SESSION_EXPIRED_CODE) {
			return result;
		}

		// Log that the session has expired and attempt to log in again
		printLogln("Reolink: session expired, logging in again.");
		showReolinkStatus("NVR Reconnect!", "Please wait!");
		sessionToken = "";
		tokenExpiryTimeMs = 0;

		// Attempt to log in again to acquire a new session token
		if (!loginReolink()) {
			return RequestResult::NetworkError;
		}

		// Retry the API call with the new session token
		url = buildApiUrl(command, "token", sessionToken.c_str());
		return sendReolinkRequest(url, command, requestDoc, responseDoc, responseCode, requireAllCommandsSucceeded);
	}

	// Send a token-authenticated Reolink API call and retry once on auth failure
	bool callReolinkApi(const char *command, JsonVariantConst params, JsonDocument &responseDoc) {
		JsonDocument requestDoc;
		JsonArray requestArray = requestDoc.to<JsonArray>();
		appendApiRequest(requestArray, command, params);
		return executeReolinkRequest(command, requestDoc, responseDoc) == RequestResult::Ok;
	}

	// Query the NVR and keep only currently online channels (the list can be empty)
	bool getActiveChannels(uint8_t *channels, uint8_t &channelCount) {
		// Create a JSON document for the request parameters
		JsonDocument paramsDoc;
		paramsDoc.to<JsonObject>();

		// Create a JSON document for the response
		JsonDocument responseDoc;

		// Call the Reolink API to get the channel status and check for errors
		channelCount = 0;
		if (!callReolinkApi("GetChannelstatus", paramsDoc.as<JsonVariantConst>(), responseDoc)) {
			return false;
		}

		// Get the array of channel status objects
		JsonArray statusArray = responseDoc[0]["value"]["status"].as<JsonArray>();

		// Iterate through the status array and keep only online channels, up to the maximum allowed
		for (JsonObject statusEntry : statusArray) {
			if ((statusEntry["online"] | 0) != 1 || channelCount >= config::REOLINK_MAX_CHANNELS) {
				continue;
			}

			// Skip channels that don't fit in the saved muted-channels masks
			const uint8_t channel = static_cast<uint8_t>(statusEntry["channel"] | channelCount);
			if (channel >= MAX_TRACKED_CHANNEL_COUNT) {
				printLogfln("Reolink: channel %u is not supported and will be skipped", channel);
				continue;
			}

			// Keep the online channel
			channels[channelCount] = channel;
			channelCount += 1;
		}

		// The API call succeeded (even if no channel is online)
		return true;
	}

	// Append the command that changes one setting of one channel to a batch request
	void appendChannelSettingRequest(JsonArray requestArray, const ChannelCommand &command, bool enabled) {
		JsonDocument paramsDoc;
		JsonObject params = paramsDoc.to<JsonObject>();

		// Set the channel and the new state of the setting
		JsonObject settingObject = params[command.setting == ChannelSetting::Push ? "Push" : "Rec"].to<JsonObject>();
		settingObject["channel"] = command.channel;
		settingObject["enable"] = enabled ? 1 : 0;
		settingObject["scheduleEnable"] = enabled ? 1 : 0;

		// Add the command to the batch
		appendApiRequest(requestArray, getChannelSettingCommand(command.setting), params);
	}

	// Send the channel commands in a single batch and read the outcome of each one
	bool applyChannelCommands(const ChannelCommand *commands, uint8_t commandCount, bool enabled, CommandOutcome *outcomes) {
		JsonDocument requestDoc;
		JsonArray requestArray = requestDoc.to<JsonArray>();

		// Append one request per channel setting to the batch
		for (uint8_t index = 0; index < commandCount; index += 1) {
			appendChannelSettingRequest(requestArray, commands[index], enabled);
		}

		// Log the batch request and send it to the Reolink API
		printLogfln("Reolink: sending %u commands to turn %s notifications and recording...", commandCount, enabled ? "on" : "off");
		showReolinkStatus(enabled ? "Quiet Mode OFF" : "Quiet Mode ON", enabled ? "Restoring alerts" : "Muting cameras");

		// Send the batch, reading the result of each command instead of failing the whole batch
		JsonDocument responseDoc;
		if (executeReolinkRequest(getChannelSettingCommand(commands[0].setting), requestDoc, responseDoc, false) != RequestResult::Ok) {
			printLogfln("Reolink: failed to %s notifications and recording.", enabled ? "restore" : "disable");
			showReolinkStatus("Camera Sync Fail", "Try again!");
			delay(config::SHOW_DISPLAY_MESSAGE_MS);
			return false;
		}

		// Read the answer to each command, and log the ones that didn't succeed
		readCommandOutcomes(responseDoc.as<JsonArrayConst>(), commands, commandCount, outcomes);
		for (uint8_t index = 0; index < commandCount; index += 1) {
			const char *command = getChannelSettingCommand(commands[index].setting);
			if (outcomes[index] == CommandOutcome::Failed) {
				printLogfln("Reolink: %s refused on channel %u: %s", command, commands[index].channel, responseDoc[index]["error"]["detail"] | "unknown error");
			} else if (outcomes[index] == CommandOutcome::Unknown) {
				printLogfln("Reolink: no answer for %s on channel %u", command, commands[index].channel);
			}
		}

		// The NVR answered
		return true;
	}
}

// Setup the Reolink integration, check the credentials and try a first login
ReolinkSetupResult setupReolink() {
	// Check if the required Reolink credentials are configured
	if (!hasReolinkCredentials()) {
		printLogln("Reolink credentials are not configured.");
		showReolinkStatus("NVR Needed", "Add credentials!");
		return ReolinkSetupResult::MissingCredentials;
	}

	// Clear any existing session token and expiry time to ensure a fresh login
	sessionToken = "";
	tokenExpiryTimeMs = 0;
	ReolinkConnectionScope connectionScope;

	// Without Wi-Fi the NVR can't be checked yet
	if (!isWiFiConnected()) {
		printLogln("Reolink configured, NVR not checked (no Wi-Fi).");
		showReolinkStatus("NVR Configured", "Wi-Fi pending");
		delay(config::SHOW_DISPLAY_MESSAGE_MS);
		return ReolinkSetupResult::Ready;
	}

	// Log in once to check the NVR and the credentials (the login shows its own errors)
	if (!ensureReolinkToken()) {
		printLogln("Reolink configured, but the first login failed.");
		delay(config::SHOW_DISPLAY_MESSAGE_MS);
		return ReolinkSetupResult::Ready;
	}

	// All OK, the NVR answered and accepted the credentials
	printLogln("Reolink linked.");
	showReolinkStatus("NVR Linked", "Ready to mute!");
	return ReolinkSetupResult::Ready;
}

// Disable notifications and recording for all active channels
ReolinkMuteResult disableReolinkNotificationsAndRecording() {
	uint8_t channels[config::REOLINK_MAX_CHANNELS] = {0};
	uint8_t channelCount = 0;
	ChannelCommand commands[MAX_CHANNEL_COMMAND_COUNT];
	CommandOutcome outcomes[MAX_CHANNEL_COMMAND_COUNT];
	ReolinkConnectionScope connectionScope;

	// Refuse to run if the feature was not configured
	if (!hasReolinkCredentials()) {
		printLogln("Reolink disable skipped because credentials are missing.");
		showReolinkStatus("NVR Needed", "Add credentials!");
		return ReolinkMuteResult::Failed;
	}

	// Without Wi-Fi the NVR can't be reached
	if (!isWiFiConnected()) {
		printLogln("Reolink disable skipped because Wi-Fi is offline.");
		showReolinkStatus("Wi-Fi Offline", "Try again!");
		delay(config::SHOW_DISPLAY_MESSAGE_MS);
		return ReolinkMuteResult::Failed;
	}

	// Show the work right away (the login and the channel list can take a few seconds)
	showReolinkStatus("Quiet Mode ON", "Connecting...");

	// Get the list of currently active channels (the request shows its own error, so keep it on screen)
	if (!getActiveChannels(channels, channelCount)) {
		printLogln("Reolink: failed to retrieve active channels.");
		delay(config::SHOW_DISPLAY_MESSAGE_MS);
		return ReolinkMuteResult::Failed;
	}

	// Build one command per setting, and stop if no camera is online
	const uint8_t commandCount = buildMuteCommands(channels, channelCount, commands);
	if (commandCount == 0) {
		printLogln("Reolink: no active channels to mute.");
		showReolinkStatus("No Cameras", "Check NVR!");
		delay(config::SHOW_DISPLAY_MESSAGE_MS);
		return ReolinkMuteResult::Failed;
	}

	// Remember the settings before muting them (so a failed request or a reboot can still restore them)
	saveMutedChannelMasks(addCommandsToMasks(getMutedChannelMasks(), commands, commandCount));

	// Send the mute commands
	if (!applyChannelCommands(commands, commandCount, false, outcomes)) {
		return ReolinkMuteResult::Failed;
	}

	// Forget the settings the NVR refused to mute (they were never changed)
	saveMutedChannelMasks(applyMuteOutcomes(getMutedChannelMasks(), commands, outcomes, commandCount));

	// Report a mute where nothing was confirmed
	const uint8_t succeededCount = countCommandOutcomes(outcomes, commandCount, CommandOutcome::Succeeded);
	if (succeededCount == 0) {
		printLogln("Reolink: the NVR did not confirm any mute command.");
		showReolinkStatus("Mute Refused", "Check NVR!");
		delay(config::SHOW_DISPLAY_MESSAGE_MS);
		return ReolinkMuteResult::Failed;
	}

	// Report a mute where only some settings were confirmed
	if (succeededCount < commandCount) {
		printLogfln("Reolink: %u of %u mute commands confirmed.", succeededCount, commandCount);
		showReolinkStatus("Partly Muted!", "Some cams failed");
		delay(config::SHOW_DISPLAY_MESSAGE_MS);
		return ReolinkMuteResult::PartiallyMuted;
	}

	// All OK, notifications and recording are now disabled
	printLogln("Reolink notifications and recording disabled.");
	showReolinkStatus("Cameras Muted!", "Exit safely!");
	delay(config::SHOW_DISPLAY_MESSAGE_MS);
	return ReolinkMuteResult::Muted;
}

// Restore notifications and recording for the muted settings of the online channels
ReolinkRestoreResult restoreReolinkNotificationsAndRecording() {
	uint8_t channels[config::REOLINK_MAX_CHANNELS] = {0};
	uint8_t channelCount = 0;
	ChannelCommand commands[MAX_CHANNEL_COMMAND_COUNT];
	CommandOutcome outcomes[MAX_CHANNEL_COMMAND_COUNT];
	ReolinkConnectionScope connectionScope;

	// Nothing to restore
	if (!hasMutedChannels(getMutedChannelMasks())) return ReolinkRestoreResult::Restored;

	// Refuse to run if the feature was not configured
	if (!hasReolinkCredentials()) {
		printLogln("Reolink restore skipped because credentials are missing.");
		showReolinkStatus("NVR Needed", "Add credentials!");
		return ReolinkRestoreResult::Failed;
	}

	// Without Wi-Fi the NVR can't be reached
	if (!isWiFiConnected()) {
		printLogln("Reolink restore skipped because Wi-Fi is offline.");
		showReolinkStatus("Wi-Fi Offline", "Will retry");
		delay(config::SHOW_DISPLAY_MESSAGE_MS);
		return ReolinkRestoreResult::Failed;
	}

	// Show the work right away (the login and the channel list can take a few seconds)
	showReolinkStatus("Quiet Mode OFF", "Connecting...");

	// Get the list of currently active channels (the request shows its own error, so keep it on screen)
	if (!getActiveChannels(channels, channelCount)) {
		printLogln("Reolink: failed to retrieve active channels.");
		delay(config::SHOW_DISPLAY_MESSAGE_MS);
		return ReolinkRestoreResult::Failed;
	}

	// Restore the muted settings of the online channels
	const uint8_t commandCount = buildRestoreCommands(channels, channelCount, getMutedChannelMasks(), commands);
	if (commandCount > 0) {
		// Send the restore commands
		if (!applyChannelCommands(commands, commandCount, true, outcomes)) {
			return ReolinkRestoreResult::Failed;
		}

		// Forget the settings the NVR confirmed as restored
		saveMutedChannelMasks(applyRestoreOutcomes(getMutedChannelMasks(), commands, outcomes, commandCount));

		// Any setting not confirmed is still muted, so retry later
		if (countCommandOutcomes(outcomes, commandCount, CommandOutcome::Succeeded) < commandCount) {
			printLogln("Reolink: some restore commands were not confirmed, will retry.");
			return ReolinkRestoreResult::Failed;
		}
	}

	// Muted settings of offline channels stay pending for a later retry
	const MutedChannelMasks remainingMasks = getMutedChannelMasks();
	if (hasMutedChannels(remainingMasks)) {
		printLogfln("Reolink: muted channels still offline (push 0x%08lX, recording 0x%08lX), will retry.", static_cast<unsigned long>(remainingMasks.pushChannelMask), static_cast<unsigned long>(remainingMasks.recordingChannelMask));
		return ReolinkRestoreResult::PartiallyRestored;
	}

	// All OK, notifications and recording are now restored
	printLogln("Reolink notifications and recording restored.");
	return ReolinkRestoreResult::Restored;
}

// Check whether some channels are still muted and need to be restored
bool hasPendingReolinkRestore() {
	return hasMutedChannels(getMutedChannelMasks());
}