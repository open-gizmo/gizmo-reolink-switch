#include "serial.h"

#include <cstdarg>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

namespace {
	// One shared buffer for formatted log lines (a template version used one buffer per call signature)
	char logBuffer[256];

	// Guard the shared buffer, so log lines from different tasks can't mix
	SemaphoreHandle_t logMutex = nullptr;

	// Format a log line into the shared buffer and print it, with or without a newline
	void printFormattedLog(bool addNewline, const char *format, va_list arguments) {
		// Skip everything when logging is off or the mutex is missing
		if (!config::DEVMODE || logMutex == nullptr) return;

		// Format and print the line while holding the buffer
		xSemaphoreTake(logMutex, portMAX_DELAY);
		vsnprintf(logBuffer, sizeof(logBuffer), format, arguments);
		if (addNewline) {
			Serial.println(logBuffer);
		} else {
			Serial.print(logBuffer);
		}
		xSemaphoreGive(logMutex);
	}
}

// Init serial
void initSerial() {
	// Create the log mutex before any task can log
	logMutex = xSemaphoreCreateMutex();

	// Start the serial port in dev mode
	if (config::DEVMODE) {
		Serial.begin(115200);
		while (!Serial) {
			delay(10);
		}
	}
}

// Printf-style debug print with format string
void printLogf(const char *format, ...) {
	va_list arguments;
	va_start(arguments, format);
	printFormattedLog(false, format, arguments);
	va_end(arguments);
}

// Printf-style debug print with format string and newline
void printLogfln(const char *format, ...) {
	va_list arguments;
	va_start(arguments, format);
	printFormattedLog(true, format, arguments);
	va_end(arguments);
}