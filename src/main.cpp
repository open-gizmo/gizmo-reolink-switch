#include "button/button.h"
#include "buzzer/buzzer.h"
#include "serial/serial.h"

// Setup
void setup() {
	// Init serial
	initSerial();
	printLogln("Serial initialized!");

	// Init buzzer
	setupBuzzer();
	printLogln("Buzzer initialized!");

	// Init button
	setupButton();
	printLogln("Button initialized on GPIO13 with interrupt!");
}

// Loop
void loop() {
	if (wasButtonPressed()) {
		printLogln("Button pressed, buzzing for 100ms");
		singleBuzz(100);
		delay(20);
	}
}