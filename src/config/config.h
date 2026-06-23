#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

namespace config {
	// Env configuration flags
	extern const bool DEVMODE;

	// Pin assignments
	extern const uint8_t BUZZER_PIN;
	extern const uint8_t BUTTON_PIN;
	extern const uint8_t DISPLAY_SDA_PIN;
	extern const uint8_t DISPLAY_SCL_PIN;

	// WiFi credentials
	extern const char WIFI_SSID[];
	extern const char WIFI_PASSWORD[];

	// Reolink credentials
	extern const char REOLINK_HOST[];
	extern const char REOLINK_USERNAME[];
	extern const char REOLINK_PASSWORD[];

	// Timing constants
	extern const uint32_t BUTTON_DEBOUNCE_DELAY_MS;
	extern const uint32_t BUTTON_LONG_PRESS_DELAY_MS;
	extern const uint32_t WIFI_CONNECT_RETRY_DELAY_MS;
	extern const uint32_t REOLINK_HTTP_TIMEOUT_MS;

	// Timer duration constants
	extern const uint32_t TIMER_SHORT_DURATION_STEP_SECONDS;
	extern const uint32_t TIMER_LONG_DURATION_STEP_SECONDS;
	extern const uint32_t TIMER_SHORT_DURATION_LIMIT_SECONDS;

	// Buzzer duration constants
	extern const uint32_t SHORT_BUZZ_MS;
	extern const uint32_t LONG_BUZZ_MS;

	// Reolink constants
	extern const uint8_t REOLINK_MAX_CHANNELS;

	// Display constants
	extern const uint32_t SHOW_DISPLAY_MESSAGE_MS;
	extern const uint8_t DISPLAY_I2C_ADDRESS;
	extern const uint8_t DISPLAY_COLUMNS;
	extern const uint8_t DISPLAY_ROWS;
}

#endif