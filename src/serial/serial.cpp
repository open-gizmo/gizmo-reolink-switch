#include "serial.h"

// Init serial
void initSerial() {
	if (config::DEVMODE) {
		Serial.begin(115200);
		while (!Serial) {
			delay(10);
		}
	}
}