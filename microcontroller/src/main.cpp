#include <WiFi.h>
#include <gizmo/device.h>
#include <gizmo/format.h>
#include "config/config.h"
#include "button/button.h"
#include "buzzer/buzzer.h"
#include "display/display.h"
#include "menu/menu.h"
#include "nvr/nvr_settings.h"
#include "reolink/reolink.h"
#include "remote/remote_control.h"
#include "serial/serial.h"
#include "server/server.h"
#include "storage/storage.h"
#include "wifi/wifi.h"

namespace {
	// NVR login revision the NVR client was reset for, and whether the login still needs a check once the Wi-Fi is up
	uint32_t appliedNvrRevision = 0;
	bool shouldTestNvr = true;

	// What the idle screen was last drawn for (it shows the setup steps, so it's redrawn when they change)
	bool wasHotspotActive = false;
	bool wasNvrConfigured = false;
	bool wasWiFiConnected = false;

	// Restore retry state (used when a restore fails or some muted cameras are offline)
	bool shouldRetryRestoreNow = false;
	uint32_t lastRestoreAttemptTimeMs = 0;

	// When the button was last polled (a long gap means the firmware was busy and didn't see the presses meanwhile)
	uint32_t lastButtonPollTimeMs = 0;

	// How to cancel, shown instead of the countdown for a moment after a short press while the timer runs
	bool isCancelHintShown = false;
	uint32_t cancelHintShownAtMs = 0;

	// Check whether the cancel hint should still be on screen
	bool isCancelHintVisible() {
		if (isCancelHintShown && millis() - cancelHintShownAtMs >= config::SHOW_DISPLAY_MESSAGE_MS) {
			isCancelHintShown = false;
		}
		return isCancelHintShown;
	}

	// Publish what the switch is doing for the web app (busy before slow work, so the web app waits instead of queuing more)
	void publishStatus(bool isBusy) {
		publishSwitchStatus({ isTimerRunning(), getRemainingSeconds(), getSelectedDurationSeconds(), hasPendingReolinkRestore(), isBusy, getReolinkStatus(), getReolinkCameraCount() });
	}

	// Print the current timer state to the serial console
	void printMenuStatus() {
		char timeBuffer[gizmo::DURATION_TEXT_SIZE] = { 0 };
		char detailLine[sizeof("Mute for ") + gizmo::DURATION_TEXT_SIZE] = { 0 }; // Fits any duration (the LCD crops to its width, and a native test checks the real lines fit)

		// Format the remaining time for display
		printLogf("%s\n", getMenuDisplayText());

		// Explain how to cancel for a moment after a short press (it doesn't change a running timer)
		if (isTimerRunning() && isCancelHintVisible()) {
			displayShowScreen("Alerts OFF", "Hold to cancel");
			return;
		}

		// Show the current timer state on the LCD
		if (isTimerRunning()) {
			gizmo::formatDuration(getRemainingSeconds(), timeBuffer, sizeof(timeBuffer));
			snprintf(detailLine, sizeof(detailLine), "%s left", timeBuffer);
			displayShowScreen("Alerts OFF", detailLine);
			return;
		}

		// Warn when some cameras could not be restored yet
		if (getSelectedDurationSeconds() == 0 && hasPendingReolinkRestore()) {
			displayShowScreen("Alerts Still OFF", "Hold to retry");
			return;
		}

		// Show the setup steps when idle: join the hotspot to set the Wi-Fi, then set up the NVR from the web app
		if (getSelectedDurationSeconds() == 0 && isSetupHotspotActive()) {
			displayShowScreen("Join Wi-Fi Setup", config::ACCESS_POINT_NAME);
			return;
		}
		if (getSelectedDurationSeconds() == 0 && !isNvrConfigured(getNvrSettings())) {
			displayShowScreen("Set Up NVR: Open", isWiFiConnected() ? WiFi.localIP().toString().c_str() : "the web app");
			return;
		}

		// Show the welcome screen when idle
		if (getSelectedDurationSeconds() == 0) {
			displayShowScreen("Reolink Switch", "Tap to start");
			return;
		}

		// Show the selected duration when idle
		gizmo::formatDuration(getSelectedDurationSeconds(), timeBuffer, sizeof(timeBuffer));
		snprintf(detailLine, sizeof(detailLine), "Mute for %s", timeBuffer);
		displayShowScreen("Hold to mute", detailLine);
	}

	// Restore the muted cameras and show the real outcome on the LCD (background retries don't beep on failure)
	void restoreAlerts(const char *successTitle, bool isBackgroundRetry = false) {
		publishStatus(true);

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
				if (!isBackgroundRetry) {
					doubleBuzz(config::SHORT_BUZZ_MS);
				}
				break;

			// The NVR could not be reached or rejected the request
			case ReolinkRestoreResult::Failed:
			default:
				printLogln("Failed to restore Reolink notifications and recording. Retrying later.");
				displayShowScreen("Restore Failed", "Will retry");
				if (!isBackgroundRetry) {
					doubleBuzz(config::SHORT_BUZZ_MS);
				}
				break;
		}

