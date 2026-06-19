#include "menu.h"

namespace {
	// Static menu items
	constexpr MenuItem menuItems[] = {
	    MenuItem::Mute15Minutes,
	    MenuItem::Mute30Minutes,
	    MenuItem::Mute60Minutes,
	    MenuItem::CancelMute,
	};

	// Current menu cursor
	uint8_t currentMenuIndex = 0;
}

// Setup menu
void setupMenu() {
	currentMenuIndex = 0;
}

// Move to the next menu item
void moveToNextMenuItem() {
	currentMenuIndex = (currentMenuIndex + 1) % (sizeof(menuItems) / sizeof(menuItems[0]));
}

// Get current menu item
MenuItem getCurrentMenuItem() {
	return menuItems[currentMenuIndex];
}

// Get current menu item label
const char *getCurrentMenuItemLabel() {
	switch (getCurrentMenuItem()) {
		case MenuItem::Mute15Minutes:
			return "Mute for 15 minutes";
		case MenuItem::Mute30Minutes:
			return "Mute for 30 minutes";
		case MenuItem::Mute60Minutes:
			return "Mute for 60 minutes";
		case MenuItem::CancelMute:
			return "Cancel mute";
	}

	return "Unknown menu item";
}