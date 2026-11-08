#include "nvr_settings_json.h"

#include <ctype.h>
#include <string.h>
#include <gizmo/text.h>

namespace {
	// Check whether a character can be part of an NVR address (a name, an IPv4 or bracketed IPv6 address, and a port)
	bool isHostCharacter(char character) {
		return isalnum((unsigned char)character) || character == '.' || character == '-' || character == ':' || character == '[' || character == ']';
	}

	// Copy an NVR address without the scheme and the trailing slashes people paste from the browser (false if it's empty, too long or has other characters)
	bool readHost(const char *value, char *output, size_t outputSize) {
		// Skip the scheme
		if (strncmp(value, "https://", 8) == 0) {
			value += 8;
		} else if (strncmp(value, "http://", 7) == 0) {
			value += 7;
		}

		// Drop the trailing slashes
		size_t length = strlen(value);
		while (length > 0 && value[length - 1] == '/') {
			length -= 1;
		}

		// Check the length and the characters
		if (length == 0 || length >= outputSize) {
			return false;
		}
		for (size_t i = 0; i < length; i++) {
			if (!isHostCharacter(value[i])) {
				return false;
			}
		}

		// Copy the address (its first length characters, without the slashes)
		gizmo::copyString(output, value, length + 1);
		return true;
	}

	// Copy a required text that fits its buffer (false if it's missing, empty, too long or not a text)
	bool readRequiredText(JsonVariantConst value, char *output, size_t outputSize) {
		if (!value.is<const char *>()) {
			return false;
		}
		const char *text = value.as<const char *>();
		const size_t length = strlen(text);
		if (length == 0 || length >= outputSize) {
			return false;
		}
		gizmo::copyString(output, text, outputSize);
		return true;
	}
}

// Write the login into a JSON object (the password only as whether one is set)
void writeNvrSettingsToJson(JsonObject json, const NvrSettings &settings) {
	json["host"] = settings.host;
	json["username"] = settings.username;
	json["hasPassword"] = settings.password[0] != '\0';
	json["configured"] = isNvrConfigured(settings);
}

// Apply a login from the web app, all or nothing (an empty or missing password keeps the saved one, but only for the same address and username: the saved password is never sent anywhere else; returns the name of the first invalid field, or nullptr)
const char *applyNvrSettingsFromJson(JsonObjectConst json, NvrSettings &settings) {
	NvrSettings updated = settings;

	// Read the address
	JsonVariantConst host = json["host"];
	if (!host.is<const char *>() || !readHost(host.as<const char *>(), updated.host, sizeof(updated.host))) {
		return "host";
	}

	// Read the username
	if (!readRequiredText(json["username"], updated.username, sizeof(updated.username))) {
		return "username";
	}

	// A new address or username forgets the saved password, so the switch never logs in somewhere else with it (the web app asks before, and the password must be typed again)
	if (strcmp(updated.host, settings.host) != 0 || strcmp(updated.username, settings.username) != 0) {
		updated.password[0] = '\0';
	}

	// Read the password, unless it's left empty to keep the saved one
	JsonVariantConst password = json["password"];
	if (!password.isNull()) {
		if (!password.is<const char *>() || strlen(password.as<const char *>()) >= sizeof(updated.password)) {
			return "password";
		}
		if (password.as<const char *>()[0] != '\0') {
			gizmo::copyString(updated.password, password.as<const char *>(), sizeof(updated.password));
		}
	}

	// A login needs a password, saved or new
	if (updated.password[0] == '\0') {
		return "password";
	}

	// Every field is valid: apply them all
	settings = updated;
	return nullptr;
}