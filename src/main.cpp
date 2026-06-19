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
}

// Loop
void loop() {
	printLogln("Buzzing for 100ms");
	singleBuzz(100);
	delay(900);
}