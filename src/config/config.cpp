#include "config.h"

namespace config {
	// Pin assignments
	const uint8_t BUZZER_PIN = 12;
	const uint8_t BUTTON_PIN = 13;

	// Button timing constants
	const uint32_t BUTTON_DEBOUNCE_DELAY_MS = 40;
	const uint32_t BUTTON_LONG_PRESS_DELAY_MS = 700;

	// Timer duration constants
	const uint32_t TIMER_SHORT_DURATION_STEP_SECONDS = 1UL * 60UL;
	const uint32_t TIMER_LONG_DURATION_STEP_SECONDS = 5UL * 60UL;
	const uint32_t TIMER_SHORT_DURATION_LIMIT_SECONDS = 5UL * 60UL;

	// Buzzer duration constants
	const uint32_t MENU_SELECTION_BUZZ_DURATION_MS = 40;
	const uint32_t TIMER_START_BUZZ_DURATION_MS = 1000;
	const uint32_t TIMER_WARNING_BUZZ_DURATION_MS = 120;
	const uint32_t TIMER_FINISH_BUZZ_DURATION_MS = 1000;
}