		// Return to the idle screen
		delay(config::SHOW_DISPLAY_MESSAGE_MS);
		printMenuStatus();
	}

	// Retry a pending restore in the background while the timer is idle
	void manageRestoreRetries() {
		// Only retry while idle, with Wi-Fi, and with cameras still muted
		if (isTimerRunning() || !isWiFiConnected() || !hasPendingReolinkRestore()) {
			return;
		}

		// Wait for the retry interval unless a retry was requested right away
		if (!shouldRetryRestoreNow && millis() - lastRestoreAttemptTimeMs < config::REOLINK_RESTORE_RETRY_MS) {
			return;
		}

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
				publishStatus(true);
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
				// A short press doesn't change a running timer, so show how to cancel it
				if (isTimerRunning()) {
					isCancelHintShown = true;
					cancelHintShownAtMs = millis();
					printMenuStatus();
					singleBuzz(config::SHORT_BUZZ_MS);
					break;
				}

				// Otherwise change the selected duration
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

namespace {
	// Run a command from the web app, through the same steps as the button
	void handleRemoteCommand(const RemoteCommand &command) {
		switch (command.type) {
			// Select the duration and start the timer
			case RemoteCommandType::StartQuietMode:
				if (!isTimerRunning()) {
					handleMenuEvent(setSelectedDuration(command.durationSeconds));
					handleMenuEvent(activateOrCancelTimer());
				}
				break;

			// Cancel the running timer
			case RemoteCommandType::CancelQuietMode:
				if (isTimerRunning()) {
					handleMenuEvent(activateOrCancelTimer());
				}
				break;

			// Retry restoring the alerts that are still off
			case RemoteCommandType::RetryRestore:
				if (!isTimerRunning() && hasPendingReolinkRestore()) {
					restoreAlerts("Welcome Back");
				}
				break;

			// Check the NVR login
			case RemoteCommandType::TestNvr:
				publishStatus(true);
				shouldTestNvr = false;
				testReolinkConnection();
				printMenuStatus();
				break;

			// No command
			case RemoteCommandType::None:
			default:
				break;
		}
	}

	// Apply a login changed in the web app, and check the login once the Wi-Fi is up
	void manageNvrLogin() {
		// Drop the old session when the login changed
		const uint32_t revision = getNvrSettingsRevision();
		if (revision != appliedNvrRevision) {
			appliedNvrRevision = revision;
			resetReolinkSession();
			shouldTestNvr = true;
		}

		// Check the login once the Wi-Fi is up (at boot, and after a change)
		if (shouldTestNvr && isWiFiConnected() && !isTimerRunning() && isNvrConfigured(getNvrSettings())) {
			shouldTestNvr = false;
			publishStatus(true);
			testReolinkConnection();
			printMenuStatus();
		}
	}

	// Redraw the idle screen when the setup steps change (hotspot opened, Wi-Fi up, NVR set)
	void manageIdleScreen() {
		const bool isHotspotActive = isSetupHotspotActive();
		const bool isConfigured = isNvrConfigured(getNvrSettings());
		const bool isConnected = isWiFiConnected();
		if (isHotspotActive == wasHotspotActive && isConfigured == wasNvrConfigured && isConnected == wasWiFiConnected) {
			return;
		}

		// Remember the new steps, and redraw only the idle screen (the timer screens redraw themselves)
		wasHotspotActive = isHotspotActive;
		wasNvrConfigured = isConfigured;
		wasWiFiConnected = isConnected;
		if (!isTimerRunning() && getSelectedDurationSeconds() == 0) {
			printMenuStatus();
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

	// Load the NVR login (set from the web app)
	loadNvrSettings();
	setupReolink();
	appliedNvrRevision = getNvrSettingsRevision();

	// Join the saved network or open the setup hotspot, and serve the web app
	setupWiFi();
	setupServer();
	printLogln("Web app ready!");

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
	// Keep the Wi-Fi connection alive, and restart when the web app asks
	manageWiFiConnection();
	gizmo::manageDevice();

	// Apply a changed NVR login, and redraw the setup steps when they change
	manageNvrLogin();
	manageIdleScreen();

	// Run a command from the web app
	const RemoteCommand remoteCommand = takeRemoteCommand();
	if (remoteCommand.type != RemoteCommandType::None) {
		handleRemoteCommand(remoteCommand);
	}

	// Drop a press made while the firmware was busy since the last poll (the screen said to wait, and a late press would act on another screen)
	if (millis() - lastButtonPollTimeMs >= config::BUTTON_BUSY_GAP_MS) {
		ignoreButtonUntilReleased();
	}

	// Poll the button for events
	const ButtonEvent buttonEvent = pollButtonEvent();
	lastButtonPollTimeMs = millis();

	// Handle any button event
	if (buttonEvent != ButtonEvent::None) {
		handleButtonEvent(buttonEvent);
	}

	// Update the menu state and handle any events
	handleMenuEvent(updateMenu());

	// Retry restoring cameras that are still muted
	manageRestoreRetries();

	// Publish what the switch is doing for the web app
	publishStatus(false);
}