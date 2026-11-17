#ifndef SCREENS_H
#define SCREENS_H

#include <stddef.h>
#include <stdint.h>

// What the LCD shows, kept free of hardware headers so the native tests can drive it

// One LCD line: 16 columns and the terminator (longer texts are cut off)
constexpr size_t SCREEN_LINE_SIZE = 17;

// A screen shown on top of the normal one for a while: one at a time, a new one replaces the old
enum class TemporaryScreen : uint8_t {
	None,
	FactoryResetCountdown, // While the BOOT button is held for a factory reset (until it's released, refused or the reset runs)
	FactoryResetRefused, // For a moment after a refused factory reset, to say why
	CancelHint, // How to cancel, shown instead of the countdown for a moment after a short press while the timer runs
	Message // For a moment after an outcome (alerts restored, mute failed...), while the button is ignored
};

// The temporary screen and when it was shown
struct TemporaryScreenState {
	TemporaryScreen screen;
	uint32_t shownAtMs;
	uint32_t factoryResetRemainingMs; // Time left before the factory reset, for its countdown
	char messageTitle[SCREEN_LINE_SIZE];
	char messageDetail[SCREEN_LINE_SIZE];
};

// What the normal screen shows, from the state of the switch
struct SwitchScreenInputs {
	bool isTimerRunning;
	uint32_t remainingSeconds;
	uint32_t selectedSeconds;
	bool hasAlertsStillOff; // Some muted cameras could not be restored yet
	bool isHotspotActive;
	bool isNvrConfigured;
	const char *address; // The address of the web app, or nullptr without Wi-Fi
	const char *hotspotName;
};

// The two lines of a screen
struct ScreenLines {
	char title[SCREEN_LINE_SIZE];
	char detail[SCREEN_LINE_SIZE];
};

// Go back to the normal screen
void clearTemporaryScreen(TemporaryScreenState &state);

// Show the time left while the BOOT button is held
void showFactoryResetCountdown(TemporaryScreenState &state, uint32_t remainingMs, uint32_t nowMs);

// Say why the factory reset was refused
void showFactoryResetRefused(TemporaryScreenState &state, uint32_t nowMs);

// Show how to cancel the running timer
void showCancelHint(TemporaryScreenState &state, uint32_t nowMs);

// Show an outcome for a moment
void showMessage(TemporaryScreenState &state, const char *title, const char *detail, uint32_t nowMs);

// Check whether an outcome is on screen (the button is ignored meanwhile, so a late press can't act on the next screen)
bool isMessageShown(const TemporaryScreenState &state);

// End a temporary screen that has been shown long enough (the factory reset countdown stays until the button is released), true when it just ended
bool expireTemporaryScreen(TemporaryScreenState &state, uint32_t nowMs, uint32_t durationMs);

// Build the lines to show: the temporary screen, or the normal one
ScreenLines buildScreenLines(const TemporaryScreenState &state, const SwitchScreenInputs &inputs);

#endif
