#include <Arduino.h>
#include <unity.h>

#include "config/config.h"
#include "menu/menu.h"

namespace {
	// Move the fake clock forward
	void advanceTimeMs(uint32_t durationMs) {
		fakeClock::currentTimeMs += durationMs;
	}

	// Press the short button a number of times
	void pressShortTimes(uint8_t pressCount) {
		for (uint8_t index = 0; index < pressCount; index += 1) {
			increaseSelectedDuration();
		}
	}

	// Select a duration and start the timer
	void startTimerWithPresses(uint8_t pressCount) {
		pressShortTimes(pressCount);
		TEST_ASSERT_EQUAL(MenuEvent::TimerStarted, activateOrCancelTimer());
	}
}

// Reset the menu and the fake clock before each test
void setUp() {
	fakeClock::currentTimeMs = 1000;
	setupMenu();
}

// Nothing to clean up after each test
void tearDown() {}

/******************************** Duration selection ********************************/

// Short presses add 1 minute up to 5 minutes, then 5 minutes at a time
void test_short_presses_follow_the_step_rules() {
	// Expected durations after each press, in minutes
	const uint32_t expectedMinutes[] = { 1, 2, 3, 4, 5, 10, 15, 20 };

	// Press once per expected value and check the selected duration each time
	for (const uint32_t minutes : expectedMinutes) {
		TEST_ASSERT_EQUAL(MenuEvent::SelectionChanged, increaseSelectedDuration());
		TEST_ASSERT_EQUAL_UINT32(minutes * 60, getSelectedDurationSeconds());
	}
}

// The selection stops at the 60 minute maximum, even if a step would pass it
void test_selection_stops_at_the_maximum() {
	// Press until the maximum: 5 presses of 1 minute, then 11 presses of 5 minutes
	pressShortTimes(16);

	// Expect exactly the maximum
	TEST_ASSERT_EQUAL_UINT32(config::TIMER_MAX_DURATION_SECONDS, getSelectedDurationSeconds());
	TEST_ASSERT_EQUAL_STRING("Selected duration: 60:00", getMenuDisplayText());
}

// One more press after the maximum wraps back to zero (so an overshoot can be fixed without muting)
void test_selection_wraps_to_zero_after_the_maximum() {
	// Press until the maximum, then once more
	pressShortTimes(16);
	TEST_ASSERT_EQUAL(MenuEvent::SelectionChanged, increaseSelectedDuration());

	// Expect the selection to be back at zero
	TEST_ASSERT_EQUAL_UINT32(0, getSelectedDurationSeconds());

	// Expect the next press to start the cycle again from 1 minute
	increaseSelectedDuration();
	TEST_ASSERT_EQUAL_UINT32(60, getSelectedDurationSeconds());
}

// Wrapping to zero leaves nothing to start
void test_long_press_after_wrap_does_nothing() {
	// Wrap the selection back to zero
	pressShortTimes(17);

	// Expect a long press not to start the timer
	TEST_ASSERT_EQUAL(MenuEvent::None, activateOrCancelTimer());
	TEST_ASSERT_FALSE(isTimerRunning());
}

// The status text shows the selected duration as M:SS
void test_selection_updates_the_display_text() {
	// Select 5 minutes
	pressShortTimes(5);

	// Expect the formatted selection text
	TEST_ASSERT_EQUAL_STRING("Selected duration: 5:00", getMenuDisplayText());
}

// A long press with no duration selected must not start anything
void test_long_press_without_duration_does_nothing() {
	// Long press right away
	TEST_ASSERT_EQUAL(MenuEvent::None, activateOrCancelTimer());

	// Expect the timer to stay idle
	TEST_ASSERT_FALSE(isTimerRunning());
}

// Short presses during the countdown must not change the running duration
void test_selection_is_ignored_while_running() {
	// Start a 1 minute timer
	startTimerWithPresses(1);

	// Press short while running
	TEST_ASSERT_EQUAL(MenuEvent::None, increaseSelectedDuration());

	// Expect the countdown to keep its original duration
	TEST_ASSERT_EQUAL_UINT32(60, getRemainingSeconds());
}

/******************************** Countdown ********************************/

// Starting the timer shows the full duration as remaining
void test_start_shows_the_full_duration() {
	// Start a 2 minute timer
	startTimerWithPresses(2);

	// Expect the full duration to be remaining
	TEST_ASSERT_TRUE(isTimerRunning());
	TEST_ASSERT_EQUAL_UINT32(120, getRemainingSeconds());
	TEST_ASSERT_EQUAL_STRING("Time remaining: 2:00", getMenuDisplayText());
}

// The countdown ticks once per second, not on every loop iteration
void test_countdown_ticks_once_per_second() {
	// Start a 1 minute timer
	startTimerWithPresses(1);

	// Expect no tick before a full second has passed
	advanceTimeMs(999);
	TEST_ASSERT_EQUAL(MenuEvent::None, updateMenu());

	// Expect one tick when the second is complete
	advanceTimeMs(1);
	TEST_ASSERT_EQUAL(MenuEvent::TimerTick, updateMenu());
	TEST_ASSERT_EQUAL_UINT32(59, getRemainingSeconds());

	// Expect no second tick within the same second
	TEST_ASSERT_EQUAL(MenuEvent::None, updateMenu());
}

