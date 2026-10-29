#ifndef SERIAL_H
#define SERIAL_H

#include <Arduino.h>
#include <gizmo/log.h>
#include "config/config.h"

// Init serial (logging is on in dev mode only)
void initSerial();

// Logging comes from gizmo-common
using gizmo::printLog;
using gizmo::printLogln;
using gizmo::printLogf;
using gizmo::printLogfln;

#endif