#include <Arduino.h>
#include <ArduinoJson.h>
#include <unity.h>

#include "config/config.h"
#include "remote/remote_json.h"

// Nothing to set up or clean up
void setUp() {}
void tearDown() {}

/******************************** Status ********************************/

// The status is written with the names the web app reads
void test_status_is_written_to_json() {
	JsonDocument output;
	writeSwitchStatusToJson(output.to<JsonObject>(), { true, 125, 0, false, false, NvrStatus::Connected, 4 });
	TEST_ASSERT_TRUE(output["timerRunning"].as<bool>());
	TEST_ASSERT_EQUAL_INT(125, output["remainingSeconds"].as<int>());
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

// Run all the status tests
int main() {
	UNITY_BEGIN();
	RUN_TEST(test_status_is_written_to_json);
	RUN_TEST(test_nvr_status_names);
	return UNITY_END();
}