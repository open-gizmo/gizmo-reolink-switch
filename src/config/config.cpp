#include "config.h"

#ifndef PIO_WIFI_SSID
#define PIO_WIFI_SSID ""
#endif

#ifndef PIO_WIFI_PASSWORD
#define PIO_WIFI_PASSWORD ""
#endif

#ifndef PIO_REOLINK_HOST
#define PIO_REOLINK_HOST ""
#endif

#ifndef PIO_REOLINK_USERNAME
#define PIO_REOLINK_USERNAME ""
#endif

#ifndef PIO_REOLINK_PASSWORD
#define PIO_REOLINK_PASSWORD ""
#endif

#ifndef PIO_DEV_MODE
#define PIO_DEV_MODE 0
#endif

namespace config {
	// Env configuration flags
	const bool DEVMODE = PIO_DEV_MODE != 0;

	// Pin assignments
	const uint8_t BUZZER_PIN = 12;
	const uint8_t BUTTON_PIN = 13;
	const uint8_t DISPLAY_SDA_PIN = 26;
	const uint8_t DISPLAY_SCL_PIN = 25;

	// WiFi credentials
	const char WIFI_SSID[] = PIO_WIFI_SSID;
	const char WIFI_PASSWORD[] = PIO_WIFI_PASSWORD;

	// Reolink credentials
	const char REOLINK_HOST[] = PIO_REOLINK_HOST;
	const char REOLINK_USERNAME[] = PIO_REOLINK_USERNAME;
	const char REOLINK_PASSWORD[] = PIO_REOLINK_PASSWORD;

	// Timing constants
	const uint32_t BUTTON_DEBOUNCE_DELAY_MS = 40;
	const uint32_t BUTTON_LONG_PRESS_DELAY_MS = 700;
	const uint32_t WIFI_CONNECT_RETRY_DELAY_MS = 500;
	const uint32_t REOLINK_HTTP_TIMEOUT_MS = 10000;

	// Timer duration constants
	const uint32_t TIMER_SHORT_DURATION_STEP_SECONDS = 1UL * 60UL;
	const uint32_t TIMER_LONG_DURATION_STEP_SECONDS = 5UL * 60UL;
	const uint32_t TIMER_SHORT_DURATION_LIMIT_SECONDS = 5UL * 60UL;

	// Buzzer duration constants
	const uint32_t MENU_SELECTION_BUZZ_DURATION_MS = 40;
	const uint32_t TIMER_START_BUZZ_DURATION_MS = 1000;
	const uint32_t TIMER_WARNING_BUZZ_DURATION_MS = 120;
	const uint32_t TIMER_FINISH_BUZZ_DURATION_MS = 1000;
	const uint8_t REOLINK_MAX_CHANNELS = 16;

	// Display constants
	const uint8_t DISPLAY_I2C_ADDRESS = 0x27;
	const uint8_t DISPLAY_COLUMNS = 16;
	const uint8_t DISPLAY_ROWS = 2;
}