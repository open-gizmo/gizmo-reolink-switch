#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

namespace config {
	// Env configuration flags
	extern const bool DEVMODE;

	// Project details shown in the web app
	extern const char PROJECT_NAME[];
	extern const char FIRMWARE_VERSION[];

	// Network names and the Wi-Fi setup
	extern const char HOSTNAME[];
	extern const char ACCESS_POINT_NAME[];
	extern const char SETUP_PAGE_PATH[];
	extern const char ASSET_PATH_PREFIX[];

	// Pin assignments
	extern const uint8_t BUZZER_PIN;
	extern const uint8_t BUTTON_PIN;
	extern const uint8_t DISPLAY_SDA_PIN;
	extern const uint8_t DISPLAY_SCL_PIN;

	// NVS namespace of the project settings: the NVR login and the muted channels (the Wi-Fi has its own, from gizmo-common)
	extern const char STORAGE_NAMESPACE[];

	// NVR login sizes (set from the web app and saved in NVS), terminator included
	constexpr size_t NVR_HOST_SIZE = 65;
	constexpr size_t NVR_USERNAME_SIZE = 33;
	constexpr size_t NVR_PASSWORD_SIZE = 65;

	// Timing constants
	extern const uint32_t BUTTON_DEBOUNCE_DELAY_MS;
	extern const uint32_t BUTTON_LONG_PRESS_DELAY_MS;
	extern const uint32_t BUTTON_BUSY_GAP_MS;
	extern const uint32_t REOLINK_HTTP_TIMEOUT_MS;
	extern const uint32_t REOLINK_RESTORE_RETRY_MS;
	extern const uint32_t REOLINK_LOGIN_PAUSE_MS;

	// Timer duration constants
	extern const uint32_t TIMER_SHORT_DURATION_STEP_SECONDS;
	extern const uint32_t TIMER_LONG_DURATION_STEP_SECONDS;
	extern const uint32_t TIMER_SHORT_DURATION_LIMIT_SECONDS;
	extern const uint32_t TIMER_MAX_DURATION_SECONDS;

	// Buzzer duration constants
	extern const uint32_t SHORT_BUZZ_MS;
	extern const uint32_t LONG_BUZZ_MS;

	// Reolink constants
	extern const uint8_t REOLINK_MAX_CHANNELS;
	extern const uint32_t REOLINK_MAX_REPLY_BYTES;

	// Display constants
	extern const uint32_t SHOW_DISPLAY_MESSAGE_MS;
	extern const uint8_t DISPLAY_I2C_ADDRESS;
	extern const uint8_t DISPLAY_COLUMNS;
	extern const uint8_t DISPLAY_ROWS;
}

#endif