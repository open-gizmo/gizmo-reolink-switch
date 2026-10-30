#ifndef BUTTON_H
#define BUTTON_H

#include <Arduino.h>

#include "button_debouncer.h"

void setupButton();
ButtonEvent pollButtonEvent();
void ignoreButtonUntilReleased();

#endif