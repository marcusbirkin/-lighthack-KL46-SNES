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

/* Task priorities. */
#define heartbeat_task_PRIORITY (configMAX_PRIORITIES - 1)
#define snes_task_PRIORITY (configMAX_PRIORITIES - 1)
#define eos_task_PRIORITY (configMAX_PRIORITIES - 1)

/* Constants */
// SLIP Encodings
static const uint8_t SLIP_END = 0xC0;
static const uint8_t SLIP_ESC = 0xDB;
static const uint8_t SLIP_ESC_END = 0xDC;
static const uint8_t SLIP_ESC_ESC = 0xDD;

/* Variables */
uint8_t OSCRXFrame[256];
uint8_t OSCRXFrameSize = 0;
uint8_t OSCTXFrame[256];
uint8_t OSCTXFrameSize = 0;

/*!
 * @brief Task responsible for heartbeat
 */
static void heartbeat_task(void *pvParameters) {
	const TickType_t xDelay = 500 / portTICK_PERIOD_MS;
	LED_RED_INIT(0);
	for (;;) {
		LED_RED_TOGGLE();
		vTaskDelay(xDelay);
	}
}

/*!
 * @brief Task responsible for reading SNES controller
 */
static void snes_task(void *pvParameters) {
	const TickType_t xRefresh = 20 / portTICK_PERIOD_MS;

	SNES_Init();

	TickType_t savedButtonTick[SNES_BUTTON_COUNT];
	for (int n = 0; n < SNES_BUTTON_COUNT; n++)
		savedButtonTick[n] = SNES_BUTTONSTATE_UP;

	for (;;) {
		LED_GREEN_TOGGLE();

		// Read buttons
		SNES_ReadButtons();

		// Check buttons
		for (int n = 0; n < SNES_BUTTON_COUNT; n++ ) {
			if (SNES_ButtonStatus[n].Tick != savedButtonTick[n]) {
				// New button event
				switch (SNES_ButtonStatus[n].State) {
					case SNES_BUTTONSTATE_UP:
						break;
					case SNES_BUTTONSTATE_DOWN:
						PRINTF("Down: %s\r\n", SNES_ButtonString[n]);
						break;
					case SNES_BUTTONSTATE_DOWN_REPEAT:
						PRINTF("Repeat: %s\r\n", SNES_ButtonString[n]);
						break;
				}

				// Save last seen time
				savedButtonTick[n] = SNES_ButtonStatus[n].Tick;
			}
		}

		vTaskDelay(xRefresh);
	}
}


/*!
 * @brief SLIP encode buffer and send to EOS via VCP
 */
void slip_send(const uint8_t *buffer, const uint32_t size) {

	/* Copy to output buffer */
	memcpy(s_currSendBuf, buffer, size);
	s_sendSize = size;

	/* Terminate correctly */
	s_currSendBuf[s_sendSize++] = SLIP_END; // Replace NULL termination with SLIP END

	/* Send */
	PRINTF("Sent: ");
	for (int n = 0; n < s_sendSize; n++) {
			PRINTF("%c", s_currSendBuf[n]);
	}
	PRINTF("\r\n", s_currSendBuf);
//	PRINTF("-");
//	for (int n = 0; n < s_sendSize; n++) {
//		PRINTF("0%x", s_currSendBuf[n]);
//	}
//	PRINTF("\r\n");

	usb_status_t error =
		USB_DeviceCdcAcmSend(s_cdcVcom.cdcAcmHandle, USB_CDC_VCOM_BULK_IN_ENDPOINT, s_currSendBuf, s_sendSize);
	s_sendSize = 0;
	if (error != kStatus_USB_Success)
	{
		PRINTF("USB error code: %d\r\n", error);
	}

	// Wait 100ms to allow to send
	vTaskDelay(100 / portTICK_PERIOD_MS);
}

/*!
 * @brief Task responsible for SLIP to EOS via VCP
 */
