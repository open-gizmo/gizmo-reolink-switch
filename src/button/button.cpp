#include "button.h"

namespace {
	// Button configuration
	const uint8_t buttonPin = 13;
	const uint32_t debounceDelayMs = 50;
	volatile bool buttonInterruptTriggered = false;
	bool buttonReady = true;
	uint32_t lastDebounceTimeMs = 0;

	// Interrupt handler for button press
	void IRAM_ATTR onButtonPress() {
		buttonInterruptTriggered = true;
	}
}

// Setup button
void setupButton() {
	pinMode(buttonPin, INPUT_PULLUP);
	attachInterrupt(digitalPinToInterrupt(buttonPin), onButtonPress, FALLING);
}

// Check if button was pressed
bool wasButtonPressed() {
	const uint32_t currentTimeMs = millis();

	// Check if button is ready and debounce time has passed
	if (!buttonReady && digitalRead(buttonPin) == HIGH && currentTimeMs - lastDebounceTimeMs >= debounceDelayMs) {
		buttonReady = true;
	}

	// Check if interrupt was triggered
	noInterrupts();
	const bool interruptTriggered = buttonInterruptTriggered;
	buttonInterruptTriggered = false;
	interrupts();

	// If interrupt was triggered, update debounce time
	if (interruptTriggered) {
		lastDebounceTimeMs = currentTimeMs;
	}

	// Check if button is ready and currently pressed
	if (buttonReady && digitalRead(buttonPin) == LOW && currentTimeMs - lastDebounceTimeMs >= debounceDelayMs) {
		buttonReady = false;
		return true;
	}

	// Button not pressed
	return false;
}