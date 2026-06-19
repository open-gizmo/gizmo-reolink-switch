#include "serial.h"

const bool serialDebug = true;

// Init serial
void initSerial() {
    if (serialDebug) {
        Serial.begin(9600);
        while (!Serial) {
            delay(10);
        }
    }
}