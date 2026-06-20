#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

namespace config {
	extern const uint8_t BUZZER_PIN;
	extern const uint8_t BUTTON_PIN;

	extern const uint32_t BUTTON_DEBOUNCE_DELAY_MS;
	extern const uint32_t BUTTON_LONG_PRESS_DELAY_MS;

	extern const uint32_t TIMER_SHORT_DURATION_STEP_SECONDS;
	extern const uint32_t TIMER_LONG_DURATION_STEP_SECONDS;
	extern const uint32_t TIMER_SHORT_DURATION_LIMIT_SECONDS;

	extern const uint32_t MENU_SELECTION_BUZZ_DURATION_MS;
	extern const uint32_t TIMER_START_BUZZ_DURATION_MS;
	extern const uint32_t TIMER_WARNING_BUZZ_DURATION_MS;
	extern const uint32_t TIMER_FINISH_BUZZ_DURATION_MS;
}

#endif