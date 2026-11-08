#include "remote_control.h"

#include <Arduino.h>

namespace {
	// The waiting command and the published state (shared by the main loop and the web server task, both small enough for a spinlock)
	RemoteCommand pendingCommand = { RemoteCommandType::None };
	SwitchStatus publishedStatus = { false, 0, 0, false, true, NvrStatus::Unknown, 0 };
	portMUX_TYPE remoteMux = portMUX_INITIALIZER_UNLOCKED;
}

// Hand a command from the web app to the main loop (safe from any task; false if one is already waiting)
bool submitRemoteCommand(const RemoteCommand &command) {
	portENTER_CRITICAL(&remoteMux);
	const bool isAccepted = pendingCommand.type == RemoteCommandType::None;
	if (isAccepted) {
		pendingCommand = command;
	}
	portEXIT_CRITICAL(&remoteMux);
	return isAccepted;
}

// Take the waiting command, if any (called by the main loop; type None when there is none)
RemoteCommand takeRemoteCommand() {
	portENTER_CRITICAL(&remoteMux);
	const RemoteCommand command = pendingCommand;
	pendingCommand = { RemoteCommandType::None };
	portEXIT_CRITICAL(&remoteMux);
	return command;
}

// Check whether a command is waiting for the main loop
bool hasPendingRemoteCommand() {
	portENTER_CRITICAL(&remoteMux);
	const bool isPending = pendingCommand.type != RemoteCommandType::None;
	portEXIT_CRITICAL(&remoteMux);
	return isPending;
}

// Publish what the switch is doing (called by the main loop)
void publishSwitchStatus(const SwitchStatus &status) {
	portENTER_CRITICAL(&remoteMux);
	publishedStatus = status;
	portEXIT_CRITICAL(&remoteMux);
}

// Get what the switch is doing (safe from any task)
SwitchStatus getSwitchStatus() {
	portENTER_CRITICAL(&remoteMux);
	const SwitchStatus status = publishedStatus;
	portEXIT_CRITICAL(&remoteMux);
	return status;
}