#include "button.h"

namespace {
	// Button configuration
	const uint8_t buttonPin = 13;
	const uint32_t debounceDelayMs = 40;
	const uint32_t longPressDelayMs = 700;

	// Button state
	bool lastRawButtonState = HIGH;
	bool debouncedButtonState = HIGH;
	uint32_t lastRawStateChangeTimeMs = 0;
	uint32_t buttonPressStartTimeMs = 0;
	bool longPressDispatched = false;
}

// Setup button
void setupButton() {
	pinMode(buttonPin, INPUT_PULLUP);
	lastRawButtonState = digitalRead(buttonPin);
	debouncedButtonState = lastRawButtonState;
	lastRawStateChangeTimeMs = millis();
}

// Poll the button and emit a semantic event
ButtonEvent pollButtonEvent() {
	const uint32_t currentTimeMs = millis();
	const bool rawButtonState = digitalRead(buttonPin);

	// Track raw pin changes so debounce timing starts at the first edge
	if (rawButtonState != lastRawButtonState) {
		lastRawButtonState = rawButtonState;
		lastRawStateChangeTimeMs = currentTimeMs;
	}

	// Promote the raw state to a debounced state once it has been stable long enough
	if (debouncedButtonState != lastRawButtonState && currentTimeMs - lastRawStateChangeTimeMs >= debounceDelayMs) {
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
	if (debouncedButtonState == LOW && !longPressDispatched && buttonPressStartTimeMs != 0 && currentTimeMs - buttonPressStartTimeMs >= longPressDelayMs) {
		longPressDispatched = true;
		return ButtonEvent::LongPress;
	}

	// No new button event
	return ButtonEvent::None;
}