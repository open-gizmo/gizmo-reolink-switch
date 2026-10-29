#ifndef FORMAT_H
#define FORMAT_H

#include <Arduino.h>

// Buffer size that fits any duration as MM:SS (up to "71582788:15" for UINT32_MAX seconds, plus the terminator)
constexpr size_t DURATION_TEXT_SIZE = 12;

// Format seconds as MM:SS for the LCD (minutes grow past two digits instead of being cut off)
void formatDuration(uint32_t totalSeconds, char *buffer, size_t bufferSize);

#endif