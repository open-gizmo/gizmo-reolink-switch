#include "screens.h"

#include <stdio.h>
#include <string.h>

#include <gizmo/format.h>

namespace {
	// Copy a text into a line, cut off at the width of the LCD
	void copyLine(char *line, const char *text) {
		snprintf(line, SCREEN_LINE_SIZE, "%s", text != nullptr ? text : "");
	}

	// Build a screen from two texts
	ScreenLines makeLines(const char *title, const char *detail) {
		ScreenLines lines = {};
		copyLine(lines.title, title);
		copyLine(lines.detail, detail);
		return lines;
	}

	// Show a temporary screen from now on
	void showScreen(TemporaryScreenState &state, TemporaryScreen screen, uint32_t nowMs) {
		state.screen = screen;
		state.shownAtMs = nowMs;
	}
}

// Go back to the normal screen
void clearTemporaryScreen(TemporaryScreenState &state) {
	state.screen = TemporaryScreen::None;
	state.shownAtMs = 0;
	state.factoryResetRemainingMs = 0;
	state.messageTitle[0] = '\0';
	state.messageDetail[0] = '\0';
}

// Show the time left while the BOOT button is held
void showFactoryResetCountdown(TemporaryScreenState &state, uint32_t remainingMs, uint32_t nowMs) {
	showScreen(state, TemporaryScreen::FactoryResetCountdown, nowMs);
	state.factoryResetRemainingMs = remainingMs;
}

// Say why the factory reset was refused
void showFactoryResetRefused(TemporaryScreenState &state, uint32_t nowMs) {
	showScreen(state, TemporaryScreen::FactoryResetRefused, nowMs);
}

// Show how to cancel the running timer
void showCancelHint(TemporaryScreenState &state, uint32_t nowMs) {
	showScreen(state, TemporaryScreen::CancelHint, nowMs);
}

// Show an outcome for a moment
void showMessage(TemporaryScreenState &state, const char *title, const char *detail, uint32_t nowMs) {
	showScreen(state, TemporaryScreen::Message, nowMs);
	copyLine(state.messageTitle, title);
	copyLine(state.messageDetail, detail);
}

// Check whether an outcome is on screen
bool isMessageShown(const TemporaryScreenState &state) {
	return state.screen == TemporaryScreen::Message;
}

// End a temporary screen that has been shown long enough, true when it just ended
bool expireTemporaryScreen(TemporaryScreenState &state, uint32_t nowMs, uint32_t durationMs) {
	// Nothing to end, or the factory reset countdown, which lasts while the button is held
	if (state.screen == TemporaryScreen::None || state.screen == TemporaryScreen::FactoryResetCountdown) {
		return false;
	}

	// Keep it until it has been shown long enough (the elapsed time stays right across a millis() rollover)
	if (nowMs - state.shownAtMs < durationMs) {
		return false;
	}
	clearTemporaryScreen(state);
	return true;
}

// Build the lines to show: the temporary screen, or the normal one
ScreenLines buildScreenLines(const TemporaryScreenState &state, const SwitchScreenInputs &inputs) {
	char timeBuffer[gizmo::DURATION_TEXT_SIZE] = { 0 };
	char detailLine[sizeof("Mute for ") + gizmo::DURATION_TEXT_SIZE] = { 0 }; // Fits any duration (the LCD crops to its width, and a native test checks the real lines fit)

	// Show the temporary screen first
	switch (state.screen) {
		// While the BOOT button is held for a factory reset, count down (releasing it stops)
		case TemporaryScreen::FactoryResetCountdown: {
			char countdownLine[sizeof("Reset in 4294967s")] = { 0 }; // Fits any value (the LCD crops to its width; it never goes above 10)
			snprintf(countdownLine, sizeof(countdownLine), "Reset in %lus", (unsigned long)((state.factoryResetRemainingMs + 999) / 1000));
			return makeLines(countdownLine, "Release to stop");
		}

		// For a moment after a refused factory reset, say why (the muted cameras must be restored first, with the saved login)
		case TemporaryScreen::FactoryResetRefused:
			return makeLines("Can't reset now", inputs.isTimerRunning ? "Stop timer first" : "Alerts still off");

		// Explain how to cancel for a moment after a short press (it doesn't change a running timer)
		case TemporaryScreen::CancelHint:
			if (inputs.isTimerRunning) {
				return makeLines("Alerts OFF", "Hold to cancel");
			}
			break;

		// Show the outcome of the last action
		case TemporaryScreen::Message:
			return makeLines(state.messageTitle, state.messageDetail);

		// No temporary screen
		case TemporaryScreen::None:
		default:
			break;
	}

	// Show the current timer state on the LCD
	if (inputs.isTimerRunning) {
		gizmo::formatDuration(inputs.remainingSeconds, timeBuffer, sizeof(timeBuffer));
		snprintf(detailLine, sizeof(detailLine), "%s left", timeBuffer);
		return makeLines("Alerts OFF", detailLine);
	}

	// Show the selected duration when idle
	if (inputs.selectedSeconds > 0) {
		gizmo::formatDuration(inputs.selectedSeconds, timeBuffer, sizeof(timeBuffer));
		snprintf(detailLine, sizeof(detailLine), "Mute for %s", timeBuffer);
		return makeLines("Hold to mute", detailLine);
	}

	// Warn when some cameras could not be restored yet
	if (inputs.hasAlertsStillOff) {
		return makeLines("Alerts Still OFF", "Hold to retry");
	}

	// Show the setup steps when idle: join the hotspot to set the Wi-Fi, then set up the NVR from the web app
	if (inputs.isHotspotActive) {
		return makeLines("Join Wi-Fi Setup", inputs.hotspotName);
	}
	if (!inputs.isNvrConfigured) {
		return makeLines("Set Up NVR: Open", inputs.address != nullptr ? inputs.address : "the web app");
	}

	// Show the welcome screen when idle
	return makeLines("Reolink Switch", "Tap to start");
}
