#include <Arduino.h>
#include <unity.h>

#include "config/config.h"
#include "format/format.h"

namespace {
	// LCD texts built around the formatted duration in main.cpp
	constexpr size_t IDLE_LINE_PREFIX_LENGTH = sizeof("Mute for ") - 1;
	constexpr size_t COUNTDOWN_LINE_SUFFIX_LENGTH = sizeof(" left") - 1;

	// Format a duration into a buffer of the documented size
	void formatIntoBuffer(uint32_t totalSeconds, char *buffer) {
		formatDuration(totalSeconds, buffer, DURATION_TEXT_SIZE);
	}
}

// Nothing to prepare before each test
void setUp() {}

// Nothing to clean up after each test
void tearDown() {}

// Short durations are shown with two-digit minutes and seconds
void test_formats_short_durations_as_mm_ss() {
	char buffer[DURATION_TEXT_SIZE] = {0};

	// Expect zero-padded minutes and seconds
	formatIntoBuffer(0, buffer);
	TEST_ASSERT_EQUAL_STRING("00:00", buffer);
	formatIntoBuffer(65, buffer);
	TEST_ASSERT_EQUAL_STRING("01:05", buffer);
	formatIntoBuffer(59 * 60 + 59, buffer);
	TEST_ASSERT_EQUAL_STRING("59:59", buffer);
}

// The maximum selectable duration is shown in full
void test_formats_the_maximum_duration() {
	char buffer[DURATION_TEXT_SIZE] = {0};

	// Format the configured maximum
	formatIntoBuffer(config::TIMER_MAX_DURATION_SECONDS, buffer);

	// Expect 60 minutes
	TEST_ASSERT_EQUAL_STRING("60:00", buffer);
}

// Durations of 100 minutes or more used to be cut off ("100:0") by a 6-byte buffer in main.cpp
void test_formats_three_digit_minutes_without_cutting_them() {
	char buffer[DURATION_TEXT_SIZE] = {0};

	// Format 100 minutes
	formatIntoBuffer(100 * 60, buffer);

	// Expect the full text
	TEST_ASSERT_EQUAL_STRING("100:00", buffer);
}

// The documented buffer size fits the largest possible value
void test_buffer_size_fits_any_duration() {
	char buffer[DURATION_TEXT_SIZE] = {0};

	// Format the largest value a uint32_t can hold
	formatIntoBuffer(UINT32_MAX, buffer);

	// Expect the full text, with nothing cut off
	TEST_ASSERT_EQUAL_STRING("71582788:15", buffer);
}

// A buffer that is too small is cut off safely instead of overflowing
void test_small_buffer_is_cut_off_safely() {
	char buffer[8] = { 'x', 'x', 'x', 'x', 'x', 'x', 'x', 'x' };

	// Format into the first 4 bytes only
	formatDuration(600, buffer, 4);

	// Expect a terminated, cut-off text and the rest of the buffer untouched
	TEST_ASSERT_EQUAL_STRING("10:", buffer);
	TEST_ASSERT_EQUAL_CHAR('x', buffer[4]);
}

// The LCD lines built around the duration must fit the display, even at the maximum
void test_lcd_lines_fit_at_the_maximum_duration() {
	char buffer[DURATION_TEXT_SIZE] = {0};

	// Format the configured maximum
	formatIntoBuffer(config::TIMER_MAX_DURATION_SECONDS, buffer);

	// Expect "Mute for MM:SS" and "MM:SS left" to fit on one LCD row (fails if the maximum grows past 99 minutes)
	TEST_ASSERT_LESS_OR_EQUAL_size_t(config::DISPLAY_COLUMNS, IDLE_LINE_PREFIX_LENGTH + strlen(buffer));
	TEST_ASSERT_LESS_OR_EQUAL_size_t(config::DISPLAY_COLUMNS, strlen(buffer) + COUNTDOWN_LINE_SUFFIX_LENGTH);
}

// Run all the format tests
int main() {
	UNITY_BEGIN();
	RUN_TEST(test_formats_short_durations_as_mm_ss);
	RUN_TEST(test_formats_the_maximum_duration);
	RUN_TEST(test_formats_three_digit_minutes_without_cutting_them);
	RUN_TEST(test_buffer_size_fits_any_duration);
	RUN_TEST(test_small_buffer_is_cut_off_safely);
	RUN_TEST(test_lcd_lines_fit_at_the_maximum_duration);
	return UNITY_END();
}