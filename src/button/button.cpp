#include "config/config.h"
#include "button.h"

namespace {
	// Debounce and long-press state of the button
	ButtonDebouncer buttonDebouncer;

	// Read the button (wired to ground, so pressed reads LOW)
	bool isButtonPressed() {
		return digitalRead(config::BUTTON_PIN) == LOW;
	}
}

// Setup button
void setupButton() {
	pinMode(config::BUTTON_PIN, INPUT_PULLUP);
	resetButtonDebouncer(buttonDebouncer, isButtonPressed(), millis());
}

// Poll the button and emit a semantic event
ButtonEvent pollButtonEvent() {
	return updateButtonDebouncer(buttonDebouncer, isButtonPressed(), millis());
}

// Drop a press made while the firmware was busy (the button wasn't polled, so its timing is unknown)
void ignoreButtonUntilReleased() {
	resetButtonDebouncer(buttonDebouncer, isButtonPressed(), millis());
}