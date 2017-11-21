/*
 * snes.h
 *
 *  Created on: 17 Nov 2017
 *      Author: mbirkin
 */

#include <string.h>

/* Board includes */
#include "board.h"
#include "pin_mux.h"
#include "clock_config.h"

/* Freescale driver includes */
#include "fsl_debug_console.h"

/* FreeRTOS kernel includes. */
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "timers.h"

/* Project includes */
#include "snes.h"
#include "vcp.h"
#include "osc.h"
#include "eos.h"

/* Task priorities. */
#define snes_task_PRIORITY (configMAX_PRIORITIES - 1)
#define eos_task_PRIORITY (configMAX_PRIORITIES - 1)

/* Constants */
// SLIP Encodings
static const uint8_t SLIP_END = 0xC0;
static const uint8_t SLIP_ESC = 0xDB;
static const uint8_t SLIP_ESC_END = 0xDC;
static const uint8_t SLIP_ESC_ESC = 0xDD;

/* Variables */
// OSC
unsigned char OSCRXFrame[1024];
uint8_t OSCRXFrameSize = 0;
// SNES
TickType_t savedButtonTick[SNES_BUTTON_COUNT];

/*!
 * @brief Task responsible for reading SNES controller
 */
static void snes_task(void *pvParameters) {
	const TickType_t xRefresh = 50 / portTICK_PERIOD_MS; // 20hz, SNES ran at 60hz (I suspect PAL versions ran at 50hz)

	/* Init */
	LED_GREEN_INIT(0);
	SNES_Init();

	for (;;) {
		LED_GREEN_TOGGLE();

		// Read buttons
		SNES_ReadButtons();

		vTaskDelay(xRefresh);
	}
}

/*!
 * @brief SLIP encode buffer and send to EOS via VCP
 */
void slip_send(const unsigned char *buffer, const uint32_t size) {
	/* Copy to output buffer */
	memcpy(s_currSendBuf, buffer, size);
	s_sendSize = size;

	/* Terminate correctly */
	s_currSendBuf[s_sendSize++] = SLIP_END; // Terminate with SLIP END

	PRINTF("Sent: ");
	for (int n = 0; n < s_sendSize - 1; n++) {
		PRINTF("%c", s_currSendBuf[n]);
	}
	PRINTF("\r\n", s_currSendBuf);

	/* Send */
	usb_status_t error =
		USB_DeviceCdcAcmSend(s_cdcVcom.cdcAcmHandle, USB_CDC_VCOM_BULK_IN_ENDPOINT, s_currSendBuf, s_sendSize);
	s_sendSize = 0;
	if (error != kStatus_USB_Success)
	{
		PRINTF("USB error code: %d\r\n", error);
	}

	vTaskDelay(10 / portTICK_PERIOD_MS);
}

