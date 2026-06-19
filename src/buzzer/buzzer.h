#ifndef BUZZER_H
#define BUZZER_H

#include <Arduino.h>

void setupBuzzer();
void singleBuzz(uint32_t durationMs = 1000);

#endif