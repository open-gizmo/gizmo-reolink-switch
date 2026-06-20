#include "button/button.h"
#include "buzzer/buzzer.h"
#include "config/config.h"
#include "menu/menu.h"
#include "serial/serial.h"

namespace {
	// Print the current timer state to the serial console
	void printMenuStatus() {
		printLogf("%s\n", getMenuDisplayText());
	}

	// React to timer menu events with logging and buzzer feedback
	void handleMenuEvent(MenuEvent event) {
		switch (event) {
			// Handle a selection change
			case MenuEvent::SelectionChanged:
				printMenuStatus();
				singleBuzz(config::MENU_SELECTION_BUZZ_DURATION_MS);
				break;

			// Handle the start of a countdown timer
			case MenuEvent::TimerStarted:
				printLogln("Timer started");
				printMenuStatus();
				singleBuzz(config::TIMER_START_BUZZ_DURATION_MS);
				break;

			// Handle a countdown timer tick
			case MenuEvent::TimerTick:
				printMenuStatus();

				// Provide a short buzz when the timer is about to complete
				if (getRemainingSeconds() == 2 || getRemainingSeconds() == 1) {
					singleBuzz(config::TIMER_WARNING_BUZZ_DURATION_MS);
				}

				break;

			// Handle the completion of a countdown timer
			case MenuEvent::TimerCompleted:
				printLogln("Timer completed. Returning to idle state.");
				printMenuStatus();
				singleBuzz(config::TIMER_FINISH_BUZZ_DURATION_MS);
				break;

			// Handle the cancellation of a countdown timer
			case MenuEvent::TimerCancelled:
				printLogln("Timer cancelled. Returning to idle state.");
				printMenuStatus();
				singleBuzz(config::TIMER_FINISH_BUZZ_DURATION_MS);
				break;

			// Handle no event
			case MenuEvent::None:
			default:
				break;
		}
	}

	// Handle a button event by performing the corresponding action
	void handleButtonEvent(ButtonEvent event) {
		switch (event) {
			// Handle a short button press
			case ButtonEvent::ShortPress:
				handleMenuEvent(increaseSelectedDuration());
				break;

			// Handle a long button press
			case ButtonEvent::LongPress:
				handleMenuEvent(activateOrCancelTimer());
				break;

			// Handle no event
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
	printLogln("Short press: add 1 minute below 5 minutes, otherwise add 5 minutes");
	printLogln("Long press: start or cancel timer");
	printMenuStatus();
}

// Loop
void loop() {
	// Poll the button for events
	const ButtonEvent buttonEvent = pollButtonEvent();

	// Handle any button event
	if (buttonEvent != ButtonEvent::None) {
		handleButtonEvent(buttonEvent);
	}

	// Update the menu state and handle any events
	handleMenuEvent(updateMenu());
}