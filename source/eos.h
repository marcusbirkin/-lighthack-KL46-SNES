/*
 * eos.h
 *
 *  Created on: 19 Nov 2017
 *      Author: mbirkin
 */

#ifndef SOURCE_EOS_H_
#define SOURCE_EOS_H_

	/* EOS Handshakes */
	static const unsigned char EOS_Handshake_Challenge[] = "ETCOSC?";
	static const unsigned char EOS_Handshake_Response[] = "OK";
	bool EOS_doneHandshake = false;

	/* OSC Commands */
	// Generic EOS
	static const unsigned char EOS_OSC[] = "eos";
	static const unsigned char EOS_OSC_OUT[] = "out";
	static const unsigned char EOS_OSC_GET[] = "get";

	// Ping
	static const unsigned char EOS_OSC_PING[] = "ping";
	const TickType_t pingInterval = 5000 / portTICK_PERIOD_MS;
	TickType_t lastPing = 0;

	// Params
	static const unsigned char EOS_OSC_PARAM[] = "param";
	static const unsigned char EOS_OSC_PARAM_PAN[] = "pan";
	static const unsigned char EOS_OSC_PARAM_TILT[] = "tilt";

	// Keys
	static const unsigned char EOS_OSC_KEY_PLUS_PERCENT[] = "+%";
	static const unsigned char EOS_OSC_KEY_MINUS_PERCENT[] = "-%";
	static const unsigned char EOS_OSC_KEY_ENTER[] = "#";

	// Commands
	static const unsigned char EOS_OSC_CMD[] = "cmd";
	static const unsigned char EOS_OSC_CMD_HIGHLIGHT[] = "Highlight";
	static const unsigned char EOS_OSC_CMD_SELECTLAST[] = "Select_Last";
	static const unsigned char EOS_OSC_CMD_NEXT[] = "Next";
	static const unsigned char EOS_OSC_CMD_LAST[] = "Last";
	static const unsigned char EOS_OSC_CMD_CHAN1[] = "/Chan 1#";

	// Macro
	static const unsigned char EOS_OSC_MACRO[] = "macro";
	static const unsigned char EOS_OSC_GET_MACRO[] = "/eos/get/macro";
	static const unsigned char EOS_OSC_OUT_MACRO[] = "/eos/out/get/macro";
	static const unsigned char EOS_OSC_MACRO_COUNT[] = "count";
	static const unsigned char EOS_OSC_MACRO_INDEX[] = "index";
	static const unsigned char EOS_OSC_MACRO_LIST[] = "list";
	static const unsigned char EOS_OSC_MACRO_FIRE[] = "fire";
	const TickType_t macroUpdateInterval = 10000 / portTICK_PERIOD_MS;
	TickType_t lastMacroUpdate = 0;
	const TickType_t macroPollInterval = 50 / portTICK_PERIOD_MS;
	TickType_t lastMacroPollUpdate = 0;

	/* EOS Variables */
	unsigned int EOS_MacroCount = 0;
	unsigned int EOS_MacroCountPolled = 0;
	unsigned int EOS_Macro_SNES_A = 0;
	unsigned int EOS_Macro_SNES_B = 0;
	unsigned int EOS_Macro_SNES_X = 0;
	unsigned int EOS_Macro_SNES_Y = 0;

#endif /* SOURCE_EOS_H_ */
