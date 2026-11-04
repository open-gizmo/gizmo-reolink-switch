#ifndef ARDUINO_STUB_H
#define ARDUINO_STUB_H

// Minimal Arduino.h for the native tests: just the types and the clock the logic modules use

#include <cstdint>
#include <cstdio>
#include <cstring>

// Fake clock controlled by the tests
namespace fakeClock {
	inline uint32_t currentTimeMs = 0;
}

// Return the fake clock time instead of the real uptime
inline uint32_t millis() {
	return fakeClock::currentTimeMs;
}

#endif