#ifndef SERIAL_H
#define SERIAL_H

#include <Arduino.h>
#include "config/config.h"

// Init serial
void initSerial();

// Print log
template <typename T>
inline void printLog(T message) {
	if (config::DEVMODE)
		Serial.print(message);
}

// Print log with newline
template <typename T>
inline void printLogln(T message) {
	if (config::DEVMODE)
		Serial.println(message);
}

// Printf-style debug print with format string
template <typename... Args>
inline void printLogf(const char *format, Args... args) {
	if (config::DEVMODE) {
		static char buffer[256];
		snprintf(buffer, sizeof(buffer), format, args...);
		Serial.print(buffer);
	}
}

// Printf-style debug print with format string and newline
template <typename... Args>
inline void printLogfln(const char *format, Args... args) {
	if (config::DEVMODE) {
		static char buffer[256];
		snprintf(buffer, sizeof(buffer), format, args...);
		Serial.println(buffer);
	}
}

#endif