// The timer completes exactly when the duration has elapsed, and returns to idle
void test_countdown_completes_and_resets() {
	// Start a 1 minute timer
	startTimerWithPresses(1);

	// Expect the timer to still run one second before the end
	advanceTimeMs(59000);
	TEST_ASSERT_EQUAL(MenuEvent::TimerTick, updateMenu());
	TEST_ASSERT_EQUAL_UINT32(1, getRemainingSeconds());

	// Expect completion at the end, with the selection reset
	advanceTimeMs(1000);
	TEST_ASSERT_EQUAL(MenuEvent::TimerCompleted, updateMenu());
	TEST_ASSERT_FALSE(isTimerRunning());
	TEST_ASSERT_EQUAL_UINT32(0, getSelectedDurationSeconds());
}

// A long press during the countdown cancels it and returns to idle
void test_long_press_cancels_the_countdown() {
	// Start a timer and let some time pass
	startTimerWithPresses(3);
	advanceTimeMs(5000);

	// Cancel with a long press
	TEST_ASSERT_EQUAL(MenuEvent::TimerCancelled, activateOrCancelTimer());

	// Expect the timer to be idle and silent
	TEST_ASSERT_FALSE(isTimerRunning());
	TEST_ASSERT_EQUAL(MenuEvent::None, updateMenu());
}

// The countdown keeps working when millis() rolls over (every ~49.7 days)
void test_countdown_survives_millis_rollover() {
	// Start a 1 minute timer 10 seconds before the clock wraps
	fakeClock::currentTimeMs = UINT32_MAX - 10000;
	startTimerWithPresses(1);

	// Move 30 seconds forward, across the wrap
	advanceTimeMs(30000);

	// Expect the elapsed time to be computed correctly
	TEST_ASSERT_EQUAL(MenuEvent::TimerTick, updateMenu());
	TEST_ASSERT_EQUAL_UINT32(30, getRemainingSeconds());

	// Expect completion after the full minute
	advanceTimeMs(30000);
	TEST_ASSERT_EQUAL(MenuEvent::TimerCompleted, updateMenu());
}

/******************************** Mute flow helpers ********************************/

// Restarting the countdown gives back the time spent muting the cameras
void test_restart_countdown_gives_back_the_full_duration() {
	// Start a 1 minute timer and spend 4 seconds on network calls
	startTimerWithPresses(1);
	advanceTimeMs(4000);

	// Restart the countdown once the cameras are muted
	restartTimerCountdown();

	// Expect the full minute to be available from now
	TEST_ASSERT_EQUAL_UINT32(60, getRemainingSeconds());
	advanceTimeMs(59000);
	TEST_ASSERT_EQUAL(MenuEvent::TimerTick, updateMenu());
	TEST_ASSERT_EQUAL_UINT32(1, getRemainingSeconds());
}

// Restarting the countdown while idle must not start a timer
void test_restart_countdown_is_ignored_when_idle() {
	// Select a duration without starting
	pressShortTimes(1);

	// Restart the countdown while idle
	restartTimerCountdown();

	// Expect the timer to stay idle
	TEST_ASSERT_FALSE(isTimerRunning());
	TEST_ASSERT_EQUAL(MenuEvent::None, updateMenu());
}

// Stopping the timer after a failed mute resets it without any event
void test_stop_timer_resets_without_events() {
	// Start a timer, then stop it as if muting failed
	startTimerWithPresses(2);
	stopTimer();

	// Expect the timer to be idle, the selection cleared, and no later events
	TEST_ASSERT_FALSE(isTimerRunning());
	TEST_ASSERT_EQUAL_UINT32(0, getSelectedDurationSeconds());
	advanceTimeMs(200000);
	TEST_ASSERT_EQUAL(MenuEvent::None, updateMenu());
}

// Run all the menu tests
int main() {
	UNITY_BEGIN();

	// Duration selection
	RUN_TEST(test_short_presses_follow_the_step_rules);
	RUN_TEST(test_selection_stops_at_the_maximum);
	RUN_TEST(test_selection_wraps_to_zero_after_the_maximum);
	RUN_TEST(test_long_press_after_wrap_does_nothing);
	RUN_TEST(test_selection_updates_the_display_text);
	RUN_TEST(test_long_press_without_duration_does_nothing);
	RUN_TEST(test_selection_is_ignored_while_running);

	// Countdown
	RUN_TEST(test_start_shows_the_full_duration);
	RUN_TEST(test_countdown_ticks_once_per_second);
	RUN_TEST(test_countdown_completes_and_resets);
	RUN_TEST(test_long_press_cancels_the_countdown);
	RUN_TEST(test_countdown_survives_millis_rollover);

	// Mute flow helpers
	RUN_TEST(test_restart_countdown_gives_back_the_full_duration);
	RUN_TEST(test_restart_countdown_is_ignored_when_idle);
	RUN_TEST(test_stop_timer_resets_without_events);

	return UNITY_END();
}