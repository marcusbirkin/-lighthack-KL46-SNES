/*
 * osc.h
 *
 *  Created on: 19 Nov 2017
 *      Author: mbirkin
 */

#include "FreeRTOS.h"
#include <stdbool.h>
#include "byteswap.h"

#ifndef SOURCE_OSC_H_
#define SOURCE_OSC_H_

// OSC Data Types
typedef enum {
	OSC_DATATYPE_ERROR = -1,

	// Standard types
	OSC_DATATYPE_INT32 = 0x69,
	OSC_DATATYPE_FLOAT32 = 0x66,
	OSC_DATATYPE_STRING = 0x73,
	OSC_DATATYPE_BLOB = 0x62,
	OSC_DATATYPE_UNKNOWN = 0xFF,
} OSC_DataTypes;

/* Set OSC buffer */
bool OSC_SetBuffer(const unsigned char* OSCBuffer, size_t size);

/* Return number of Addresses */
unsigned int OSC_GetAddrCount();

/* Check if address is contated in addressIndex */
bool OSC_MatchAddr(const unsigned char* address, const size_t size, const uint8_t addressIndex);

/* Return address at index
 *
 * @return false if addressindex not found
 * @return address Address string (null terminated)
 */
bool OSC_GetAddr(const uint8_t addressIndex, unsigned char* address);

/* Return number of Arguments */
unsigned int OSC_GetArgCount();

/* Return OSC type of argument index
 * Returns -1 if argument not found */
OSC_DataTypes OSC_GetArgType(const uint8_t argIndex);

/* Return OSC value of argument index
 *
 * @return Pointer to argument value
 */
const void* OSC_GetArgValue(const uint8_t argIndex);

#endif /* SOURCE_OSC_H_ */
