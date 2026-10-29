#include "config/config.h"
#include "button/button.h"
#include "buzzer/buzzer.h"
#include "display/display.h"
#include "format/format.h"
#include "menu/menu.h"
#include "reolink/reolink.h"
#include "serial/serial.h"
#include "storage/storage.h"
#include "wifi/wifi.h"

namespace {
	// If the program is in an idle state where it cannot operate
	bool programIsIdle = false;

	// Restore retry state (used when a restore fails or some muted cameras are offline)
	bool shouldRetryRestoreNow = false;
	uint32_t lastRestoreAttemptTimeMs = 0;

	// Print the current timer state to the serial console
	void printMenuStatus() {
		char timeBuffer[DURATION_TEXT_SIZE] = {0};
		char detailLine[sizeof("Mute for ") + DURATION_TEXT_SIZE] = {0}; // Fits any duration (the LCD crops to its width, and a native test checks the real lines fit)

		// Format the remaining time for display
		printLogf("%s\n", getMenuDisplayText());

		// Show the current timer state on the LCD
		if (isTimerRunning()) {
			formatDuration(getRemainingSeconds(), timeBuffer, sizeof(timeBuffer));
			snprintf(detailLine, sizeof(detailLine), "%s left", timeBuffer);
			displayShowScreen("Alerts OFF", detailLine);
			return;
		}

		// Warn when some cameras could not be restored yet
		if (getSelectedDurationSeconds() == 0 && hasPendingReolinkRestore()) {
			displayShowScreen("Alerts Still OFF", "Hold to retry");
			return;
		}

		// Show the welcome screen when idle
		if (getSelectedDurationSeconds() == 0) {
			displayShowScreen("Reolink Switch", "Tap to start");
			return;
		}

		// Show the selected duration when idle
		formatDuration(getSelectedDurationSeconds(), timeBuffer, sizeof(timeBuffer));
		snprintf(detailLine, sizeof(detailLine), "Mute for %s", timeBuffer);
		displayShowScreen("Hold to mute", detailLine);
	}

	// Restore the muted cameras and show the real outcome on the LCD (background retries don't beep on failure)
	void restoreAlerts(const char *successTitle, bool isBackgroundRetry = false) {
		// Remember the attempt for the retry timer
		lastRestoreAttemptTimeMs = millis();
		shouldRetryRestoreNow = false;

		// Attempt the restore and report its outcome
		switch (restoreReolinkNotificationsAndRecording()) {
			// All muted cameras are back on
			case ReolinkRestoreResult::Restored:
				printLogln("Alerts restored. Returning to idle state.");
				displayShowScreen(successTitle, "Alerts restored!");
				singleBuzz(config::LONG_BUZZ_MS);
				break;

			// Some muted cameras are offline and will be restored when they come back
			case ReolinkRestoreResult::PartiallyRestored:
				printLogln("Some muted cameras are offline. Retrying later.");
				displayShowScreen("Cam Offline!", "Will retry");
				if (!isBackgroundRetry) doubleBuzz(config::SHORT_BUZZ_MS);
				break;

			// The NVR could not be reached or rejected the request
			case ReolinkRestoreResult::Failed:
			default:
				printLogln("Failed to restore Reolink notifications and recording. Retrying later.");
				displayShowScreen("Restore Failed", "Will retry");
				if (!isBackgroundRetry) doubleBuzz(config::SHORT_BUZZ_MS);
				break;
		}

		// Return to the idle screen
		delay(config::SHOW_DISPLAY_MESSAGE_MS);
		printMenuStatus();
	}

	// Retry a pending restore in the background while the timer is idle
	void manageRestoreRetries() {
		// Only retry while idle, with Wi-Fi, and with cameras still muted
		if (isTimerRunning() || !isWiFiConnected() || !hasPendingReolinkRestore()) return;

		// Wait for the retry interval unless a retry was requested right away
		if (!shouldRetryRestoreNow && millis() - lastRestoreAttemptTimeMs < config::REOLINK_RESTORE_RETRY_MS) return;

		// Retry the restore
		printLogln("Retrying pending Reolink restore...");
		restoreAlerts("Welcome Back", true);
	}

