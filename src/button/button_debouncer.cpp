#include "config/config.h"
#include "button_debouncer.h"

// Start from the current button level, with no press in progress (also drops a press made while the firmware was busy, even if still held)
void resetButtonDebouncer(ButtonDebouncer &debouncer, bool isPressed, uint32_t currentTimeMs) {
	debouncer.lastRawPressed = isPressed;
	debouncer.debouncedPressed = isPressed;
	debouncer.lastRawChangeTimeMs = currentTimeMs;
	debouncer.pressStartTimeMs = 0;
	debouncer.isPressTracked = false;
	debouncer.longPressDispatched = false;
}

// Feed the current button level and emit a short or long press event
ButtonEvent updateButtonDebouncer(ButtonDebouncer &debouncer, bool isPressed, uint32_t currentTimeMs) {
	// Track raw level changes so debounce timing starts at the first edge
	if (isPressed != debouncer.lastRawPressed) {
		debouncer.lastRawPressed = isPressed;
		debouncer.lastRawChangeTimeMs = currentTimeMs;
	}

	// Promote the raw level to the debounced level once it has been stable long enough
	if (debouncer.debouncedPressed != debouncer.lastRawPressed && currentTimeMs - debouncer.lastRawChangeTimeMs >= config::BUTTON_DEBOUNCE_DELAY_MS) {
		debouncer.debouncedPressed = debouncer.lastRawPressed;

		// Button press started
		if (debouncer.debouncedPressed) {
			debouncer.pressStartTimeMs = currentTimeMs;
			debouncer.isPressTracked = true;
			debouncer.longPressDispatched = false;
			return ButtonEvent::None;
		}

		// Button released: a short press, unless it was already a long press or an ignored one
		const bool shouldDispatchShortPress = debouncer.isPressTracked && !debouncer.longPressDispatched;
		debouncer.isPressTracked = false;
		if (shouldDispatchShortPress) {
			return ButtonEvent::ShortPress;
		}
	}

	// Emit the long press once the button has been held long enough
	if (debouncer.debouncedPressed && debouncer.isPressTracked && !debouncer.longPressDispatched && currentTimeMs - debouncer.pressStartTimeMs >= config::BUTTON_LONG_PRESS_DELAY_MS) {
		debouncer.longPressDispatched = true;
		return ButtonEvent::LongPress;
	}

	// No new button event
	return ButtonEvent::None;
}