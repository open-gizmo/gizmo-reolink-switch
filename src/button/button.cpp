#include "config/config.h"
#include "button.h"

namespace {
	// Button state
	bool lastRawButtonState = HIGH;
	bool debouncedButtonState = HIGH;
	uint32_t lastRawStateChangeTimeMs = 0;
	uint32_t buttonPressStartTimeMs = 0;
	bool longPressDispatched = false;
}

// Setup button
void setupButton() {
	pinMode(config::BUTTON_PIN, INPUT_PULLUP);
	lastRawButtonState = digitalRead(config::BUTTON_PIN);
	debouncedButtonState = lastRawButtonState;
	lastRawStateChangeTimeMs = millis();
}

// Poll the button and emit a semantic event
ButtonEvent pollButtonEvent() {
	const uint32_t currentTimeMs = millis();
	const bool rawButtonState = digitalRead(config::BUTTON_PIN);

	// Track raw pin changes so debounce timing starts at the first edge
	if (rawButtonState != lastRawButtonState) {
		lastRawButtonState = rawButtonState;
		lastRawStateChangeTimeMs = currentTimeMs;
	}

	// Promote the raw state to a debounced state once it has been stable long enough
	if (debouncedButtonState != lastRawButtonState && currentTimeMs - lastRawStateChangeTimeMs >= config::BUTTON_DEBOUNCE_DELAY_MS) {
		debouncedButtonState = lastRawButtonState;

		if (debouncedButtonState == LOW) {
			// Button press started
			buttonPressStartTimeMs = currentTimeMs;
			longPressDispatched = false;
		} else {
			// Button released after a short press
			const bool shouldDispatchShortPress = !longPressDispatched && buttonPressStartTimeMs != 0;
			buttonPressStartTimeMs = 0;

			if (shouldDispatchShortPress) {
				return ButtonEvent::ShortPress;
			}
		}
	}

	// Emit the long-press event once the button has been held long enough
	if (debouncedButtonState == LOW && !longPressDispatched && buttonPressStartTimeMs != 0 && currentTimeMs - buttonPressStartTimeMs >= config::BUTTON_LONG_PRESS_DELAY_MS) {
		longPressDispatched = true;
		return ButtonEvent::LongPress;
	}

	// No new button event
	return ButtonEvent::None;
}