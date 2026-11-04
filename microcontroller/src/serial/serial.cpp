#include "serial.h"

// Init serial (logging is on in dev mode only)
void initSerial() {
	gizmo::initLog(config::DEVMODE);
}