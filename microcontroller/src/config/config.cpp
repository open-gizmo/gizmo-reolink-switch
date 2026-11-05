#include "config.h"

#ifndef PIO_DEV_MODE
#define PIO_DEV_MODE 0
#endif

#ifndef PIO_FIRMWARE_VERSION
#define PIO_FIRMWARE_VERSION "dev"
#endif

namespace config {
	// Env configuration flags
	const bool DEVMODE = PIO_DEV_MODE != 0;

	// Project details shown in the web app (the release build sets the version)
	const char PROJECT_NAME[] = "Gizmo Reolink Switch";
	const char FIRMWARE_VERSION[] = PIO_FIRMWARE_VERSION;

	// Network names and the Wi-Fi setup
	const char HOSTNAME[] = "gizmo-reolink-switch"; // http://gizmo-reolink-switch.local, always the project name
	const char ACCESS_POINT_NAME[] = "Gizmo-Reolink"; // Open hotspot for the Wi-Fi setup, when no saved network works (it fits the LCD)
	const char SETUP_PAGE_PATH[] = "/ap-setup"; // Page of the web app the hotspot clients are sent to
	const char ASSET_PATH_PREFIX[] = "/_nuxt/"; // Files of the web app, never redirected by the hotspot

	// Pin assignments
	const uint8_t BUZZER_PIN = 12;
	const uint8_t BUTTON_PIN = 13;
	const uint8_t DISPLAY_SDA_PIN = 26;
	const uint8_t DISPLAY_SCL_PIN = 25;

	// Timing constants
	const uint32_t BUTTON_DEBOUNCE_DELAY_MS = 40;
	const uint32_t BUTTON_LONG_PRESS_DELAY_MS = 700;
	const uint32_t BUTTON_BUSY_GAP_MS = 250; // Longer than this between two button polls means slow work (a request, a long beep), so a press made meanwhile is dropped
	const uint32_t REOLINK_HTTP_TIMEOUT_MS = 10000;
	const uint32_t REOLINK_RESTORE_RETRY_MS = 60000; // Retry a failed or partial restore every minute
	const uint32_t REOLINK_LOGIN_PAUSE_MS = 10UL * 60UL * 1000UL; // Wait after a refused login (NVRs lock the account after a few failures)

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