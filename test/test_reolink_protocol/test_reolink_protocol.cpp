#include <Arduino.h>
#include <unity.h>

#include "reolink/reolink_protocol.h"

namespace {
	// Parse a reply of the NVR into a document
	JsonDocument parseReply(const char *json) {
		JsonDocument doc;
		TEST_ASSERT_FALSE(deserializeJson(doc, json));
		return doc;
	}
}

// Nothing to prepare before each test
void setUp() {}

// Nothing to clean up after each test
void tearDown() {}

/******************************** Session lease ********************************/

// The token is refreshed a minute before the NVR expires it
void test_token_expiry_keeps_a_minute_of_margin() {
	TEST_ASSERT_EQUAL_UINT32(1000 + (3600 - 60) * 1000UL, computeTokenExpiryMs(1000, 3600));
}

// A lease too short for the margin is used as is
void test_short_lease_is_used_without_margin() {
	TEST_ASSERT_EQUAL_UINT32(1000 + 30 * 1000UL, computeTokenExpiryMs(1000, 30));
}

// A huge lease is capped, so the expiry stays in the range the check can handle
void test_huge_lease_is_capped() {
	const uint32_t expiryMs = computeTokenExpiryMs(1000, 0xFFFFFFFFUL);

	// Expect the capped lease, and a token that is not expired right after the login
	TEST_ASSERT_EQUAL_UINT32(1000 + (MAX_TOKEN_LEASE_SECONDS - 60) * 1000UL, expiryMs);
	TEST_ASSERT_FALSE(isTokenExpired(2000, expiryMs));
}

// The token expires exactly at its expiry time
void test_token_expires_at_its_expiry_time() {
	TEST_ASSERT_FALSE(isTokenExpired(9999, 10000));
	TEST_ASSERT_TRUE(isTokenExpired(10000, 10000));
	TEST_ASSERT_TRUE(isTokenExpired(10001, 10000));
}

// The expiry works across the millis() rollover
void test_token_expiry_works_across_clock_rollover() {
	// Log in right before the clock wraps around
	const uint32_t loginTimeMs = 0xFFFFFFFFUL - 1000;
	const uint32_t expiryMs = computeTokenExpiryMs(loginTimeMs, 3600);

	// Expect a valid token after the wrap, and an expired one once the lease is over
	TEST_ASSERT_FALSE(isTokenExpired(500, expiryMs));
	TEST_ASSERT_TRUE(isTokenExpired(expiryMs + 1, expiryMs));
}

/******************************** Reply check ********************************/

// A reply where every command succeeded is OK
void test_successful_reply_is_ok() {
	const JsonDocument doc = parseReply(R"([{"cmd":"GetChannelstatus","code":0,"value":{}}])");
	TEST_ASSERT_EQUAL(ReplyStatus::Ok, checkReplyResults(doc.as<JsonArrayConst>(), true).status);
}

// An empty or missing result list is an unreadable answer
void test_empty_reply_is_empty() {
	const JsonDocument emptyArray = parseReply("[]");
	const JsonDocument notArray = parseReply(R"({"code":0})");
	TEST_ASSERT_EQUAL(ReplyStatus::Empty, checkReplyResults(emptyArray.as<JsonArrayConst>(), true).status);
	TEST_ASSERT_EQUAL(ReplyStatus::Empty, checkReplyResults(notArray.as<JsonArrayConst>(), true).status);
}

// An expired session is reported with its code, even when each command is read separately
void test_expired_session_fails_the_request() {
	const JsonDocument doc = parseReply(R"([{"cmd":"SetPushV20","code":1,"error":{"rspCode":-6,"detail":"please login first"}}])");

	// Expect the session to be reported as expired in both modes
	const ReplyCheck strictCheck = checkReplyResults(doc.as<JsonArrayConst>(), true);
	TEST_ASSERT_EQUAL(ReplyStatus::SessionExpired, strictCheck.status);
	TEST_ASSERT_EQUAL_INT(SESSION_EXPIRED_CODE, strictCheck.errorCode);
	TEST_ASSERT_EQUAL(ReplyStatus::SessionExpired, checkReplyResults(doc.as<JsonArrayConst>(), false).status);
}

// A refused command fails the request when every command must succeed, with what the NVR said
void test_refused_command_fails_a_strict_request() {
	const JsonDocument doc = parseReply(R"([{"cmd":"Login","code":1,"error":{"rspCode":-7,"detail":"login failed"}}])");
	const ReplyCheck check = checkReplyResults(doc.as<JsonArrayConst>(), true);

	// Expect the refusal, the error code, the command and the detail
	TEST_ASSERT_EQUAL(ReplyStatus::CommandRefused, check.status);
	TEST_ASSERT_EQUAL_INT(-7, check.errorCode);
	TEST_ASSERT_EQUAL_STRING("Login", check.failedCommand);
	TEST_ASSERT_EQUAL_STRING("login failed", check.detail);
}

// A refused command doesn't fail a batch whose commands are read one by one
void test_refused_command_is_ok_in_a_batch() {
	const JsonDocument doc = parseReply(R"([{"cmd":"SetPushV20","code":0},{"cmd":"SetRecV20","code":1,"error":{"rspCode":-9}}])");
	TEST_ASSERT_EQUAL(ReplyStatus::Ok, checkReplyResults(doc.as<JsonArrayConst>(), false).status);
}

