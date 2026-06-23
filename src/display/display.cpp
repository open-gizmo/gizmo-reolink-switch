#include "display.h"

#include <cstring>
#include <Wire.h>
#include <hd44780.h>
#include <hd44780ioClass/hd44780_I2Cexp.h>

#include "config/config.h"

namespace {
	hd44780_I2Cexp display;
	bool displayReady = false;

	constexpr uint8_t MAX_DISPLAY_LINE_COUNT = 4;
	constexpr uint8_t MAX_DISPLAY_LINE_LENGTH = 20;

	char displayLines[MAX_DISPLAY_LINE_COUNT][MAX_DISPLAY_LINE_LENGTH + 1] = {{0}};
	uint8_t currentLineIndex = 0;
	uint8_t currentColumnIndex = 0;
	uint8_t activeDisplayRows = 0;
	uint8_t activeDisplayColumns = 0;

	// Clear one line in the internal buffer
	void clearLine(uint8_t lineIndex) {
		memset(displayLines[lineIndex], ' ', activeDisplayColumns);
		displayLines[lineIndex][activeDisplayColumns] = '\0';
	}

	// Fill one display line with centered text clipped to the active LCD width
	void setDisplayLine(uint8_t lineIndex, const char *text) {
		// If the line index is out of bounds, do nothing
		if (lineIndex >= activeDisplayRows) {
			return;
		}

		// Reset the full line to padded spaces before writing new text
		clearLine(lineIndex);

		// If the text is null or the display has no columns, do nothing
		if (text == nullptr || activeDisplayColumns == 0) {
			return;
		}

		// Calculate the visible length of the text and the starting column for centering
		const size_t textLength = strlen(text);
		uint8_t visibleLength = textLength < activeDisplayColumns ? static_cast<uint8_t>(textLength) : activeDisplayColumns;

		// Keep displayed messages at an even width when there is room to pad them.
		if (visibleLength < activeDisplayColumns && (visibleLength % 2) != 0) {
			visibleLength += 1;
		}

		const uint8_t startColumn = visibleLength < activeDisplayColumns ? static_cast<uint8_t>((activeDisplayColumns - visibleLength) / 2) : 0;

		// Copy the visible portion of the text into the display line buffer
		for (uint8_t index = 0; index < visibleLength; index += 1) {
			displayLines[lineIndex][startColumn + index] = text[index];
		}
	}

	// Draw the current line buffer to the physical display
	void renderDisplay() {
		// If the display is not ready, do not attempt to render
		if (!displayReady) {
			return;
		}

		// Clear the physical display before rendering the new content
		display.clear();

		// Iterate through each active display row and write the corresponding line buffer to the display
		for (uint8_t index = 0; index < activeDisplayRows; index += 1) {
			display.setCursor(0, index);
			display.print(displayLines[index]);
		}
	}

	// Move to the next line, scrolling the buffer when needed
	void advanceLine() {
		// If the current line index is less than the last active row, simply move to the next line
		if (currentLineIndex < activeDisplayRows - 1) {
			currentLineIndex += 1;
		} else {
			// If the current line index is at the last active row, scroll the buffer up by one line
			for (uint8_t index = 0; index < activeDisplayRows - 1; index += 1) {
				strncpy(displayLines[index], displayLines[index + 1], MAX_DISPLAY_LINE_LENGTH + 1);
			}

			// Clear the last line after scrolling
			clearLine(activeDisplayRows - 1);
		}

		// Reset the column index to the start of the new line
		currentColumnIndex = 0;
		clearLine(currentLineIndex);
	}

	// Append one character to the current line, wrapping when needed
	void appendCharacter(char character) {
		// If the character is a newline, advance to the next line
		if (character == '\n') {
			advanceLine();
			return;
		}

		// If the current column index exceeds the active display columns, advance to the next line
		if (currentColumnIndex >= activeDisplayColumns) {
			advanceLine();
		}

		// Append the character to the current line buffer and null-terminate it
		displayLines[currentLineIndex][currentColumnIndex] = character;
		currentColumnIndex += 1;
		displayLines[currentLineIndex][currentColumnIndex] = '\0';
	}

	// Append a null-terminated string to the current line buffer
	void appendText(const char *message) {
		// If the message is null, do nothing
		if (message == nullptr) {
			return;
		}

		// Iterate through each character in the message and append it to the current line buffer
		while (*message != '\0') {
			appendCharacter(*message);
			message += 1;
		}
	}
}

// Init the I2C display module
void initDisplay() {
	// Initialize the I2C bus with the specified SDA and SCL pins
	Wire.begin(config::DISPLAY_SDA_PIN, config::DISPLAY_SCL_PIN);
	Wire.setClock(100000);
	displayReady = false;
	activeDisplayColumns = config::DISPLAY_COLUMNS <= MAX_DISPLAY_LINE_LENGTH ? config::DISPLAY_COLUMNS : MAX_DISPLAY_LINE_LENGTH;
	activeDisplayRows = config::DISPLAY_ROWS <= MAX_DISPLAY_LINE_COUNT ? config::DISPLAY_ROWS : MAX_DISPLAY_LINE_COUNT;

	// If the display has no active columns or rows, do not attempt to initialize it
	if (activeDisplayColumns == 0 || activeDisplayRows == 0) {
		return;
	}

	// Initialize the display with the specified number of columns and rows
	const int initStatus = display.begin(activeDisplayColumns, activeDisplayRows);

	// If the display initialization failed, do not proceed with further setup
	if (initStatus != 0) {
		return;
	}

	// Turn on the backlight and clear the display to prepare it for use
	display.backlight();
	display.clear();
	displayReady = true;
	clearDisplayMessages();
	renderDisplay();
}

// Check whether the display is ready to accept writes
bool isDisplayReady() {
	return displayReady;
}

// Clear all text currently shown on the display
void clearDisplayMessages() {
	// Clear each line in the internal buffer
	for (uint8_t index = 0; index < activeDisplayRows; index += 1) {
		clearLine(index);
	}

	// Reset the current line and column indices to the start of the display
	currentLineIndex = 0;
	currentColumnIndex = 0;
	renderDisplay();
}

// Show a clean two-line screen optimized for the LCD
void displayShowScreen(const char *line1, const char *line2) {
	// If the display is not ready, do not attempt to show the screen
	if (!displayReady) {
		return;
	}

	// Clear the display and set the specified lines in the internal buffer
	clearDisplayMessages();
	setDisplayLine(0, line1);

	// If the display has more than one row, set the second line in the internal buffer
	if (activeDisplayRows > 1) {
		setDisplayLine(1, line2);
	}

	// Render the updated internal buffer to the physical display
	renderDisplay();
}

// Append text to the current display line buffer
void displayPrint(const char *message) {
	// If the display is not ready, do not attempt to append text
	if (!displayReady) {
		return;
	}

	// Append the specified text to the current line buffer and render the display
	appendText(message);
	renderDisplay();
}

// Append text and terminate the current display line
void displayPrintln(const char *message) {
	// If the display is not ready, do not attempt to append text
	if (!displayReady) {
		return;
	}

	// Append the specified text to the current line buffer, advance to the next line, and render the display
	appendText(message);
	advanceLine();
	renderDisplay();
}