#ifndef REMOTE_TYPES_H
#define REMOTE_TYPES_H

#include <stdint.h>

// Commands the web app sends to the main loop (the loop runs them, since only it talks to the NVR); the web app sets the device up, and never controls the cameras: the quiet mode only starts, stops and restores from the button
enum class RemoteCommandType : uint8_t {
	None,
	TestNvr,
};

// A command from the web app
struct RemoteCommand {
	RemoteCommandType type;
};

// Last known state of the NVR connection (the values are part of the web API)
enum class NvrStatus : uint8_t {
	NotConfigured,
	Unknown, // Not checked since the boot or the last login change
	Connected,
	Unreachable, // No answer, or no Wi-Fi
	LoginRefused, // Wrong username or password
	LoginPaused, // Waiting after a refused login, so the NVR doesn't lock the account
};

// What the switch is doing, published by the main loop for the web app
struct SwitchStatus {
	bool isTimerRunning;
	uint32_t remainingSeconds;
	uint32_t selectedSeconds;
	bool hasPendingRestore; // Alerts still off after a failed or partial restore
	bool isBusy; // The loop is running a command or talking to the NVR
	NvrStatus nvrStatus;
	uint8_t cameraCount; // Online cameras at the last check
};

#endif