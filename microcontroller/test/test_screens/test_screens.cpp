#include <Arduino.h>
#include <string.h>
#include <unity.h>

#include "config/config.h"
#include "display/screens.h"

namespace {
	// How long a temporary screen stays, like on the device
	constexpr uint32_t SHOWN_MS = 1000;

	// Temporary screen under test, and the state of the switch the normal screen is built from
	TemporaryScreenState state;
	SwitchScreenInputs inputs;

	// Build the lines for the current state
	ScreenLines build() {
		return buildScreenLines(state, inputs);
	}

	// Check both lines of the screen
	void assertScreen(const char *title, const char *detail) {
		const ScreenLines lines = build();
		TEST_ASSERT_EQUAL_STRING(title, lines.title);
		TEST_ASSERT_EQUAL_STRING(detail, lines.detail);
	}
}

// Start each test idle, set up, with Wi-Fi and nothing on top of the normal screen
void setUp() {
	clearTemporaryScreen(state);
	inputs = { false, 0, 0, false, false, true, "192.168.1.42", "Reolink Setup" };
}

// Nothing to clean up after each test
void tearDown() {}

/******************************** Normal screen ********************************/

// Idle and set up, the switch invites a tap
void test_shows_the_welcome_screen_when_idle() {
	assertScreen("Reolink Switch", "Tap to start");
}

// A chosen duration waits for the long press
void test_shows_the_selected_duration() {
	inputs.selectedSeconds = 5 * 60;
	assertScreen("Hold to mute", "Mute for 05:00");
}

// The countdown shows the time left
void test_shows_the_countdown_while_the_timer_runs() {
	inputs.isTimerRunning = true;
	inputs.remainingSeconds = 61;
	assertScreen("Alerts OFF", "01:01 left");
}

// Cameras still muted come before the setup steps, but not before a chosen duration
void test_warns_about_alerts_still_off_when_idle() {
	inputs.hasAlertsStillOff = true;
	assertScreen("Alerts Still OFF", "Hold to retry");
	inputs.selectedSeconds = 60;
	assertScreen("Hold to mute", "Mute for 01:00");
}

// The setup steps: the hotspot first, then the NVR at the address of the web app
void test_shows_the_setup_steps() {
	inputs.isNvrConfigured = false;
	inputs.isHotspotActive = true;
	assertScreen("Join Wi-Fi Setup", "Reolink Setup");
	inputs.isHotspotActive = false;
	assertScreen("Set Up NVR: Open", "192.168.1.42");
	inputs.address = nullptr;
	assertScreen("Set Up NVR: Open", "the web app");
}

/******************************** Temporary screens ********************************/

// The factory reset countdown rounds up, and wins over the countdown of the timer
void test_counts_down_the_factory_reset() {
	inputs.isTimerRunning = true;
	showFactoryResetCountdown(state, 9001, 0);
	assertScreen("Reset in 10s", "Release to stop");
	showFactoryResetCountdown(state, 1, 0);
	assertScreen("Reset in 1s", "Release to stop");
}

// The countdown stays as long as the button is held, and releasing it goes back to the normal screen
void test_keeps_the_factory_reset_countdown_until_released() {
	showFactoryResetCountdown(state, 5000, 0);
	TEST_ASSERT_FALSE(expireTemporaryScreen(state, 60000, SHOWN_MS));
	TEST_ASSERT_EQUAL(TemporaryScreen::FactoryResetCountdown, state.screen);
	clearTemporaryScreen(state);
	assertScreen("Reolink Switch", "Tap to start");
}

// A refused reset says why: the timer, or the cameras still muted
void test_says_why_the_factory_reset_was_refused() {
	showFactoryResetRefused(state, 0);
	assertScreen("Can't reset now", "Alerts still off");
	inputs.isTimerRunning = true;
	assertScreen("Can't reset now", "Stop timer first");
}

// The refusal replaces the countdown, so only one temporary screen is ever shown
void test_replaces_the_countdown_with_the_refusal() {
	showFactoryResetCountdown(state, 1000, 0);
	showFactoryResetRefused(state, 10);
	TEST_ASSERT_EQUAL(TemporaryScreen::FactoryResetRefused, state.screen);
	TEST_ASSERT_FALSE(expireTemporaryScreen(state, 10 + SHOWN_MS - 1, SHOWN_MS));
	TEST_ASSERT_TRUE(expireTemporaryScreen(state, 10 + SHOWN_MS, SHOWN_MS));
	TEST_ASSERT_EQUAL(TemporaryScreen::None, state.screen);
}

