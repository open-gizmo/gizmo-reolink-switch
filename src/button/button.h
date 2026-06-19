#ifndef BUTTON_H
#define BUTTON_H

#include <Arduino.h>

enum class ButtonEvent : uint8_t {
	None,
	ShortPress,
	LongPress,
};

void setupButton();
ButtonEvent pollButtonEvent();

#endif