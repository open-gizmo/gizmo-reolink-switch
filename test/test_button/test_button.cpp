#include <Arduino.h>
#include <unity.h>

#include "config/config.h"
#include "button/button_debouncer.h"

namespace {
	// Debouncer under test, and the time of the fake clock
	ButtonDebouncer debouncer;
	uint32_t currentTimeMs = 0;

	// Hold the button at one level for a while, polling every millisecond, and return the events seen
	uint8_t holdLevel(bool isPressed, uint32_t durationMs, ButtonEvent *events = nullptr, uint8_t maxEvents = 0) {
		uint8_t eventCount = 0;
		for (uint32_t elapsedMs = 0; elapsedMs < durationMs; elapsedMs += 1) {
			currentTimeMs += 1;
			const ButtonEvent event = updateButtonDebouncer(debouncer, isPressed, currentTimeMs);
			if (event == ButtonEvent::None) continue;
			if (eventCount < maxEvents) events[eventCount] = event;
			eventCount += 1;
		}
		return eventCount;
	}

	// Press for a while then release, and return the only event expected from it
	ButtonEvent pressFor(uint32_t pressDurationMs) {
		ButtonEvent events[2] = { ButtonEvent::None, ButtonEvent::None };
		uint8_t eventCount = holdLevel(true, pressDurationMs, events, 2);
		eventCount += holdLevel(false, config::BUTTON_DEBOUNCE_DELAY_MS * 2, events + eventCount, 2 - eventCount);
		TEST_ASSERT_LESS_OR_EQUAL_UINT8(1, eventCount);
		return events[0];
	}
}

// Start each test with the button released and a clock far from zero
void setUp() {
	currentTimeMs = 5000;
	resetButtonDebouncer(debouncer, false, currentTimeMs);
}

// Nothing to clean up after each test
void tearDown() {}

/******************************** Short and long presses ********************************/

// A press shorter than the long-press delay is a short press, sent on release
void test_quick_press_is_a_short_press() {
	TEST_ASSERT_EQUAL(ButtonEvent::ShortPress, pressFor(200));
}

// A press held past the long-press delay is a long press, sent while still held
void test_held_press_is_a_long_press() {
	ButtonEvent events[2] = { ButtonEvent::None, ButtonEvent::None };

	// Hold past the long-press delay (plus the debounce delay before the press counts)
	const uint8_t eventCount = holdLevel(true, config::BUTTON_DEBOUNCE_DELAY_MS + config::BUTTON_LONG_PRESS_DELAY_MS + 10, events, 2);

	// Expect a long press before the release
	TEST_ASSERT_EQUAL_UINT8(1, eventCount);
	TEST_ASSERT_EQUAL(ButtonEvent::LongPress, events[0]);
}

// Releasing after a long press doesn't add a short press
void test_release_after_long_press_sends_nothing() {
	TEST_ASSERT_EQUAL(ButtonEvent::LongPress, pressFor(config::BUTTON_DEBOUNCE_DELAY_MS + config::BUTTON_LONG_PRESS_DELAY_MS + 100));
}

// Holding for a long time sends only one long press
void test_long_hold_sends_one_long_press() {
	TEST_ASSERT_EQUAL_UINT8(1, holdLevel(true, 10000));
}

/******************************** Debounce ********************************/

// A glitch shorter than the debounce delay is not a press
void test_glitch_is_ignored() {
	TEST_ASSERT_EQUAL(ButtonEvent::None, pressFor(config::BUTTON_DEBOUNCE_DELAY_MS / 2));
}

// Contact bounce while pressing gives a single short press
void test_bouncing_contact_gives_one_short_press() {
	// Bounce a few times, then hold and release
	for (uint8_t bounce = 0; bounce < 5; bounce += 1) {
		holdLevel(true, 3);
		holdLevel(false, 2);
	}

	// Expect one short press in total
	TEST_ASSERT_EQUAL(ButtonEvent::ShortPress, pressFor(150));
}

/******************************** Busy firmware ********************************/

// A press still held after busy work is dropped: no long press, and no short press on release
void test_press_held_through_busy_work_is_dropped() {
	// The press started while the firmware was busy, and is still held when it polls again
	holdLevel(true, 5);
	resetButtonDebouncer(debouncer, true, currentTimeMs);

	// Expect nothing while held, nor on release
	TEST_ASSERT_EQUAL_UINT8(0, holdLevel(true, 2000));
	TEST_ASSERT_EQUAL_UINT8(0, holdLevel(false, 200));

	// The next press works again
	TEST_ASSERT_EQUAL(ButtonEvent::ShortPress, pressFor(200));
}

// A button held at boot gives no event until it is pressed again
void test_button_held_at_boot_is_ignored() {
	resetButtonDebouncer(debouncer, true, currentTimeMs);
	TEST_ASSERT_EQUAL_UINT8(0, holdLevel(true, 2000));
	TEST_ASSERT_EQUAL_UINT8(0, holdLevel(false, 200));
}

/******************************** Clock rollover ********************************/

// Presses work across the millis() rollover
void test_presses_work_across_clock_rollover() {
	// Start right before the clock wraps around
	currentTimeMs = 0xFFFFFFFFUL - 20;
	resetButtonDebouncer(debouncer, false, currentTimeMs);

	// Expect a short press, then a long press, through the wrap
	TEST_ASSERT_EQUAL(ButtonEvent::ShortPress, pressFor(200));
	TEST_ASSERT_EQUAL(ButtonEvent::LongPress, pressFor(config::BUTTON_DEBOUNCE_DELAY_MS + config::BUTTON_LONG_PRESS_DELAY_MS + 100));
}

// A press that starts exactly when the clock reads zero still counts
void test_press_starting_at_time_zero_counts() {
	// Make the press register at time zero (the old code used 0 as "no press")
	currentTimeMs = 0xFFFFFFFFUL - config::BUTTON_DEBOUNCE_DELAY_MS;
	resetButtonDebouncer(debouncer, false, currentTimeMs);

	// Expect a short press
	TEST_ASSERT_EQUAL(ButtonEvent::ShortPress, pressFor(200));
}

// Run all the button tests
int main() {
	UNITY_BEGIN();
	RUN_TEST(test_quick_press_is_a_short_press);
	RUN_TEST(test_held_press_is_a_long_press);
	RUN_TEST(test_release_after_long_press_sends_nothing);
	RUN_TEST(test_long_hold_sends_one_long_press);
	RUN_TEST(test_glitch_is_ignored);
	RUN_TEST(test_bouncing_contact_gives_one_short_press);
	RUN_TEST(test_press_held_through_busy_work_is_dropped);
	RUN_TEST(test_button_held_at_boot_is_ignored);
	RUN_TEST(test_presses_work_across_clock_rollover);
	RUN_TEST(test_press_starting_at_time_zero_counts);
	return UNITY_END();
}