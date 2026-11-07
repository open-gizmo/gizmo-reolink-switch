#include "serial.h"

// Init serial (logging is on in dev mode only)
void initSerial() {
	gizmo::initLog(); // On only when the build sets PIO_DEV_MODE=1 (in platformio_secrets.ini)
}