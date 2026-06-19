#ifndef MENU_H
#define MENU_H

#include <Arduino.h>

enum class MenuEvent : uint8_t {
	None,
	SelectionChanged,
	TimerStarted,
	TimerTick,
	TimerCompleted,
	TimerCancelled,
};

void setupMenu();
MenuEvent increaseSelectedDuration();
MenuEvent activateOrCancelTimer();
MenuEvent updateMenu();
bool isTimerRunning();
uint32_t getRemainingSeconds();
const char *getMenuDisplayText();

#endif