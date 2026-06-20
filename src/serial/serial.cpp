#include "serial.h"

// Init serial
void initSerial() {
	if (config::DEVMODE) {
		Serial.begin(9600);
		while (!Serial) {
			delay(10);
		}
	}
}