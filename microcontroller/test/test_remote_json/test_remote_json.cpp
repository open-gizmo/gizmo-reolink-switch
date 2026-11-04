#include <Arduino.h>
#include <ArduinoJson.h>
#include <unity.h>

#include "config/config.h"
#include "remote/remote_json.h"

namespace {
	// Command under test, and the JSON the web app sends
	RemoteCommand command;
	JsonDocument doc;

	// Parse a JSON body into the command, returning the invalid field (nullptr when it's valid)
	const char *parseJson(const char *body) {
		doc.clear();
		deserializeJson(doc, body);
		return parseQuietModeCommand(doc.as<JsonObjectConst>(), command);
	}
}

// Start each test with no command
void setUp() {
	command = { RemoteCommandType::None, 0 };
}

// Nothing to clean up after each test
void tearDown() {}

/******************************** Commands ********************************/

// Start takes whole minutes, up to the maximum of the button
void test_start_takes_minutes() {
	TEST_ASSERT_NULL(parseJson("{\"action\":\"start\",\"minutes\":15}"));
	TEST_ASSERT_EQUAL(RemoteCommandType::StartQuietMode, command.type);
	TEST_ASSERT_EQUAL_UINT32(15 * 60, command.durationSeconds);
	TEST_ASSERT_NULL(parseJson("{\"action\":\"start\",\"minutes\":60}"));
}

// Start refuses no duration, zero, too long or a fraction of a minute
void test_start_refuses_bad_durations() {
	TEST_ASSERT_EQUAL_STRING("minutes", parseJson("{\"action\":\"start\"}"));
	TEST_ASSERT_EQUAL_STRING("minutes", parseJson("{\"action\":\"start\",\"minutes\":0}"));
	TEST_ASSERT_EQUAL_STRING("minutes", parseJson("{\"action\":\"start\",\"minutes\":61}"));
	TEST_ASSERT_EQUAL_STRING("minutes", parseJson("{\"action\":\"start\",\"minutes\":2.5}"));
	TEST_ASSERT_EQUAL(RemoteCommandType::None, command.type);
}

// Cancel and retry need no other field
void test_cancel_and_retry() {
	TEST_ASSERT_NULL(parseJson("{\"action\":\"cancel\"}"));
	TEST_ASSERT_EQUAL(RemoteCommandType::CancelQuietMode, command.type);
	TEST_ASSERT_NULL(parseJson("{\"action\":\"retry\"}"));
	TEST_ASSERT_EQUAL(RemoteCommandType::RetryRestore, command.type);
}

// An unknown or missing action is refused
void test_unknown_action_is_refused() {
	TEST_ASSERT_EQUAL_STRING("action", parseJson("{\"action\":\"mute\"}"));
	TEST_ASSERT_EQUAL_STRING("action", parseJson("{\"minutes\":5}"));
}

/******************************** Status ********************************/

// The status is written with the names the web app reads
void test_status_is_written_to_json() {
	JsonDocument output;
	writeSwitchStatusToJson(output.to<JsonObject>(), { true, 125, 0, false, false, NvrStatus::Connected, 4 });
	TEST_ASSERT_TRUE(output["timerRunning"].as<bool>());
	TEST_ASSERT_EQUAL_INT(125, output["remainingSeconds"].as<int>());
	TEST_ASSERT_EQUAL_INT(config::TIMER_MAX_DURATION_SECONDS / 60, output["maxMinutes"].as<int>());
	TEST_ASSERT_FALSE(output["alertsStillOff"].as<bool>());
	TEST_ASSERT_EQUAL_STRING("connected", output["nvrStatus"].as<const char *>());
	TEST_ASSERT_EQUAL_INT(4, output["cameraCount"].as<int>());
}

// Every NVR status has its own web API name
void test_nvr_status_names() {
	TEST_ASSERT_EQUAL_STRING("not_configured", getNvrStatusName(NvrStatus::NotConfigured));
	TEST_ASSERT_EQUAL_STRING("unknown", getNvrStatusName(NvrStatus::Unknown));
	TEST_ASSERT_EQUAL_STRING("unreachable", getNvrStatusName(NvrStatus::Unreachable));
	TEST_ASSERT_EQUAL_STRING("login_refused", getNvrStatusName(NvrStatus::LoginRefused));
	TEST_ASSERT_EQUAL_STRING("login_paused", getNvrStatusName(NvrStatus::LoginPaused));
}

// Run all the web app command and status tests
int main() {
	UNITY_BEGIN();
	RUN_TEST(test_start_takes_minutes);
	RUN_TEST(test_start_refuses_bad_durations);
	RUN_TEST(test_cancel_and_retry);
	RUN_TEST(test_unknown_action_is_refused);
	RUN_TEST(test_status_is_written_to_json);
	RUN_TEST(test_nvr_status_names);
	return UNITY_END();
}