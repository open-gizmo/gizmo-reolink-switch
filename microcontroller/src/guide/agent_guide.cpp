#include "agent_guide.h"
#include <string.h>
#include <strings.h>

namespace {
	// Placeholder the device address replaces
	constexpr const char *HOST_PLACEHOLDER = "{host}";

	// The guide, with the address as a placeholder (kept in flash)
	constexpr const char *AGENT_GUIDE = R"(# Gizmo Reolink Switch

A button on the local network that mutes a Reolink NVR for a while: when its owner leaves home, a press turns off the notifications and the recording of the cameras for the time they chose, and the switch turns them back on when the timer ends. A 16x2 LCD and a buzzer show what it is doing.

## HTTP API

The switch answers plain HTTP requests on the local network, to read its state:

- `GET http://{host}/api/status` returns the state of the switch as JSON: the timer, whether the alerts are still off, the NVR connection and the online cameras.
- `GET http://{host}/api/nvr` returns the NVR the switch talks to: its address and username, never the password.

```sh
curl "http://{host}/api/status"
```

## Tips

- The quiet mode is started and stopped with the button only, on purpose: no request can turn the cameras off or on.
- `timerRunning` is `true` while the cameras are muted; `remainingSeconds` is the time left, and `selectedSeconds` the time chosen with the button.
- `alertsStillOff` is `true` when the timer ended but the switch could not turn the alerts back on yet: it keeps retrying, and the owner should check the NVR.
- `nvrStatus` is `connected`, `unreachable`, `login_refused`, `login_paused` (after refused logins, the switch waits before trying again), `not_configured` or `unknown`; `cameraCount` counts the cameras online.
- `busy` is `true` while the switch is talking to the NVR.
- Errors of the API are JSON: `{ "errorId": "...", "errorMessage": "...", "data": null }`.
- The switch only answers on its local network, and without authentication: anyone on that network can read it.
)";

	// Find the position of a media type in an Accept header, ignoring the case (-1 if it isn't there)
	int findMediaType(const char *accept, const char *mediaType) {
		const size_t acceptLength = strlen(accept);
		const size_t mediaTypeLength = strlen(mediaType);
		for (size_t position = 0; position + mediaTypeLength <= acceptLength; position++) {
			if (strncasecmp(accept + position, mediaType, mediaTypeLength) == 0) {
				return static_cast<int>(position);
			}
		}
		return -1;
	}
}

// Write the guide piece by piece, with the address the agent used
void writeAgentGuide(const char *host, AgentGuideWriter write, void *context) {
	const size_t placeholderLength = strlen(HOST_PLACEHOLDER);
	const char *remaining = AGENT_GUIDE;

	// Write the text between placeholders, and the address in their place
	const char *placeholder = strstr(remaining, HOST_PLACEHOLDER);
	while (placeholder != nullptr) {
		write(remaining, static_cast<size_t>(placeholder - remaining), context);
		write(host, strlen(host), context);
		remaining = placeholder + placeholderLength;
		placeholder = strstr(remaining, HOST_PLACEHOLDER);
	}

	// Write the end of the guide
	write(remaining, strlen(remaining), context);
}

// Check whether a client prefers Markdown to HTML from its Accept header
bool isMarkdownPreferred(const char *accept) {
	// No header: a plain request, which gets the web app
	if (accept == nullptr) {
		return false;
	}

	// Markdown must be listed, and come before HTML when both are (the quality values are not weighed)
	const int markdownPosition = findMediaType(accept, "text/markdown");
	const int htmlPosition = findMediaType(accept, "text/html");
	return markdownPosition >= 0 && (htmlPosition < 0 || markdownPosition < htmlPosition);
}