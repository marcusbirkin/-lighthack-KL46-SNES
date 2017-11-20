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
	uint16_t button_index = 0;
	uint16_t button_data = 0;

	// Latch data and read first button
	SNES_LAT(1);
	SNES_WAIT();
	SNES_LAT(0);
	button_data |= SNES_READ_DATA(button_index);

	// Clock data in
	for (; button_index < button_count; button_index++) {
		SNES_CLK(0);
		SNES_WAIT();
		button_data |= SNES_READ_DATA(button_index);
		SNES_CLK(1);
		SNES_WAIT();
	}

	// Save state
	for (int n = 0; n < SNES_BUTTON_COUNT; n++) {
		if (button_data & 1U << n) {
			switch (SNES_ButtonStatus[n].State) {
			case SNES_BUTTONSTATE_DOWN:
				// Not new, but maybe repeating now?
				if ( SNES_ButtonStatus[n].Tick + SNES_FirstRepeatRateTick < xTaskGetTickCount()) {
					SNES_ButtonStatus[n].State = SNES_BUTTONSTATE_DOWN_REPEAT;
					SNES_ButtonStatus[n].Tick = xTaskGetTickCount();

					PRINTF("SNES Repeat: %s\r\n", SNES_ButtonString[n]);
				}
				break;
			case SNES_BUTTONSTATE_DOWN_REPEAT:
				// Still repeating
				if ( SNES_ButtonStatus[n].Tick + SNES_ContinuousRepeatRateTick < xTaskGetTickCount()) {
					SNES_ButtonStatus[n].Tick = xTaskGetTickCount();

					PRINTF("SNES Repeat: %s\r\n", SNES_ButtonString[n]);
				}
				break;
			default:
			case SNES_BUTTONSTATE_UP:
				// New button down!
				SNES_ButtonStatus[n].State = SNES_BUTTONSTATE_DOWN;
				SNES_ButtonStatus[n].Tick = xTaskGetTickCount();

				PRINTF("SNES Down: %s\r\n", SNES_ButtonString[n]);
				break;
			}
		} else  {
			if (SNES_ButtonStatus[n].State != SNES_BUTTONSTATE_UP) {
				// New Up
				SNES_ButtonStatus[n].State = SNES_BUTTONSTATE_UP;
				SNES_ButtonStatus[n].Tick = xTaskGetTickCount();

				PRINTF("SNES Up: %s\r\n", SNES_ButtonString[n]);
			}
		}
	}
}

