#ifndef DISPLAY_H
#define DISPLAY_H

#include <Arduino.h>

// Init the I2C display module
void initDisplay();

// Check whether the display is ready to accept writes
bool isDisplayReady();

// Clear all text currently shown on the display
void clearDisplayMessages();

// Show a clean two-line screen optimized for the LCD
void displayShowScreen(const char *line1, const char *line2 = "");

// Append text to the current display line buffer
void displayPrint(const char *message);

// Append text and terminate the current display line
void displayPrintln(const char *message);

// Printf-style display print with format string
template <typename... Args>
inline void displayPrintf(const char *format, Args... args) {
	static char buffer[256];
	snprintf(buffer, sizeof(buffer), format, args...);
	displayPrint(buffer);
}

// Printf-style display print with format string and newline
template <typename... Args>
inline void displayPrintfln(const char *format, Args... args) {
	static char buffer[256];
	snprintf(buffer, sizeof(buffer), format, args...);
	displayPrintln(buffer);
}

#endif