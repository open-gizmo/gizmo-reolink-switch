#include "button/button.h"
#include "buzzer/buzzer.h"
#include "menu/menu.h"
#include "serial/serial.h"

namespace {
	// Print the current menu item label to the serial console
	void printCurrentMenuItem() {
		printLogf("Current menu item: %s\n", getCurrentMenuItemLabel());
	}

	// Handle the selected menu item action
	void handleMenuSelection() {
		printLogf("Selected menu item: %s\n", getCurrentMenuItemLabel());
		singleBuzz(120);
		delay(60);
		singleBuzz(120);
	}

	// Handle a button event by performing the corresponding action
	void handleButtonEvent(ButtonEvent event) {
		switch (event) {
			case ButtonEvent::ShortPress:
				moveToNextMenuItem();
				printCurrentMenuItem();
				singleBuzz(40);
				break;

			case ButtonEvent::LongPress:
				handleMenuSelection();
				break;

			case ButtonEvent::None:
			default:
				break;
		}
	}
}

// Setup
void setup() {
	// Init serial
	initSerial();
	printLogln("Serial initialized!");

	// Init buzzer
	setupBuzzer();
	printLogln("Buzzer initialized!");

	// Init button
	setupButton();
	printLogln("Button initialized!");

	// Init menu
	setupMenu();
	printLogln("Menu initialized!");
	printLogln("Short press: next item");
	printLogln("Long press: select item");
	printCurrentMenuItem();
}

// Loop
void loop() {
	const ButtonEvent buttonEvent = pollButtonEvent();

	// Handle any button event
	if (buttonEvent != ButtonEvent::None) {
		handleButtonEvent(buttonEvent);
	}
}