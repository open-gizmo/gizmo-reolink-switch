#include "config/config.h"
#include "menu.h"

namespace {
	char menuDisplayText[64] = "Selected duration: 0:00";

	// Timer state
	bool timerRunning = false;
	uint32_t selectedDurationSeconds = 0;
	uint32_t countdownDurationSeconds = 0;
	uint32_t countdownStartTimeMs = 0;
	uint32_t lastDisplayedRemainingSeconds = 0;

	// Update the current status message using total seconds
	void updateMenuDisplayText(const char *prefix, uint32_t totalSeconds) {
		const uint32_t minutes = totalSeconds / 60;
		const uint32_t seconds = totalSeconds % 60;
		snprintf(menuDisplayText, sizeof(menuDisplayText), "%s%lu:%02lu", prefix, static_cast<unsigned long>(minutes), static_cast<unsigned long>(seconds));
	}

	// Reset the timer back to the normal selection state
	void resetTimerState() {
		timerRunning = false;
		selectedDurationSeconds = 0;
		countdownDurationSeconds = 0;
		countdownStartTimeMs = 0;
		lastDisplayedRemainingSeconds = 0;
		updateMenuDisplayText("Selected duration: ", selectedDurationSeconds);
	}
}

// Setup menu
void setupMenu() {
	resetTimerState();
}

// Increase the selected timer duration by 1 or 5 minutes depending on the current value
MenuEvent increaseSelectedDuration() {
	// Ignore selection changes while the timer is running
	if (timerRunning) {
		return MenuEvent::None;
	}

	// Determine the step size based on the current selected duration
	const uint32_t durationStepSeconds = selectedDurationSeconds < config::TIMER_SHORT_DURATION_LIMIT_SECONDS ? config::TIMER_SHORT_DURATION_STEP_SECONDS : config::TIMER_LONG_DURATION_STEP_SECONDS;

	// Increase the selected duration and update the display
	selectedDurationSeconds += durationStepSeconds;
	updateMenuDisplayText("Selected duration: ", selectedDurationSeconds);

	// Return the event indicating that the selection has changed
	return MenuEvent::SelectionChanged;
}

// Start the timer when idle or cancel it while it is running
MenuEvent activateOrCancelTimer() {
	// Cancel the timer if it is currently running
	if (timerRunning) {
		resetTimerState();
		return MenuEvent::TimerCancelled;
	}

	// Start the timer if a duration has been selected
	if (selectedDurationSeconds == 0) {
		return MenuEvent::None;
	}

	// Start the countdown timer
	timerRunning = true;
	countdownDurationSeconds = selectedDurationSeconds;
	countdownStartTimeMs = millis();
	lastDisplayedRemainingSeconds = countdownDurationSeconds;
	updateMenuDisplayText("Time remaining: ", countdownDurationSeconds);

	// Return the event indicating that the timer has started
	return MenuEvent::TimerStarted;
}

// Update the timer state and emit events when the display changes
MenuEvent updateMenu() {
	// Ignore updates while the timer is not running
	if (!timerRunning) {
		return MenuEvent::None;
	}

	// Calculate the elapsed time and check if the countdown has completed
	const uint32_t elapsedSeconds = (millis() - countdownStartTimeMs) / 1000;

	// If the countdown has completed, reset the timer state and emit the completion event
	if (elapsedSeconds >= countdownDurationSeconds) {
		resetTimerState();
		return MenuEvent::TimerCompleted;
	}

	// Calculate the remaining time and update the display if it has changed
	const uint32_t remainingSeconds = countdownDurationSeconds - elapsedSeconds;

	// Update the display only when the remaining time changes to avoid excessive updates
	if (remainingSeconds != lastDisplayedRemainingSeconds) {
		lastDisplayedRemainingSeconds = remainingSeconds;
		updateMenuDisplayText("Time remaining: ", remainingSeconds);
		return MenuEvent::TimerTick;
	}

	// No new menu event to emit
	return MenuEvent::None;
}

// Check whether the countdown timer is currently running
bool isTimerRunning() {
	return timerRunning;
}

// Get the currently selected timer duration in seconds while idle
uint32_t getSelectedDurationSeconds() {
	return selectedDurationSeconds;
}

// Get the current countdown value in seconds
uint32_t getRemainingSeconds() {
	return timerRunning ? lastDisplayedRemainingSeconds : 0;
}

// Get the current timer status text
const char *getMenuDisplayText() {
	return menuDisplayText;
}