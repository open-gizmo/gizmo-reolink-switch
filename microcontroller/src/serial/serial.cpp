#include "serial.h"

// Init serial (logging is on in dev mode only)
void initSerial() {
	gizmo::initLog(); // Off in the released firmware, on in the debug build (pio run -e debug)
}