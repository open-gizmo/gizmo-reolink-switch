#include "buzzer.h"

namespace {
const uint8_t buzzerPin = 12;
}

// Setup buzzer
void setupBuzzer() {
    pinMode(buzzerPin, OUTPUT);
    digitalWrite(buzzerPin, LOW);
}

// Single buzz
void singleBuzz(uint32_t durationMs) {
    digitalWrite(buzzerPin, HIGH);
    delay(durationMs);
    digitalWrite(buzzerPin, LOW);
}