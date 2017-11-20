/*
 * snes.h
 *
 *  Created on: 17 Nov 2017
 *      Author: mbirkin
 */

#ifndef SOURCE_SNES_H_
#define SOURCE_SNES_H_

#define SNES_FirstRepeatRateTick 1000 / portTICK_PERIOD_MS // 1s
#define SNES_ContinuousRepeatRateTick 100 / portTICK_PERIOD_MS // 100ms

typedef enum {
	SNES_BUTTON_B,
	SNES_BUTTON_Y,
	SNES_BUTTON_SELECT,
	SNES_BUTTON_START,
	SNES_BUTTON_UP,
	SNES_BUTTON_DOWN,
	SNES_BUTTON_LEFT,
	SNES_BUTTON_RIGHT,
	SNES_BUTTON_A,
	SNES_BUTTON_X,
	SNES_BUTTON_L,
	SNES_BUTTON_R,
	SNES_BUTTON_COUNT
} SNES_Buttons;

const static char *SNES_ButtonString[SNES_BUTTON_COUNT] = {
		"SNES_B",
		"SNES_Y",
		"SNES_SELECT",
		"SNES_START",
		"SNES_UP",
		"SNES_DOWN",
		"SNES_LEFT",
		"SNES_RIGHT",
		"SNES_A",
		"SNES_X",
		"SNES_L",
		"SNES_R"};

typedef enum {
	SNES_BUTTONSTATE_UP,
	SNES_BUTTONSTATE_DOWN,
	SNES_BUTTONSTATE_DOWN_REPEAT
} SNES_ButtonsState;
struct  SNES_ButtonsStatus_t {
	SNES_ButtonsState State;
	TickType_t Tick;
};
struct SNES_ButtonsStatus_t SNES_ButtonStatus[SNES_BUTTON_COUNT];

/* "Public" Functions */
void SNES_Init();
void SNES_ReadButtons();


#endif /* SOURCE_SNES_H_ */
