#include <Arduino.h>
#include <unity.h>

#include <gizmo/format.h>
#include "config/config.h"

namespace {
	// LCD texts built around the formatted duration in main.cpp
	constexpr size_t IDLE_LINE_PREFIX_LENGTH = sizeof("Mute for ") - 1;
	constexpr size_t COUNTDOWN_LINE_SUFFIX_LENGTH = sizeof(" left") - 1;

	// Format a duration into a buffer of the documented size
	void formatIntoBuffer(uint32_t totalSeconds, char *buffer) {
		gizmo::formatDuration(totalSeconds, buffer, gizmo::DURATION_TEXT_SIZE);
	}
}

// Nothing to prepare before each test
void setUp() {}

// Nothing to clean up after each test
void tearDown() {}

// The LCD lines built around the duration must fit the display, even at the maximum
void test_lcd_lines_fit_at_the_maximum_duration() {
	char buffer[gizmo::DURATION_TEXT_SIZE] = {0};

	// Format the configured maximum
	formatIntoBuffer(config::TIMER_MAX_DURATION_SECONDS, buffer);

	// Expect "Mute for MM:SS" and "MM:SS left" to fit on one LCD row (fails if the maximum grows past 99 minutes)
	TEST_ASSERT_LESS_OR_EQUAL_size_t(config::DISPLAY_COLUMNS, IDLE_LINE_PREFIX_LENGTH + strlen(buffer));
	TEST_ASSERT_LESS_OR_EQUAL_size_t(config::DISPLAY_COLUMNS, strlen(buffer) + COUNTDOWN_LINE_SUFFIX_LENGTH);
}

// Run all the LCD line tests
int main() {
	UNITY_BEGIN();
	RUN_TEST(test_lcd_lines_fit_at_the_maximum_duration);
	return UNITY_END();
}