void rxOSC() {
	// RX Active!
	LED_RED_ON();

    int32_t i;

    /* Check for SLIP end of frame */
    for (i = 0; i < s_recvSize; i++)
    {
    	OSCRXFrame[OSCRXFrameSize++] = s_currRecvBuf[i];
        if (s_currRecvBuf[i] == SLIP_END) {
        	OSCRXFrameSize--;
        	/* Process frame */

        	// Processing start
        	LED_RED_ON();

        	// Debug buffer
//			if (OSCRXFrameSize > 1) {
//				PRINTF("Received: ");
//				for (int n = 0; n < OSCRXFrameSize; n++) {
//					PRINTF("%c", OSCRXFrame[n]);
//				}
//				PRINTF("\r\n");
//			}

        	// Check if OSC
        	if (OSC_SetBuffer(OSCRXFrame, OSCRXFrameSize) == false) {
				// EOS handshake challenge
				if (memcmp(OSCRXFrame, EOS_Handshake_Challenge, strlen((char*)EOS_Handshake_Response)) == 0) {
					PRINTF("EOS: Handshake challenge, sending response...");
					slip_send(EOS_Handshake_Response, sizeof(EOS_Handshake_Response) - 1);
					EOS_doneHandshake = true;
				}
				} else {
					// From EOS OSC?
					if (
							OSC_MatchAddr(EOS_OSC, strlen((char*)EOS_OSC), 0) &&
							OSC_MatchAddr(EOS_OSC_OUT, strlen((char*)EOS_OSC_OUT), 1)
						)
					{

						// Pong
						if (OSC_MatchAddr(EOS_OSC_PING, strlen((char*)EOS_OSC_PING), 2)) {
							PRINTF("EOS: Pong\r\n");
							EOS_doneHandshake = true;
						};

						// Macro Count
						if (
								OSC_MatchAddr(EOS_OSC_GET, strlen((char*)EOS_OSC_GET), 2) &&
								OSC_MatchAddr(EOS_OSC_MACRO, strlen((char*)EOS_OSC_MACRO), 3) &&
								OSC_MatchAddr(EOS_OSC_MACRO_COUNT, strlen((char*)EOS_OSC_MACRO_COUNT), 4)
							)
						{
							// Get index (arg 0)
							if (OSC_GetArgType(0) == OSC_DATATYPE_INT32) {
								int32_t *argValue = (int32_t*)OSC_GetArgValue(0);
								if (argValue) {
									EOS_MacroCount = swap_uint32(*argValue);
									PRINTF("EOS: Macro count %u\r\n", EOS_MacroCount);
								}
							}
						}; //..Macro Count

						// Macro Details
						if (
								OSC_MatchAddr(EOS_OSC_GET, strlen((char*)EOS_OSC_GET), 2) &&
								OSC_MatchAddr(EOS_OSC_MACRO, strlen((char*)EOS_OSC_MACRO), 3) &&
								OSC_MatchAddr(EOS_OSC_MACRO_LIST, strlen((char*)EOS_OSC_MACRO_LIST), 5)
							)
						{
							unsigned char* addressMacroNum[256];
							if (OSC_GetAddr(4, addressMacroNum)) // Address 4 is Macro number (NOT Macro Index)
							{
								unsigned int macroNum = atoi((char*)addressMacroNum);
								// Get Label (arg 2)
								if (OSC_GetArgType(2) == OSC_DATATYPE_STRING) {
									int32_t *argValue = (int32_t*)OSC_GetArgValue(2);
									if (argValue) {
										// Does the Macro name match any that we are looking for?
										PRINTF("EOS: Macro %u label '%s'\r\n", macroNum, (char*)argValue);
										if (strcmp((char*)argValue, SNES_ButtonString[SNES_BUTTON_A]) == 0)
											EOS_Macro_SNES_A = macroNum;
										else if (strcmp((char*)argValue, SNES_ButtonString[SNES_BUTTON_B]) == 0)
											EOS_Macro_SNES_B = macroNum;
										else if (strcmp((char*)argValue, SNES_ButtonString[SNES_BUTTON_X]) == 0)
											EOS_Macro_SNES_X = macroNum;
										else if (strcmp((char*)argValue, SNES_ButtonString[SNES_BUTTON_Y]) == 0)
											EOS_Macro_SNES_Y = macroNum;
										else if (strcmp((char*)argValue, SNES_ButtonString[SNES_KONAMI]) == 0)
											EOS_Macro_SNES_KONAMI = macroNum;
									}
								}

							}
						}; //..Macro Details

					} //..EOS OSC
				} //..Check if OSC

			// Done with frame
			OSCRXFrameSize = 0;

        	// Processing done
			LED_RED_OFF();
        }
    }
    s_recvSize = 0;

	// RX Done
	LED_RED_OFF();
}

