#include "button.h"

namespace {
	// Button configuration
	const uint8_t buttonPin = 13;
	const uint32_t debounceDelayMs = 40;
	const uint32_t longPressDelayMs = 700;

	// Button state
	volatile bool buttonStateChanged = false;
	bool lastRawButtonState = HIGH;
	bool debouncedButtonState = HIGH;
	uint32_t lastRawStateChangeTimeMs = 0;
	uint32_t buttonPressStartTimeMs = 0;
	bool longPressDispatched = false;

	// Interrupt handler for button state changes
	void IRAM_ATTR onButtonStateChange() {
		buttonStateChanged = true;
	}
}

// Setup button
void setupButton() {
	pinMode(buttonPin, INPUT_PULLUP);
	lastRawButtonState = digitalRead(buttonPin);
	debouncedButtonState = lastRawButtonState;
	attachInterrupt(digitalPinToInterrupt(buttonPin), onButtonStateChange, CHANGE);
}

// Poll the button and emit a semantic event
ButtonEvent pollButtonEvent() {
	const uint32_t currentTimeMs = millis();
	const bool rawButtonState = digitalRead(buttonPin);

	// Safely consume any pending interrupt signal
	noInterrupts();
	const bool hadInterrupt = buttonStateChanged;
	buttonStateChanged = false;
	interrupts();

	// Track raw pin changes so debounce timing starts at the first edge
	if ((hadInterrupt || rawButtonState != lastRawButtonState) && rawButtonState != lastRawButtonState) {
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