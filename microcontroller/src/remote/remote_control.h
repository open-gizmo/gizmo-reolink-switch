#ifndef REMOTE_CONTROL_H
#define REMOTE_CONTROL_H

#include "remote/remote_types.h"

// Hand a command from the web app to the main loop (safe from any task; false if one is already waiting)
bool submitRemoteCommand(const RemoteCommand &command);

// Take the waiting command, if any (called by the main loop; type None when there is none)
RemoteCommand takeRemoteCommand();

// Check whether a command is waiting for the main loop
bool hasPendingRemoteCommand();

// Publish what the switch is doing (called by the main loop)
void publishSwitchStatus(const SwitchStatus &status);

// Get what the switch is doing (safe from any task)
SwitchStatus getSwitchStatus();

#endif