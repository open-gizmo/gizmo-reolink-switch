#include <Arduino.h>
#include <ArduinoJson.h>
#include <string.h>
#include <unity.h>

#include "nvr/nvr_settings_json.h"

namespace {
	// Login under test, and the JSON the web app sends
	NvrSettings settings;
	JsonDocument doc;

	// Apply a JSON body to the login, returning the invalid field (nullptr when all are valid)
	const char *applyJson(const char *body) {
		doc.clear();
		deserializeJson(doc, body);
		return applyNvrSettingsFromJson(doc.as<JsonObjectConst>(), settings);
	}
}

// Start each test from a saved login
void setUp() {
	settings = {};
	strcpy(settings.host, "192.168.1.10");
	strcpy(settings.username, "admin");
	strcpy(settings.password, "secret");
}

// Nothing to clean up after each test
void tearDown() {}

/******************************** Valid logins ********************************/

// A complete login replaces the saved one
void test_complete_login_is_saved() {
	TEST_ASSERT_NULL(applyJson("{\"host\":\"nvr.local:8443\",\"username\":\"guard\",\"password\":\"newpass\"}"));
	TEST_ASSERT_EQUAL_STRING("nvr.local:8443", settings.host);
	TEST_ASSERT_EQUAL_STRING("guard", settings.username);
	TEST_ASSERT_EQUAL_STRING("newpass", settings.password);
}

// An empty or missing password keeps the saved one (the web app never gets it back)
void test_empty_password_keeps_the_saved_one() {
	TEST_ASSERT_NULL(applyJson("{\"host\":\"192.168.1.20\",\"username\":\"admin\",\"password\":\"\"}"));
	TEST_ASSERT_EQUAL_STRING("secret", settings.password);
	TEST_ASSERT_NULL(applyJson("{\"host\":\"192.168.1.20\",\"username\":\"admin\"}"));
	TEST_ASSERT_EQUAL_STRING("secret", settings.password);
}

// An address pasted from the browser loses its scheme and trailing slash
void test_pasted_address_is_cleaned() {
	TEST_ASSERT_NULL(applyJson("{\"host\":\"https://192.168.1.30/\",\"username\":\"admin\"}"));
	TEST_ASSERT_EQUAL_STRING("192.168.1.30", settings.host);
	TEST_ASSERT_NULL(applyJson("{\"host\":\"http://[fe80::1]:443//\",\"username\":\"admin\"}"));
	TEST_ASSERT_EQUAL_STRING("[fe80::1]:443", settings.host);
}

/******************************** Invalid logins ********************************/

// A missing, empty or odd address is refused
void test_invalid_address_is_refused() {
	TEST_ASSERT_EQUAL_STRING("host", applyJson("{\"username\":\"admin\"}"));
	TEST_ASSERT_EQUAL_STRING("host", applyJson("{\"host\":\"https://\",\"username\":\"admin\"}"));
	TEST_ASSERT_EQUAL_STRING("host", applyJson("{\"host\":\"192.168.1.10/api.cgi\",\"username\":\"admin\"}"));
	TEST_ASSERT_EQUAL_STRING("host", applyJson("{\"host\":\"nvr local\",\"username\":\"admin\"}"));
}

// A missing or too long username is refused
void test_invalid_username_is_refused() {
	TEST_ASSERT_EQUAL_STRING("username", applyJson("{\"host\":\"192.168.1.10\",\"username\":\"\"}"));
	TEST_ASSERT_EQUAL_STRING("username", applyJson("{\"host\":\"192.168.1.10\",\"username\":\"abcdefghijabcdefghijabcdefghijabc\"}"));
}

// A first login needs a password
void test_first_login_needs_a_password() {
	settings.password[0] = '\0';
	TEST_ASSERT_EQUAL_STRING("password", applyJson("{\"host\":\"192.168.1.10\",\"username\":\"admin\",\"password\":\"\"}"));
}

// One invalid field leaves the saved login as it was
void test_invalid_login_changes_nothing() {
	TEST_ASSERT_EQUAL_STRING("username", applyJson("{\"host\":\"10.0.0.1\",\"username\":42,\"password\":\"other\"}"));
	TEST_ASSERT_EQUAL_STRING("192.168.1.10", settings.host);
	TEST_ASSERT_EQUAL_STRING("secret", settings.password);
}

/******************************** Output ********************************/

// The password is never written, only whether there is one
void test_password_is_never_written() {
	JsonDocument output;
	writeNvrSettingsToJson(output.to<JsonObject>(), settings);
	TEST_ASSERT_EQUAL_STRING("192.168.1.10", output["host"].as<const char *>());
	TEST_ASSERT_TRUE(output["hasPassword"].as<bool>());
	TEST_ASSERT_TRUE(output["configured"].as<bool>());
	TEST_ASSERT_FALSE(output["password"].is<const char *>());
}

// Run all the NVR login tests
int main() {
	UNITY_BEGIN();
	RUN_TEST(test_complete_login_is_saved);
	RUN_TEST(test_empty_password_keeps_the_saved_one);
	RUN_TEST(test_pasted_address_is_cleaned);
	RUN_TEST(test_invalid_address_is_refused);
	RUN_TEST(test_invalid_username_is_refused);
	RUN_TEST(test_first_login_needs_a_password);
	RUN_TEST(test_invalid_login_changes_nothing);
	RUN_TEST(test_password_is_never_written);
	return UNITY_END();
}