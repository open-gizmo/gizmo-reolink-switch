#include "reolink.h"

#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>

#include "config/config.h"
#include "display/display.h"
#include "serial/serial.h"

namespace {
	// Keep the current session token and its expiry time in memory
	String sessionToken;
	uint32_t tokenExpiryTimeMs = 0;

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
		return static_cast<int32_t>(millis() - tokenExpiryTimeMs) >= 0;
	}

	// Append one API command to a request array
	void appendApiRequest(JsonArray requestArray, const char *command, JsonVariantConst params) {
		JsonObject requestObject = requestArray.add<JsonObject>();
		requestObject["cmd"] = command;
		requestObject["action"] = 0;
		requestObject["param"] = params;
	}

	// Send one HTTPS API request with a prebuilt JSON array body and parse the standard Reolink envelope
	bool sendReolinkRequest(const String &url, const char *command, JsonDocument &requestDoc, JsonDocument &responseDoc, int &responseCode) {
		// Reolink NVRs commonly use self-signed HTTPS certificates
		WiFiClientSecure secureClient;
		secureClient.setInsecure();
		HTTPClient http;

		// Open the HTTP connection to the Reolink API endpoint
		if (!http.begin(secureClient, url)) {
			printLogfln("Reolink: failed to open HTTP client for %s", command);
			showReolinkStatus("NVR Link Error", "Check network!");
			responseCode = -1;
			return false;
		}

		// Set the HTTP timeout and content type for the request
		http.setTimeout(config::REOLINK_HTTP_TIMEOUT_MS);
		http.addHeader("Content-Type", "application/json");

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
			showReolinkStatus("NVR Link Error", "Check network!");
			responseCode = statusCode;
			return false;
		}

		// Check for non-successful HTTP status codes
		if (statusCode < 200 || statusCode >= 300) {
			printLogfln("Reolink: HTTP status %d for %s", statusCode, command);
			printLogfln("Reolink response: %s", responseBody.c_str());
			showReolinkStatus("NVR Busy", "Try again!");
			responseCode = statusCode;
			return false;
		}

		// Parse the JSON response body into a document
		const DeserializationError jsonError = deserializeJson(responseDoc, responseBody);

		// Check for JSON parsing errors
		if (jsonError) {
			printLogfln("Reolink: failed to parse JSON for %s: %s", command, jsonError.c_str());
			showReolinkStatus("NVR Reply Err!", "Try again!");
			responseCode = -1;
			return false;
		}

		// Get the results array
		JsonArray results = responseDoc.as<JsonArray>();

		// Check if the results array is empty or null, which indicates an error
		if (results.isNull() || results.size() == 0) {
			printLogfln("Reolink: empty API result for %s", command);
			showReolinkStatus("NVR Reply Err!", "Empty answer");
			responseCode = -1;
			return false;
		}

		// Iterate through the results and check for errors in each command response
		for (JsonObject result : results) {
			const int resultCode = result["code"] | -1;
			responseCode = result["error"]["rspCode"] | resultCode;

			// If the result code is not zero, log the failed command and its error detail
			if (resultCode != 0) {
				const char *failedCommand = result["cmd"] | command;
				const char *detail = result["error"]["detail"] | "unknown error";
				printLogfln("Reolink: %s failed: %s", failedCommand, detail);
				showReolinkStatus("NVR Command Err!", "Try again!");
				return false;
			}
		}

		// If we reach this point, the API call was successful, and we can return true
		responseCode = 0;
		return true;
	}

	// Log in once and cache the returned token for later API calls
	bool loginReolink() {
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
		if (!sendReolinkRequest(loginUrl, "Login", requestDoc, responseDoc, responseCode)) {
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
			showReolinkStatus("NVR Login Err!", "Token missing!");
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

	// Send one prepared request document with token auth and retry once on auth failure
	bool executeReolinkRequest(const char *command, JsonDocument &requestDoc, JsonDocument &responseDoc) {
		// Ensure a valid session token is available before making the API call
		if (!ensureReolinkToken()) {
			printLogln("Reolink: unable to acquire a session token.");
			showReolinkStatus("NVR Login Err!", "Check login!");
			return false;
		}

		// Create the API URL
		int responseCode = 0;
		String url = buildApiUrl(command, "token", sessionToken.c_str());

		// Send the API request and check for errors
		if (sendReolinkRequest(url, command, requestDoc, responseDoc, responseCode)) {
			return true;
		}

		// If the response code indicates an expired token, attempt to log in again and retry the API call
		if (responseCode != -6) {
			return false;
		}

		// Log that the session has expired and attempt to log in again
		printLogln("Reolink: session expired, logging in again.");
		showReolinkStatus("NVR Reconnect!", "Please wait!");

		// Attempt to log in again to acquire a new session token
		if (!loginReolink()) {
			return false;
		}

		// Retry the API call with the new session token
		url = buildApiUrl(command, "token", sessionToken.c_str());
		return sendReolinkRequest(url, command, requestDoc, responseDoc, responseCode);
	}

	// Send a token-authenticated Reolink API call and retry once on auth failure
	bool callReolinkApi(const char *command, JsonVariantConst params, JsonDocument &responseDoc) {
		JsonDocument requestDoc;
		JsonArray requestArray = requestDoc.to<JsonArray>();
		appendApiRequest(requestArray, command, params);
		return executeReolinkRequest(command, requestDoc, responseDoc);
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

	// Append one channel's push and recording commands to a shared batch request
	void appendChannelMuteRequests(JsonArray requestArray, uint8_t channel, bool enabled) {
		// Create the JSON parameters for the push notification command for this channel
		JsonDocument pushParamsDoc;
		JsonObject pushParams = pushParamsDoc.to<JsonObject>();
		JsonObject pushObject = pushParams["Push"].to<JsonObject>();
		pushObject["channel"] = channel;
		pushObject["enable"] = enabled ? 1 : 0;
		pushObject["scheduleEnable"] = enabled ? 1 : 0;
		appendApiRequest(requestArray, "SetPushV20", pushParams);

		// Create the JSON parameters for the recording command for this channel
		JsonDocument recParamsDoc;
		JsonObject recParams = recParamsDoc.to<JsonObject>();
		JsonObject recObject = recParams["Rec"].to<JsonObject>();
		recObject["channel"] = channel;
		recObject["enable"] = enabled ? 1 : 0;
		recObject["scheduleEnable"] = enabled ? 1 : 0;
		appendApiRequest(requestArray, "SetRecV20", recParams);
	}

	// Apply the requested notification and recording state to all active channels in a single batch
	bool applyReolinkNotificationsAndRecordingState(bool enabled) {
		uint8_t channels[config::REOLINK_MAX_CHANNELS] = {0};
		uint8_t channelCount = 0;

		// Get the list of currently active channels from the NVR
		if (!getActiveChannels(channels, channelCount)) {
			printLogln("Reolink: failed to retrieve active channels.");
			showReolinkStatus("No Cameras", "Check NVR!");
			delay(config::SHOW_DISPLAY_MESSAGE_MS);
			return false;
		}

		// Get the response code from the API
		JsonDocument requestDoc;
		JsonArray requestArray = requestDoc.to<JsonArray>();

		// Append the mute requests for each active channel to the batch request
		for (uint8_t index = 0; index < channelCount; index += 1) {
			appendChannelMuteRequests(requestArray, channels[index], enabled);
		}

		// Log the batch request and send it to the Reolink API
		printLogfln("Reolink: sending batch request to turn %s notifications and recording for %u active channels...", enabled ? "on" : "off", channelCount);
		showReolinkStatus(enabled ? "Quiet Mode OFF" : "Quiet Mode ON", enabled ? "Restoring alerts" : "Muting cameras");

		// Create a JSON document for the response
		JsonDocument responseDoc;

		// Send the batch request to the Reolink API and check for errors
		if (!executeReolinkRequest("SetPushV20", requestDoc, responseDoc)) {
			printLogfln("Reolink: failed to %s notifications and recording.", enabled ? "restore" : "disable");
			showReolinkStatus("Camera Sync Fail", "Try again!");
			delay(config::SHOW_DISPLAY_MESSAGE_MS);
			return false;
		}

		// Log the successful batch request and return true
		printLogfln("Reolink: batch request sent successfully. Notifications and recording are now %s for %u active channels.", enabled ? "enabled" : "disabled", channelCount);
		delay(config::SHOW_DISPLAY_MESSAGE_MS);
		showReolinkStatus(enabled ? "Alerts Restored!" : "Cameras Muted!", enabled ? "Welcome back" : "Exit safely!");
		delay(config::SHOW_DISPLAY_MESSAGE_MS);

		// All OK, return true
		return true;
	}
}

// Setup the Reolink integration and check for missing credentials
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

	// All OK, return Ready if the credentials are present
	printLogln("Reolink configured.");
	showReolinkStatus("NVR Linked", "Ready to mute!");
	return ReolinkSetupResult::Ready;
}

// Disable notifications and recording for all active channels
bool disableReolinkNotificationsAndRecording() {
	// Refuse to run if the feature was not configured
	if (!hasReolinkCredentials()) {
		printLogln("Reolink disable skipped because credentials are missing.");
		showReolinkStatus("NVR Needed", "Add credentials!");
		return false;
	}

	// Attempt to apply the disabled state to all active channels and check for errors
	if (!applyReolinkNotificationsAndRecordingState(false)) {
		return false;
	}

	// All OK, notifications and recording are now disabled
	printLogln("Reolink notifications and recording disabled.");
	showReolinkStatus("Cameras Muted!", "Exit safely!");
	delay(config::SHOW_DISPLAY_MESSAGE_MS);
	return true;
}

// Restore notifications and recording for all active channels
bool restoreReolinkNotificationsAndRecording() {
	// Refuse to run if the feature was not configured
	if (!hasReolinkCredentials()) {
		printLogln("Reolink restore skipped because credentials are missing.");
		showReolinkStatus("NVR Needed", "Add credentials!");
		return false;
	}

	// Attempt to apply the enabled state to all active channels and check for errors
	if (!applyReolinkNotificationsAndRecordingState(true)) {
		return false;
	}

	// All OK, notifications and recording are now restored
	printLogln("Reolink notifications and recording restored.");
	showReolinkStatus("Alerts Restored!", "Welcome back");
	delay(config::SHOW_DISPLAY_MESSAGE_MS);
	return true;
}