	// React to timer menu events with logging and buzzer feedback
	void handleMenuEvent(MenuEvent event) {
		switch (event) {
			// Handle a selection change
			case MenuEvent::SelectionChanged:
				printMenuStatus();
				singleBuzz(config::SHORT_BUZZ_MS);
				break;

			// Handle the start of a countdown timer
			case MenuEvent::TimerStarted: {
				// Additional feedback
				singleBuzz(config::SHORT_BUZZ_MS);

				// Attempt to disable Reolink notifications and recording when the timer starts
				const ReolinkMuteResult muteResult = disableReolinkNotificationsAndRecording();
				if (muteResult == ReolinkMuteResult::Failed) {
					printLogln("Failed to disable Reolink notifications and recording. Cancelling timer start.");
					stopTimer();
					displayShowScreen("Quiet Mode Fail!", "Try again!");
					delay(config::SHOW_DISPLAY_MESSAGE_MS);

					// Restore any camera the failed request may have muted
					if (hasPendingReolinkRestore()) {
						restoreAlerts("Mute Cancelled");
					} else {
						printMenuStatus();
					}
					break;
				}

				// Warn when only some settings could be muted (the countdown still runs, to restore them at the end)
				if (muteResult == ReolinkMuteResult::PartiallyMuted) {
					doubleBuzz(config::SHORT_BUZZ_MS);
				}

				// Start the countdown now that the cameras are muted (so the network calls don't eat into it)
				restartTimerCountdown();
				printLogln("Timer started");
				printMenuStatus();
				singleBuzz(config::LONG_BUZZ_MS);
				break;
			}

			// Handle a countdown timer tick
			case MenuEvent::TimerTick:
				printMenuStatus();

				// Provide a short buzz when the timer is about to complete
				if (getRemainingSeconds() == 2 || getRemainingSeconds() == 1) {
					singleBuzz(config::SHORT_BUZZ_MS);
				}

				break;

			// Handle the completion of a countdown timer
			case MenuEvent::TimerCompleted:
				printLogln("Timer completed.");
				restoreAlerts("Time Is Up");
				break;

			// Handle the cancellation of a countdown timer
			case MenuEvent::TimerCancelled:
				// Additional feedback
				singleBuzz(config::SHORT_BUZZ_MS);
				printLogln("Timer cancelled.");
				restoreAlerts("Welcome Back");
				break;

			// Handle no event
			case MenuEvent::None:
			default:
				break;
		}
	}

	// Handle a button event by performing the corresponding action
	void handleButtonEvent(ButtonEvent event) {
		switch (event) {
			// Handle a short button press
			case ButtonEvent::ShortPress:
				handleMenuEvent(increaseSelectedDuration());
				break;

			// Handle a long button press
			case ButtonEvent::LongPress:
				// Retry a pending restore right away when no duration is selected
				if (!isTimerRunning() && getSelectedDurationSeconds() == 0 && hasPendingReolinkRestore()) {
					restoreAlerts("Welcome Back");
					break;
				}

				// Otherwise start or cancel the timer
				handleMenuEvent(activateOrCancelTimer());
				break;

			// Handle no event
			case ButtonEvent::None:
			default:
				break;
		}
	}
}

// Setup
void setup() {
	// Init serial
	initSerial();
	printLogln("Serial initialized!");

	// Init display
	initDisplay();
	printLogln("Display initialized!");
	displayShowScreen("Reolink Switch", "Booting now...");
	delay(config::SHOW_DISPLAY_MESSAGE_MS);

	// Init buzzer
	setupBuzzer();
	printLogln("Buzzer initialized!");

	// Init button
	setupButton();
	printLogln("Button initialized!");

	// Init storage (loads the cameras still muted before the last reboot)
	setupStorage();
	printLogln("Storage initialized!");

	// Connect WiFi
	if (setupWiFi() == WiFiSetupResult::MissingCredentials) {
		printLogln("Program halted. Configure WiFi credentials and reboot.");
		displayShowScreen("Wi-Fi Needed", "Add credentials!");
		programIsIdle = true;
		return;
	}

	// Configure Reolink API
	if (setupReolink() == ReolinkSetupResult::MissingCredentials) {
		printLogln("Program halted. Configure Reolink credentials and reboot.");
		displayShowScreen("NVR Needed", "Add credentials!");
		programIsIdle = true;
		return;
	}

	// Init menu
	setupMenu();
	printLogln("Menu initialized!");

	// Restore the cameras right away if the device rebooted while they were muted
	if (hasPendingReolinkRestore()) {
		printLogln("Cameras were still muted before the last reboot. Restoring them now.");
		shouldRetryRestoreNow = true;
	}
	printLogln("Short press: add 1 minute below 5 minutes, otherwise add 5 minutes (after 60 minutes, back to zero)");
	printLogln("Long press: start or cancel timer");
	printMenuStatus();
}

// Loop
void loop() {
	// If the program is idle, do nothing
	if (programIsIdle) {
		delay(1000);
		return;
	}

	// Keep the Wi-Fi connection alive
	manageWiFiConnection();

	// Poll the button for events
	const ButtonEvent buttonEvent = pollButtonEvent();

	// Handle any button event
	if (buttonEvent != ButtonEvent::None) {
		handleButtonEvent(buttonEvent);
	}

	// Update the menu state and handle any events
	handleMenuEvent(updateMenu());

	// Retry restoring cameras that are still muted
	manageRestoreRetries();
}