void txOSC() {
	uint8_t osccmd[256];

	if (!EOS_doneHandshake)
	{
		/* Unsolicited handshake and ping */
		if (xTaskGetTickCount() > lastPing + pingInterval) {
			slip_send(EOS_Handshake_Response, sizeof(EOS_Handshake_Response) - 1);
			sprintf((char*)osccmd,"/%s/%s", EOS_OSC, EOS_OSC_PING);
			slip_send(osccmd, strlen((char *)osccmd) + 1);
			lastPing = xTaskGetTickCount();
		}
		return;
	}

	/* PING */
	if (xTaskGetTickCount() > lastPing + pingInterval) {
		// OSC Ping
		sprintf((char*)osccmd,"/%s/%s", EOS_OSC, EOS_OSC_PING);
		slip_send(osccmd, strlen((char *)osccmd) + 1);
		lastPing = xTaskGetTickCount();
	}

	/* Update Macro Count */
	if ((lastMacroUpdate ==0) || (xTaskGetTickCount() > lastMacroUpdate + macroUpdateInterval)) {
		// Update Macro Count if we've gotten all the current details
		if (EOS_MacroCount == EOS_MacroCountPolled) {
			EOS_MacroCount = 0;
			EOS_MacroCountPolled = 0;
			sprintf((char*)osccmd,"/%s/%s/%s/%s", EOS_OSC, EOS_OSC_GET, EOS_OSC_MACRO, EOS_OSC_MACRO_COUNT);
			slip_send(osccmd, strlen((char *)osccmd) + 1);
			lastMacroUpdate = xTaskGetTickCount();
		}
	}

	/* Poll Macro Details */
	if (xTaskGetTickCount() > lastMacroPollUpdate + macroPollInterval) {
		// Get next macro details
		if (EOS_MacroCount > EOS_MacroCountPolled) {
			sprintf((char*)osccmd,"/%s/%s/%s/%s/%u", EOS_OSC, EOS_OSC_GET, EOS_OSC_MACRO, EOS_OSC_MACRO_INDEX, EOS_MacroCountPolled++);
			slip_send(osccmd, strlen((char *)osccmd) + 1);
			lastMacroPollUpdate = xTaskGetTickCount();
		}
	}

	/* SNES Buttons */
	for (int button = 0; button < SNES_BUTTON_COUNT; button++ ) {
		if (SNES_ButtonStatus[button].Tick != savedButtonTick[button]) {
			// New button event
			switch (SNES_ButtonStatus[button].State) {
				case SNES_BUTTONSTATE_UP:
					break;
				case SNES_BUTTONSTATE_DOWN:
				case SNES_BUTTONSTATE_DOWN_REPEAT:
					switch (button) {
						case (SNES_BUTTON_UP):
							sprintf((char*)osccmd,"/%s/%s/%s/%s", EOS_OSC, EOS_OSC_PARAM, EOS_OSC_PARAM_TILT, EOS_OSC_KEY_PLUS_PERCENT);
							slip_send(osccmd, strlen((char *)osccmd) + 1);
							break;
						case (SNES_BUTTON_DOWN):
							sprintf((char*)osccmd,"/%s/%s/%s/%s", EOS_OSC, EOS_OSC_PARAM, EOS_OSC_PARAM_TILT, EOS_OSC_KEY_MINUS_PERCENT);
							slip_send(osccmd, strlen((char *)osccmd) + 1);
							break;
						case (SNES_BUTTON_LEFT):
							sprintf((char*)osccmd,"/%s/%s/%s/%s", EOS_OSC, EOS_OSC_PARAM, EOS_OSC_PARAM_PAN, EOS_OSC_KEY_MINUS_PERCENT);
							slip_send(osccmd, strlen((char *)osccmd) + 1);
							break;
						case (SNES_BUTTON_RIGHT):
							sprintf((char*)osccmd,"/%s/%s/%s/%s", EOS_OSC, EOS_OSC_PARAM, EOS_OSC_PARAM_PAN, EOS_OSC_KEY_PLUS_PERCENT);
							slip_send(osccmd, strlen((char *)osccmd) + 1);
							break;
						case (SNES_BUTTON_SELECT):
							if (SNES_ButtonStatus[button].State == SNES_BUTTONSTATE_DOWN)
							{
								sprintf((char*)osccmd,"/%s/%s/%s/%s", EOS_OSC, EOS_OSC_CMD, EOS_OSC_CMD_HIGHLIGHT, EOS_OSC_KEY_ENTER );
								slip_send(osccmd, strlen((char *)osccmd) + 1);

								sprintf((char*)osccmd,"/%s/%s/%s/%s", EOS_OSC, EOS_OSC_CMD, EOS_OSC_CMD_SELECTLAST, EOS_OSC_KEY_ENTER);
								slip_send(osccmd, strlen((char *)osccmd) + 1);
							}
							break;
						case (SNES_BUTTON_START):
							if (SNES_ButtonStatus[button].State == SNES_BUTTONSTATE_DOWN)
								sprintf((char*)osccmd,"/%s/%s/%s/%s", EOS_OSC, EOS_OSC_CMD, EOS_OSC_CMD_CHAN1, EOS_OSC_KEY_ENTER);
								slip_send(osccmd, strlen((char *)osccmd) + 1);
							break;
						case (SNES_BUTTON_L):
							sprintf((char*)osccmd,"/%s/%s/%s/%s", EOS_OSC, EOS_OSC_CMD, EOS_OSC_CMD_LAST, EOS_OSC_KEY_ENTER);
							slip_send(osccmd, strlen((char *)osccmd) + 1);
							break;
						case (SNES_BUTTON_R):
							sprintf((char*)osccmd,"/%s/%s/%s/%s", EOS_OSC, EOS_OSC_CMD, EOS_OSC_CMD_NEXT, EOS_OSC_KEY_ENTER);
							slip_send(osccmd, strlen((char *)osccmd) + 1);
							break;
						case (SNES_BUTTON_A):
							if (EOS_Macro_SNES_A) {
								PRINTF("EOS: FIRE MACRO %d\r\n", EOS_Macro_SNES_A);
								sprintf((char*)osccmd,"/%s/%s/%u/%s", EOS_OSC, EOS_OSC_MACRO, EOS_Macro_SNES_A, EOS_OSC_MACRO_FIRE);
								slip_send(osccmd, strlen((char *)osccmd) + 1);
							}
							break;
						case (SNES_BUTTON_B):
							if (EOS_Macro_SNES_B) {
								PRINTF("EOS: FIRE MACRO %d\r\n", EOS_Macro_SNES_B);
								sprintf((char*)osccmd,"/%s/%s/%u/%s", EOS_OSC, EOS_OSC_MACRO, EOS_Macro_SNES_B, EOS_OSC_MACRO_FIRE);
								slip_send(osccmd, strlen((char *)osccmd) + 1);
							}
							break;
						case (SNES_BUTTON_X):
							if (EOS_Macro_SNES_X) {
								PRINTF("EOS: FIRE MACRO %d\r\n", EOS_Macro_SNES_X);
								sprintf((char*)osccmd,"/%s/%s/%u/%s", EOS_OSC, EOS_OSC_MACRO, EOS_Macro_SNES_X, EOS_OSC_MACRO_FIRE);
								slip_send(osccmd, strlen((char *)osccmd) + 1);
							}
							break;
						case (SNES_BUTTON_Y):
							if (EOS_Macro_SNES_Y) {
								PRINTF("EOS: FIRE MACRO %d\r\n", EOS_Macro_SNES_Y);
								sprintf((char*)osccmd,"/%s/%s/%u/%s", EOS_OSC, EOS_OSC_MACRO, EOS_Macro_SNES_Y, EOS_OSC_MACRO_FIRE);
								slip_send(osccmd, strlen((char *)osccmd) + 1);
							}
							break;
						case (SNES_KONAMI):
							if (EOS_Macro_SNES_KONAMI) {
								PRINTF("EOS: FIRE MACRO %d\r\n", EOS_Macro_SNES_KONAMI);
								sprintf((char*)osccmd,"/%s/%s/%u/%s", EOS_OSC, EOS_OSC_MACRO, EOS_Macro_SNES_KONAMI, EOS_OSC_MACRO_FIRE);
								slip_send(osccmd, strlen((char *)osccmd) + 1);
							}
							break;
					}
					break;
			}

			// Save last seen time
			savedButtonTick[button] = SNES_ButtonStatus[button].Tick;
		}
	}
}