// The cancel hint shows over the countdown, and only while the timer runs
void test_shows_the_cancel_hint_only_while_the_timer_runs() {
	inputs.isTimerRunning = true;
	inputs.remainingSeconds = 30;
	showCancelHint(state, 0);
	assertScreen("Alerts OFF", "Hold to cancel");
	inputs.isTimerRunning = false;
	assertScreen("Reolink Switch", "Tap to start");
}

// An outcome shows for a moment, with the button ignored, then the normal screen comes back
void test_shows_an_outcome_for_a_moment() {
	showMessage(state, "Time Is Up", "Alerts restored!", 500);
	TEST_ASSERT_TRUE(isMessageShown(state));
	assertScreen("Time Is Up", "Alerts restored!");
	TEST_ASSERT_FALSE(expireTemporaryScreen(state, 500 + SHOWN_MS - 1, SHOWN_MS));
	TEST_ASSERT_TRUE(expireTemporaryScreen(state, 500 + SHOWN_MS, SHOWN_MS));
	TEST_ASSERT_FALSE(isMessageShown(state));
	assertScreen("Reolink Switch", "Tap to start");
	TEST_ASSERT_FALSE(expireTemporaryScreen(state, 500 + 2 * SHOWN_MS, SHOWN_MS));
}

// A new outcome replaces the last one and starts its own moment (a failed mute, then the restore that follows it)
void test_replaces_an_outcome_with_the_next_one() {
	showMessage(state, "Quiet Mode Fail!", "Try again!", 0);
	showMessage(state, "Mute Cancelled", "Alerts restored!", 800);
	assertScreen("Mute Cancelled", "Alerts restored!");
	TEST_ASSERT_FALSE(expireTemporaryScreen(state, SHOWN_MS, SHOWN_MS));
	TEST_ASSERT_TRUE(expireTemporaryScreen(state, 800 + SHOWN_MS, SHOWN_MS));
}

// The moment ends on time across a millis() rollover
void test_expires_across_the_millis_rollover() {
	showMessage(state, "Cam Offline!", "Will retry", 0xFFFFFF00UL);
	TEST_ASSERT_FALSE(expireTemporaryScreen(state, 0xFFFFFF10UL, SHOWN_MS));
	TEST_ASSERT_FALSE(expireTemporaryScreen(state, 0x00000010UL, SHOWN_MS));
	TEST_ASSERT_TRUE(expireTemporaryScreen(state, static_cast<uint32_t>(0xFFFFFF00UL + SHOWN_MS), SHOWN_MS));
}

// Texts longer than the LCD are cut off at its width
void test_cuts_long_texts_at_the_width_of_the_lcd() {
	showMessage(state, "A title far too long for the LCD", nullptr, 0);
	const ScreenLines lines = build();
	TEST_ASSERT_EQUAL_size_t(config::DISPLAY_COLUMNS, strlen(lines.title));
	TEST_ASSERT_EQUAL_STRING("", lines.detail);
	TEST_ASSERT_EQUAL_size_t(config::DISPLAY_COLUMNS + 1, SCREEN_LINE_SIZE);
}

// Run all the screen tests
int main() {
	UNITY_BEGIN();
	RUN_TEST(test_shows_the_welcome_screen_when_idle);
	RUN_TEST(test_shows_the_selected_duration);
	RUN_TEST(test_shows_the_countdown_while_the_timer_runs);
	RUN_TEST(test_warns_about_alerts_still_off_when_idle);
	RUN_TEST(test_shows_the_setup_steps);
	RUN_TEST(test_counts_down_the_factory_reset);
	RUN_TEST(test_keeps_the_factory_reset_countdown_until_released);
	RUN_TEST(test_says_why_the_factory_reset_was_refused);
	RUN_TEST(test_replaces_the_countdown_with_the_refusal);
	RUN_TEST(test_shows_the_cancel_hint_only_while_the_timer_runs);
	RUN_TEST(test_shows_an_outcome_for_a_moment);
	RUN_TEST(test_replaces_an_outcome_with_the_next_one);
	RUN_TEST(test_expires_across_the_millis_rollover);
	RUN_TEST(test_cuts_long_texts_at_the_width_of_the_lcd);
	return UNITY_END();
}