static void eos_task(void *pvParameters) {
	const TickType_t xRefresh = 20 / portTICK_PERIOD_MS;

	/* EOS Handshakes */
	static const uint8_t EOS_Handshake_Challenge[] = "ETCOSC?";
	static const uint8_t EOS_Handshake_Response[] = "OK";
	bool doneHandshake = false;

	/* OSC Commands */
	static const uint8_t EOS_OSC_Ping[] = "/eos/ping";
	static const uint8_t EOS_OSC_Pong[] = "/eos/out/ping";
	const TickType_t pingInterval = 5000 / portTICK_PERIOD_MS;
	TickType_t lastPing = 0;
	static const uint8_t EOS_OSC_PAN_LEFT[] = "/eos/param/Pan/-%";
	static const uint8_t EOS_OSC_PAN_RIGHT[] = "/eos/param/Pan/+%";
	static const uint8_t EOS_OSC_TILT_UP[] = "/eos/param/Tilt/+%";
	static const uint8_t EOS_OSC_TILT_DOWN[] = "/eos/param/Tilt/-%";
	static const uint8_t EOS_OSC_HIGHTLIGHT[] = "/eos/cmd/Highlight#";
	static const uint8_t EOS_OSC_SELECTLAST[] = "/eos/cmd/Select_Last#";
	static const uint8_t EOS_OSC_NEXT[] = "/eos/cmd/Next#";
	static const uint8_t EOS_OSC_LAST[] = "/eos/cmd/Last#";
	static const uint8_t EOS_OSC_CHAN1[] = "/eos/cmd/Chan 1#";

	/* SNES Stuff */
	TickType_t savedButtonTick[SNES_BUTTON_COUNT] ;

	/* Start USB */
	USB_DeviceApplicationInit();



	for(;;) {
		/*
		 * EOS doesn't set DTR
		 * So pretend it's set!
		 */
		usb_cdc_acm_info_t *acmInfo = &s_usbCdcAcmInfo;
		acmInfo->uartState |= USB_DEVICE_CDC_UART_STATE_RX_CARRIER;
		s_cdcVcom.startTransactions = 1;

		/* Do background USB */
		USB_DeviceCdcAcmSend(s_cdcVcom.cdcAcmHandle, USB_CDC_VCOM_BULK_IN_ENDPOINT, NULL, 0);

        if ((1 == s_cdcVcom.attach) && (1 == s_cdcVcom.startTransactions))
        {
    		/* Deal with any RX */
            if ((0 != s_recvSize) && (0xFFFFFFFF != s_recvSize))
            {
                int32_t i;

                /* Check for SLIP end of frame */
                for (i = 0; i < s_recvSize; i++)
                {
                	OSCRXFrame[OSCRXFrameSize++] = s_currRecvBuf[i];
                    if (s_currRecvBuf[i] == SLIP_END) {
                    	OSCRXFrame[--OSCRXFrameSize] = 0; // Remove slip char, and null terminate

                    	/* Process frame */
                    	// EOS handshake challenge
                    	if (strcmp((char *)OSCRXFrame, (char *)EOS_Handshake_Challenge) == 0) {
                    		PRINTF("EOS: Handshake challenge, sending response...");
                    		slip_send(EOS_Handshake_Response, sizeof(EOS_Handshake_Response) - 1);
                    		doneHandshake = true;
                    	} else

                    	// Pong
                    	if (strcmp((char *)OSCRXFrame, (char *)EOS_OSC_Pong) == 0) {
							PRINTF("EOS: Pong\r\n");
						} else

						// Other
						{
							if (OSCRXFrameSize > 1) {
								PRINTF("Received: '%s'\r\n", OSCRXFrame);
							}
						}

                    	// Done with frame
                    	OSCRXFrameSize = 0;
                    }
                }
                s_recvSize = 0;

            }
			/* OR send any TX required */
			else if (doneHandshake)
			{
				/* PING */
				if (xTaskGetTickCount() > lastPing + pingInterval) {
					// OSC Ping
					slip_send(EOS_OSC_Ping, sizeof(EOS_OSC_Ping));
					lastPing = xTaskGetTickCount();
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
										slip_send(EOS_OSC_TILT_UP, sizeof(EOS_OSC_TILT_UP));
										break;
									case (SNES_BUTTON_DOWN):
										slip_send(EOS_OSC_TILT_DOWN, sizeof(EOS_OSC_TILT_DOWN));
										break;
									case (SNES_BUTTON_LEFT):
										slip_send(EOS_OSC_PAN_LEFT, sizeof(EOS_OSC_PAN_LEFT));
										break;
									case (SNES_BUTTON_RIGHT):
										slip_send(EOS_OSC_PAN_RIGHT, sizeof(EOS_OSC_PAN_RIGHT));
										break;
									case (SNES_BUTTON_SELECT):
										if (SNES_ButtonStatus[button].State == SNES_BUTTONSTATE_DOWN)
										{
											slip_send(EOS_OSC_HIGHTLIGHT, sizeof(EOS_OSC_HIGHTLIGHT));
											slip_send(EOS_OSC_SELECTLAST, sizeof(EOS_OSC_SELECTLAST));
										}
										break;
									case (SNES_BUTTON_START):
										if (SNES_ButtonStatus[button].State == SNES_BUTTONSTATE_DOWN)
											slip_send(EOS_OSC_CHAN1, sizeof(EOS_OSC_CHAN1));
										break;
									case (SNES_BUTTON_L):
										slip_send(EOS_OSC_LAST, sizeof(EOS_OSC_LAST));
										break;
									case (SNES_BUTTON_R):
										slip_send(EOS_OSC_NEXT, sizeof(EOS_OSC_NEXT));
										break;
									break;
								}
								break;
						}

						// Save last seen time
						savedButtonTick[button] = SNES_ButtonStatus[button].Tick;
					}
				}


			} // RX or TX

        } else {
        	doneHandshake = false;
        } // USB Connected?

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
	xTaskCreate(heartbeat_task, "Heartbeat_task", configMINIMAL_STACK_SIZE, NULL, heartbeat_task_PRIORITY, NULL);
	xTaskCreate(snes_task, "SNES_task", configMINIMAL_STACK_SIZE, NULL, snes_task_PRIORITY, NULL);
	xTaskCreate(eos_task, "EOS_task", 5000L / sizeof(portSTACK_TYPE), &s_cdcVcom, eos_task_PRIORITY, &s_cdcVcom.applicationTaskHandle);

	vTaskStartScheduler();

	for(;;) { /* Infinite loop to avoid leaving the main function */
		__asm("NOP"); /* something to use as a breakpoint stop while looping */
	}
}