// A failed command without error details uses its own code, and has no name or detail
void test_refused_command_without_details() {
	const JsonDocument doc = parseReply(R"([{"code":3}])");
	const ReplyCheck check = checkReplyResults(doc.as<JsonArrayConst>(), true);

	// Expect the result code and no name or detail
	TEST_ASSERT_EQUAL(ReplyStatus::CommandRefused, check.status);
	TEST_ASSERT_EQUAL_INT(3, check.errorCode);
	TEST_ASSERT_NULL(check.failedCommand);
	TEST_ASSERT_NULL(check.detail);
}

// A result without a code counts as failed
void test_result_without_code_is_refused() {
	const JsonDocument doc = parseReply(R"([{"cmd":"GetChannelstatus"}])");
	TEST_ASSERT_EQUAL(ReplyStatus::CommandRefused, checkReplyResults(doc.as<JsonArrayConst>(), true).status);
}

/******************************** Batch outcomes ********************************/

// Each answer gives the outcome of the command at the same position
void test_outcomes_follow_the_answers() {
	const ChannelCommand commands[] = { { 0, ChannelSetting::Push }, { 0, ChannelSetting::Recording }, { 2, ChannelSetting::Push } };
	const JsonDocument doc = parseReply(R"([{"cmd":"SetPushV20","code":0},{"cmd":"SetRecV20","code":1},{"cmd":"SetPushV20","code":0}])");
	CommandOutcome outcomes[3];

	// Read the outcomes
	readCommandOutcomes(doc.as<JsonArrayConst>(), commands, 3, outcomes);

	// Expect success, refusal, success
	TEST_ASSERT_EQUAL(CommandOutcome::Succeeded, outcomes[0]);
	TEST_ASSERT_EQUAL(CommandOutcome::Failed, outcomes[1]);
	TEST_ASSERT_EQUAL(CommandOutcome::Succeeded, outcomes[2]);
}

// An answer for another command leaves the outcome unknown (the setting may or may not have changed)
void test_mismatched_answer_is_unknown() {
	const ChannelCommand commands[] = { { 0, ChannelSetting::Push }, { 0, ChannelSetting::Recording } };
	const JsonDocument doc = parseReply(R"([{"cmd":"SetRecV20","code":0},{"cmd":"SetRecV20","code":0}])");
	CommandOutcome outcomes[2];

	// Read the outcomes
	readCommandOutcomes(doc.as<JsonArrayConst>(), commands, 2, outcomes);

	// Expect the first one unknown, the second one read normally
	TEST_ASSERT_EQUAL(CommandOutcome::Unknown, outcomes[0]);
	TEST_ASSERT_EQUAL(CommandOutcome::Succeeded, outcomes[1]);
}

// Commands the NVR didn't answer stay unknown, and extra answers are ignored
void test_missing_and_extra_answers() {
	const ChannelCommand commands[] = { { 1, ChannelSetting::Push }, { 1, ChannelSetting::Recording } };
	const JsonDocument shortReply = parseReply(R"([{"cmd":"SetPushV20","code":0}])");
	const JsonDocument longReply = parseReply(R"([{"cmd":"SetPushV20","code":0},{"cmd":"SetRecV20","code":0},{"cmd":"SetRecV20","code":1}])");
	CommandOutcome outcomes[2];

	// A short reply leaves the second command unknown
	readCommandOutcomes(shortReply.as<JsonArrayConst>(), commands, 2, outcomes);
	TEST_ASSERT_EQUAL(CommandOutcome::Succeeded, outcomes[0]);
	TEST_ASSERT_EQUAL(CommandOutcome::Unknown, outcomes[1]);

	// A long reply only fills the commands that were sent
	readCommandOutcomes(longReply.as<JsonArrayConst>(), commands, 2, outcomes);
	TEST_ASSERT_EQUAL(CommandOutcome::Succeeded, outcomes[0]);
	TEST_ASSERT_EQUAL(CommandOutcome::Succeeded, outcomes[1]);
}

// An answer without a command name is matched by position
void test_unnamed_answer_is_matched_by_position() {
	const ChannelCommand commands[] = { { 4, ChannelSetting::Recording } };
	const JsonDocument doc = parseReply(R"([{"code":0}])");
	CommandOutcome outcomes[1];

	// Read the outcome
	readCommandOutcomes(doc.as<JsonArrayConst>(), commands, 1, outcomes);

	// Expect the success to be kept
	TEST_ASSERT_EQUAL(CommandOutcome::Succeeded, outcomes[0]);
}

// Run all the Reolink protocol tests
int main() {
	UNITY_BEGIN();
	RUN_TEST(test_token_expiry_keeps_a_minute_of_margin);
	RUN_TEST(test_short_lease_is_used_without_margin);
	RUN_TEST(test_huge_lease_is_capped);
	RUN_TEST(test_token_expires_at_its_expiry_time);
	RUN_TEST(test_token_expiry_works_across_clock_rollover);
	RUN_TEST(test_successful_reply_is_ok);
	RUN_TEST(test_empty_reply_is_empty);
	RUN_TEST(test_expired_session_fails_the_request);
	RUN_TEST(test_refused_command_fails_a_strict_request);
	RUN_TEST(test_refused_command_is_ok_in_a_batch);
	RUN_TEST(test_refused_command_without_details);
	RUN_TEST(test_result_without_code_is_refused);
	RUN_TEST(test_outcomes_follow_the_answers);
	RUN_TEST(test_mismatched_answer_is_unknown);
	RUN_TEST(test_missing_and_extra_answers);
	RUN_TEST(test_unnamed_answer_is_matched_by_position);
	return UNITY_END();
}