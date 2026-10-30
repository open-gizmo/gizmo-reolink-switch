#ifndef BUTTON_DEBOUNCER_H
#define BUTTON_DEBOUNCER_H

#include <Arduino.h>

// Button events, as seen by the rest of the firmware
enum class ButtonEvent : uint8_t {
	None,
	ShortPress,
	LongPress,
};

// Debounce and long-press state of one button, kept free of the pin so the native tests can drive it
struct ButtonDebouncer {
	bool lastRawPressed;
	bool debouncedPressed;
	uint32_t lastRawChangeTimeMs;
	uint32_t pressStartTimeMs;
	bool isPressTracked; // A press that can still become a short or long press
	bool longPressDispatched;
};

void resetButtonDebouncer(ButtonDebouncer &debouncer, bool isPressed, uint32_t currentTimeMs);
ButtonEvent updateButtonDebouncer(ButtonDebouncer &debouncer, bool isPressed, uint32_t currentTimeMs);

#endif