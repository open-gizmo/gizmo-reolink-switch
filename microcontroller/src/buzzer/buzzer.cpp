#include "config/config.h"
#include "buzzer.h"

// Setup buzzer
void setupBuzzer() {
	pinMode(config::BUZZER_PIN, OUTPUT);
	digitalWrite(config::BUZZER_PIN, LOW);
}

// Single buzz
void singleBuzz(uint32_t durationMs) {
	digitalWrite(config::BUZZER_PIN, HIGH);
	delay(durationMs);
	digitalWrite(config::BUZZER_PIN, LOW);
}

// Double buzz (two short beeps with a pause, used as a warning)
void doubleBuzz(uint32_t durationMs) {
	singleBuzz(durationMs);
	delay(durationMs * 2); // Pause between the beeps so they sound distinct
	singleBuzz(durationMs);
}