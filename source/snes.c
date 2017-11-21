/*
 * snes.c
 *
 *  Created on: 17 Nov 2017
 *      Author: mbirkin
 */

/* Board includes */
#include "board.h"
#include "pin_mux.h"
#include "clock_config.h"

/* FreeRTOS kernel includes. */
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "timers.h"

/* Freescale driver includes */
#include "fsl_debug_console.h"
#include "fsl_gpio.h"

#include "snes.h"

/* "Private" functions */
static inline void SNES_LAT(bool value) {
	GPIO_PinWrite(BOARD_INITSNES_SNES_LAT_GPIO, BOARD_INITSNES_SNES_LAT_PIN, value);
}
static inline void SNES_CLK(bool value) {
	GPIO_PinWrite(BOARD_INITSNES_SNES_CLK_GPIO, BOARD_INITSNES_SNES_CLK_PIN, value);
}
static inline uint16_t SNES_READ_DATA(uint16_t button_index) {
	return (!GPIO_PinRead(BOARD_INITSNES_SNES_DATA_GPIO, BOARD_INITSNES_SNES_DATA_PIN)) ? 1U << button_index: 0;
}
inline void SNES_WAIT() {
	for(int n = 0; n < 60; n++) { __asm("NOP"); } // About 6us
}

void SNES_Init() {
	/* Setup pins */

	// Clock
	gpio_pin_config_t clk_config =
	{
		BOARD_INITSNES_SNES_LAT_DIRECTION,
	    1, // Idle high
	};
	GPIO_PinInit(
			BOARD_INITSNES_SNES_CLK_GPIO,
			BOARD_INITSNES_SNES_CLK_PIN,
			&clk_config
		);

	// Latch
	gpio_pin_config_t lat_config =
	{
		BOARD_INITSNES_SNES_LAT_DIRECTION,
	    0,
	};
	GPIO_PinInit(
			BOARD_INITSNES_SNES_LAT_GPIO,
			BOARD_INITSNES_SNES_LAT_PIN,
			&lat_config
		); // Latch

	// Data
	gpio_pin_config_t data_config =
	{
		BOARD_INITSNES_SNES_DATA_DIRECTION,
	    0,
	};
	GPIO_PinInit(
			BOARD_INITSNES_SNES_DATA_GPIO,
			BOARD_INITSNES_SNES_DATA_PIN,
			&data_config
		);
}

void SNES_ReadButtons() {
	const uint8_t button_count = 16;
	uint16_t buttonIndex = 0;
	uint16_t buttonData = 0;

	// Latch data and read first button
	SNES_LAT(1);
	SNES_WAIT();
	SNES_LAT(0);
	buttonData |= SNES_READ_DATA(buttonIndex);

	// Clock data in
	for (; buttonIndex < button_count; buttonIndex++) {
		SNES_CLK(0);
		SNES_WAIT();
		buttonData |= SNES_READ_DATA(buttonIndex);
		SNES_CLK(1);
		SNES_WAIT();
	}

	// Save state
	for (buttonIndex = 0; buttonIndex < SNES_BUTTON_COUNT - 1; buttonIndex++) {
		if (buttonData & 1U << buttonIndex) {
			switch (SNES_ButtonStatus[buttonIndex].State) {
			case SNES_BUTTONSTATE_DOWN:
				// Not new, but maybe repeating now?
				if ( SNES_ButtonStatus[buttonIndex].Tick + SNES_FirstRepeatRateTick < xTaskGetTickCount()) {
					SNES_ButtonStatus[buttonIndex].State = SNES_BUTTONSTATE_DOWN_REPEAT;
					SNES_ButtonStatus[buttonIndex].Tick = xTaskGetTickCount();

					PRINTF("SNES Repeat: %s\r\n", SNES_ButtonString[buttonIndex]);
				}
				break;
			case SNES_BUTTONSTATE_DOWN_REPEAT:
				// Still repeating
				if ( SNES_ButtonStatus[buttonIndex].Tick + SNES_ContinuousRepeatRateTick < xTaskGetTickCount()) {
					SNES_ButtonStatus[buttonIndex].Tick = xTaskGetTickCount();

					PRINTF("SNES Repeat: %s\r\n", SNES_ButtonString[buttonIndex]);
				}
				break;
			default:
			case SNES_BUTTONSTATE_UP:
				// New button down!
				SNES_ButtonStatus[buttonIndex].State = SNES_BUTTONSTATE_DOWN;
				SNES_ButtonStatus[buttonIndex].Tick = xTaskGetTickCount();

				if (SNES_KONAMI_ARRAY[SNES_KONAMI_INDEX++] == buttonIndex)
				{
					if (SNES_KONAMI_INDEX == sizeof(SNES_KONAMI_ARRAY)) {
						SNES_KONAMI_INDEX = 0;
						SNES_ButtonStatus[SNES_KONAMI].State = SNES_BUTTONSTATE_DOWN;
						SNES_ButtonStatus[SNES_KONAMI].Tick = xTaskGetTickCount();

						PRINTF("SNES COMBO: %s\r\n", SNES_ButtonString[SNES_KONAMI]);
					}
				} else {
					SNES_KONAMI_INDEX = 0;
					SNES_ButtonStatus[SNES_KONAMI].State = SNES_BUTTONSTATE_UP;
					SNES_ButtonStatus[SNES_KONAMI].Tick = xTaskGetTickCount();
				}

				PRINTF("SNES Down: %s\r\n", SNES_ButtonString[buttonIndex]);
				break;
			}
		} else  {
			if (SNES_ButtonStatus[buttonIndex].State != SNES_BUTTONSTATE_UP) {
				// New Up
				SNES_ButtonStatus[buttonIndex].State = SNES_BUTTONSTATE_UP;
				SNES_ButtonStatus[buttonIndex].Tick = xTaskGetTickCount();

				PRINTF("SNES Up: %s\r\n", SNES_ButtonString[buttonIndex]);
			}
		}
	}
}

