#include "reolink.h"

#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>

#include "config/config.h"
#include "serial/serial.h"

namespace {
	// Keep the current session token and its expiry time in memory
	String sessionToken;
	uint32_t tokenExpiryTimeMs = 0;

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
		return static_cast<int32_t>(millis() - tokenExpiryTimeMs) >= 0;
	}

	// Send one HTTPS API request to a fully built URL and parse the standard Reolink envelope
	bool sendReolinkRequest(const String &url, const char *command, JsonVariantConst params, JsonDocument &responseDoc, int &responseCode) {
		// Reolink NVRs commonly use self-signed HTTPS certificates
		WiFiClientSecure secureClient;
		secureClient.setInsecure();

		HTTPClient http;

		// Open the HTTP connection to the Reolink API endpoint
		if (!http.begin(secureClient, url)) {
			printLogfln("Reolink: failed to open HTTP client for %s", command);
			responseCode = -1;
			return false;
		}

		// Set the HTTP timeout and content type for the request
		http.setTimeout(config::REOLINK_HTTP_TIMEOUT_MS);
		http.addHeader("Content-Type", "application/json");

		// Build the JSON request body with the command, action, and parameters
		JsonDocument requestDoc;
		JsonArray requestArray = requestDoc.to<JsonArray>();
		JsonObject requestObject = requestArray.add<JsonObject>();
		requestObject["cmd"] = command;
		requestObject["action"] = 0;
		requestObject["param"] = params;

		// The API expects a JSON array even for a single command
		String requestBody;
		serializeJson(requestDoc, requestBody);

		// Send the HTTP POST request and get the response code and body
		const int statusCode = http.POST(requestBody);
		const String responseBody = http.getString();
		http.end();

		// Check for HTTP errors
		if (statusCode <= 0) {
			printLogfln("Reolink: HTTP request failed for %s (code %d)", command, statusCode);
			responseCode = statusCode;
			return false;
		}

		// Check for non-successful HTTP status codes
		if (statusCode < 200 || statusCode >= 300) {
			printLogfln("Reolink: HTTP status %d for %s", statusCode, command);
			printLogfln("Reolink response: %s", responseBody.c_str());
			responseCode = statusCode;
			return false;
		}

		// Parse the JSON response body into a document
		const DeserializationError jsonError = deserializeJson(responseDoc, responseBody);

		// Check for JSON parsing errors
		if (jsonError) {
			printLogfln("Reolink: failed to parse JSON for %s: %s", command, jsonError.c_str());
			responseCode = -1;
			return false;
		}

		// Get the first element of the response array
		JsonObject result = responseDoc[0];

		// Check if the result is null or missing
		if (result.isNull()) {
			printLogfln("Reolink: empty API result for %s", command);
			responseCode = -1;
			return false;
		}

		// Extract the result code and response code from the API result
		const int resultCode = result["code"] | -1;
		responseCode = result["error"]["rspCode"] | resultCode;

		// Check if the API result indicates an error
		if (resultCode != 0) {
			const char *detail = result["error"]["detail"] | "unknown error";
			printLogfln("Reolink: %s failed: %s", command, detail);
			return false;
		}

		// If we reach this point, the API call was successful, and we can return true
		responseCode = 0;
		return true;
	}

	// Log in once and cache the returned token for later API calls
	bool loginReolink() {
		// Create a JSON document with the login parameters
		JsonDocument paramsDoc;
		JsonObject userObject = paramsDoc["User"].to<JsonObject>();
		userObject["userName"] = config::REOLINK_USERNAME;
		userObject["password"] = config::REOLINK_PASSWORD;

		// Prepare a JSON document to hold the response and a variable for the response code
		JsonDocument responseDoc;
		int responseCode = 0;
		const String loginUrl = buildApiUrl("Login", "token", "null");

		// Send the login request and check for errors
		if (!sendReolinkRequest(loginUrl, "Login", paramsDoc.as<JsonVariantConst>(), responseDoc, responseCode)) {
			sessionToken = "";
			tokenExpiryTimeMs = 0;
			return false;
		}

		// Extract the token name and lease time from the response, using default values if missing
		const char *tokenName = responseDoc[0]["value"]["Token"]["name"] | "";
		const uint32_t leaseTimeSeconds = responseDoc[0]["value"]["Token"]["leaseTime"] | 3600;

		// If the token name is empty, treat it as a failed login and clear the session token
		if (tokenName[0] == '\0') {
			printLogln("Reolink: login succeeded but returned no token.");
			sessionToken = "";
			tokenExpiryTimeMs = 0;
			return false;
		}

		// Cache the session token and calculate its expiry time, subtracting 60 seconds to refresh before actual expiration
		sessionToken = tokenName;
		tokenExpiryTimeMs = millis() + ((leaseTimeSeconds > 60 ? leaseTimeSeconds - 60 : leaseTimeSeconds) * 1000UL);
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

	// Send a token-authenticated Reolink API call and retry once on auth failure
	bool callReolinkApi(const char *command, JsonVariantConst params, JsonDocument &responseDoc) {
		// Ensure a valid session token is available before making the API call
		if (!ensureReolinkToken()) {
			printLogln("Reolink: unable to acquire a session token.");
			return false;
		}

		// Create the API URL
		int responseCode = 0;
		String url = buildApiUrl(command, "token", sessionToken.c_str());

		// Send the API request and check for errors
		if (sendReolinkRequest(url, command, params, responseDoc, responseCode)) {
			return true;
		}

		// If the response code indicates an expired token, attempt to log in again and retry the API call
		if (responseCode != -6) {
			return false;
		}

		// Log that the session has expired and attempt to log in again
		printLogln("Reolink: session expired, logging in again.");

		// Attempt to log in again to acquire a new session token
		if (!loginReolink()) {
			return false;
		}

		// Retry the API call with the new session token
		url = buildApiUrl(command, "token", sessionToken.c_str());
		return sendReolinkRequest(url, command, params, responseDoc, responseCode);
	}

	// Query the NVR and keep only currently online channels
	bool getActiveChannels(uint8_t *channels, uint8_t &channelCount) {
		// Create a JSON document for the request parameters
		JsonDocument paramsDoc;
		paramsDoc.to<JsonObject>();

		// Create a JSON document for the response
		JsonDocument responseDoc;

		// Call the Reolink API to get the channel status and check for errors
		if (!callReolinkApi("GetChannelstatus", paramsDoc.as<JsonVariantConst>(), responseDoc)) {
			return false;
		}

		// Get the array of channel status objects
		channelCount = 0;
		JsonArray statusArray = responseDoc[0]["value"]["status"].as<JsonArray>();

		// Iterate through the status array and keep only online channels, up to the maximum allowed
		for (JsonObject statusEntry : statusArray) {
			if ((statusEntry["online"] | 0) == 1 && channelCount < config::REOLINK_MAX_CHANNELS) {
				channels[channelCount] = static_cast<uint8_t>(statusEntry["channel"] | channelCount);
				channelCount += 1;
			}
		}

		// Return true if at least one active channel was found
		return channelCount > 0;
	}

	// Send a minimal push notification payload for one channel
	bool setChannelPushEnabled(uint8_t channel, bool enabled) {
		// Create a JSON document for the request parameters
		JsonDocument paramsDoc;
		JsonObject pushObject = paramsDoc["Push"].to<JsonObject>();
		pushObject["channel"] = channel;
		pushObject["enable"] = enabled ? 1 : 0;
		pushObject["scheduleEnable"] = enabled ? 1 : 0;

		// Create a JSON document for the response
		JsonDocument responseDoc;

		// Call the Reolink API to set the push notification state and return the result
		return callReolinkApi("SetPushV20", paramsDoc.as<JsonVariantConst>(), responseDoc);
	}

	// Send a minimal recording payload for one channel
	bool setChannelRecordingEnabled(uint8_t channel, bool enabled) {
		// Create a JSON document for the request parameters
		JsonDocument paramsDoc;
		JsonObject recObject = paramsDoc["Rec"].to<JsonObject>();
		recObject["channel"] = channel;
		recObject["enable"] = enabled ? 1 : 0;
		recObject["scheduleEnable"] = enabled ? 1 : 0;

		// Create a JSON document for the response
		JsonDocument responseDoc;

		// Call the Reolink API to set the recording state and return the result
		return callReolinkApi("SetRecV20", paramsDoc.as<JsonVariantConst>(), responseDoc);
	}

	// Apply the same on or off state to both notifications and recording for one channel
	bool setChannelMuteState(uint8_t channel, bool enabled) {
		// Log the intended state change for the channel
		printLogfln("Reolink: setting channel %u push notifications %s...", channel, enabled ? "on" : "off");

		// Set the push notification state for the channel
		if (!setChannelPushEnabled(channel, enabled)) {
			return false;
		}

		// Log successful notification state change before attempting to set the recording state
		printLogfln("Reolink: channel %u push notifications %s!", channel, enabled ? "enabled" : "disabled");

		// Log the intended recording state change for the channel
		printLogfln("Reolink: setting channel %u recording %s...", channel, enabled ? "on" : "off");

		// Set the recording state for the channel
		if (!setChannelRecordingEnabled(channel, enabled)) {
			return false;
		}

		// Log successful recording state change
		printLogfln("Reolink: channel %u recording %s!", channel, enabled ? "enabled" : "disabled");

		// All OK, the channel state has been updated successfully
		return true;
	}
}