/*!
 * @brief Task responsible for SLIP to EOS via VCP
 */
static void eos_task(void *pvParameters) {
	const TickType_t xRefresh = 10 / portTICK_PERIOD_MS;

	/* Init */
	LED_RED_INIT(0);


	/* Start USB */
	USB_DeviceApplicationInit();

	for(;;) {
		if (s_cdcVcom.attach) {
			/*
			 * EOS doesn't set DTR
			 * So pretend it's set!
			 */
			usb_cdc_acm_info_t *acmInfo = &s_usbCdcAcmInfo;
			acmInfo->uartState |= USB_DEVICE_CDC_UART_STATE_RX_CARRIER;
			s_cdcVcom.startTransactions = 1;
		}

        if ((s_cdcVcom.attach) && (s_cdcVcom.startTransactions))
        {
            if ((0 != s_recvSize) && (0xFFFFFFFF != s_recvSize))
            {

            	// Process any incoming OSC
            	rxOSC();

				// Get next packet
				USB_DeviceCdcAcmRecv(s_cdcVcom.cdcAcmHandle, USB_CDC_VCOM_BULK_OUT_ENDPOINT, s_currRecvBuf, g_UsbDeviceCdcVcomDicEndpoints[0].maxPacketSize);

            } else {

            	// Send any required OSC
				txOSC();

            }
        } else {

        	// USB has gone away...
        	EOS_doneHandshake = false;

        }

        vTaskDelay(xRefresh);
	} // Forever
}

/*!
 * @brief Application entry point.
 */
int main(void) {
	/* Init board hardware. */
	BOARD_InitBootPins();
	BOARD_BootClockRUN();
	BOARD_InitDebugConsole();

	PRINTF("\r\n");
	PRINTF("--------------------\r\n");
	PRINTF("#lighthack KL46 SNES\r\n");
	PRINTF("%s %s\r\n", __DATE__, __TIME__);
	PRINTF("--------------------\r\n");

	/* Create RTOS task */
	xTaskCreate(snes_task, "SNES_task", configMINIMAL_STACK_SIZE, NULL, snes_task_PRIORITY, NULL);
	xTaskCreate(eos_task, "EOS_task", configMINIMAL_STACK_SIZE * 20, &s_cdcVcom, eos_task_PRIORITY, &s_cdcVcom.applicationTaskHandle);

	vTaskStartScheduler();

	for(;;) { /* Infinite loop to avoid leaving the main function */
		__asm("NOP"); /* something to use as a breakpoint stop while looping */
	}
}



