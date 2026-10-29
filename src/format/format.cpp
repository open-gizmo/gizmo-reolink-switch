#include "format.h"

// Format seconds as MM:SS for the LCD (minutes grow past two digits instead of being cut off)
void formatDuration(uint32_t totalSeconds, char *buffer, size_t bufferSize) {
	const uint32_t minutes = totalSeconds / 60;
	const uint32_t seconds = totalSeconds % 60;
	snprintf(buffer, bufferSize, "%02lu:%02lu", static_cast<unsigned long>(minutes), static_cast<unsigned long>(seconds));
}