// Setup the Reolink integration and check for missing credentials
ReolinkSetupResult setupReolink() {
	// Check if the required Reolink credentials are configured
	if (!hasReolinkCredentials()) {
		printLogln("Reolink credentials are not configured.");
		return ReolinkSetupResult::MissingCredentials;
	}

	// Clear any existing session token and expiry time to ensure a fresh login
	sessionToken = "";
	tokenExpiryTimeMs = 0;

	// All OK, return Ready if the credentials are present
	printLogln("Reolink configured.");
	return ReolinkSetupResult::Ready;
}

// Disable notifications and recording for all active channels
bool disableReolinkNotificationsAndRecording() {
	// Refuse to run if the feature was not configured
	if (!hasReolinkCredentials()) {
		printLogln("Reolink disable skipped because credentials are missing.");
		return false;
	}

	uint8_t channels[config::REOLINK_MAX_CHANNELS] = {0};
	uint8_t channelCount = 0;

	// Get the list of currently active channels from the NVR
	if (!getActiveChannels(channels, channelCount)) {
		printLogln("Reolink: failed to retrieve active channels.");
		return false;
	}

	// Force notifications and recording off for every active channel
	for (uint8_t index = 0; index < channelCount; index += 1) {
		if (!setChannelMuteState(channels[index], false)) {
			printLogfln("Reolink: failed to disable channel %u", channels[index]);
			return false;
		}
	}

	// All OK, notifications and recording are now disabled
	printLogln("Reolink notifications and recording disabled.");
	return true;
}

// Restore notifications and recording for all active channels
bool restoreReolinkNotificationsAndRecording() {
	// Refuse to run if the feature was not configured
	if (!hasReolinkCredentials()) {
		printLogln("Reolink restore skipped because credentials are missing.");
		return false;
	}

	uint8_t channels[config::REOLINK_MAX_CHANNELS] = {0};
	uint8_t channelCount = 0;

	// Get the list of currently active channels from the NVR
	if (!getActiveChannels(channels, channelCount)) {
		printLogln("Reolink: failed to retrieve active channels for restore.");
		return false;
	}

	// Force notifications and recording back on for every active channel
	for (uint8_t index = 0; index < channelCount; index += 1) {
		if (!setChannelMuteState(channels[index], true)) {
			printLogfln("Reolink: failed to restore channel %u", channels[index]);
			return false;
		}
	}

	// All OK, notifications and recording are now restored
	printLogln("Reolink notifications and recording restored.");
	return true;
}