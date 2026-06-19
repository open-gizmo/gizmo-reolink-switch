#ifndef MENU_H
#define MENU_H

#include <Arduino.h>

enum class MenuItem : uint8_t {
	Mute15Minutes,
	Mute30Minutes,
	Mute60Minutes,
	CancelMute,
};

void setupMenu();
void moveToNextMenuItem();
MenuItem getCurrentMenuItem();
const char *getCurrentMenuItemLabel();

#endif