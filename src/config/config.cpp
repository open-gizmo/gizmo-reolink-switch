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
	const uint32_t BUTTON_BUSY_GAP_MS = 250; // Longer than this between two button polls means slow work (a request, a long beep), so a press made meanwhile is dropped
	const uint32_t WIFI_CONNECT_RETRY_DELAY_MS = 500;
	const uint32_t REOLINK_HTTP_TIMEOUT_MS = 10000;
	const uint32_t REOLINK_RESTORE_RETRY_MS = 60000; // Retry a failed or partial restore every minute
	const uint32_t REOLINK_LOGIN_PAUSE_MS = 10UL * 60UL * 1000UL; // Wait after a refused login (NVRs lock the account after a few failures)
	const uint32_t WIFI_CONNECT_TIMEOUT_MS = 20000; // Stop waiting at boot and keep retrying in the background
	const uint32_t WIFI_RECONNECT_INTERVAL_MS = 30000; // Time without Wi-Fi before forcing a reconnect

	// Timer duration constants
	const uint32_t TIMER_SHORT_DURATION_STEP_SECONDS = 1UL * 60UL;
	const uint32_t TIMER_LONG_DURATION_STEP_SECONDS = 5UL * 60UL;
	const uint32_t TIMER_SHORT_DURATION_LIMIT_SECONDS = 5UL * 60UL;
	const uint32_t TIMER_MAX_DURATION_SECONDS = 60UL * 60UL; // Keep accidental long mutes short (the next press wraps back to zero)

	// Buzzer duration constants
	const uint32_t SHORT_BUZZ_MS = 50;
	const uint32_t LONG_BUZZ_MS = 800;

	// Reolink constants
	const uint8_t REOLINK_MAX_CHANNELS = 16;
	const uint32_t REOLINK_MAX_REPLY_BYTES = 16384; // NVR replies are a few KB, anything larger is refused before it eats the heap TLS needs

	// Display constants
	const uint32_t SHOW_DISPLAY_MESSAGE_MS = 1000;
	const uint8_t DISPLAY_I2C_ADDRESS = 0x27;
	const uint8_t DISPLAY_COLUMNS = 16;
	const uint8_t DISPLAY_ROWS = 2;
}