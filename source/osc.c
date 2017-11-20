/*
 * osc.c
 *
 *  Created on: 19 Nov 2017
 *      Author: mbirkin
 */

#include "osc.h"
#include "fsl_debug_console.h"

const uint8_t* m_buffer;
size_t m_size;
unsigned int m_addrCount;
unsigned int m_argCount;

bool OSC_SetBuffer(const unsigned char* OSCBuffer, size_t size) {
	m_buffer = OSCBuffer;
	m_size = size;

	// Get address then argument count
	m_addrCount = 0;
	m_argCount = 0;
	for (int n = 0; n < m_size; n++) {
		if (m_buffer[n] == '/') {
				m_addrCount++;
		}
		if (m_buffer[n] == ',') {
			while (m_buffer[n++] != 0x00) {
				m_argCount++;
			}
			break;
		}
	}
	if (!m_addrCount) {
		return false;
	}

	return true;
}

unsigned int OSC_GetAddrCount() {
	return m_addrCount;
}

bool OSC_MatchAddr(const unsigned char* address, const size_t size, const uint8_t addressIndex) {
	if ((!OSC_GetAddrCount()) || (addressIndex >= OSC_GetAddrCount()))
		return false;

	unsigned int addressCount = 0;
	for (int n = 0; n < m_size; n++) {
		if (m_buffer[n] == '/') {
				addressCount++;
		}
		if (addressCount == (addressIndex + 1)) {
			if (memcmp(&m_buffer[++n], address, size) == 0)
				return true;
			else
				return false;
		}
	}

	return false;
}


bool OSC_GetAddr(const uint8_t addressIndex, unsigned char* address) {
	if ((!OSC_GetAddrCount()) || (addressIndex >= OSC_GetAddrCount()))
		return false;

	unsigned int addressCount = 0;
	for (int n = 0; n < m_size; n++) {
		if (m_buffer[n] == '/') {
				addressCount++;
		}
		if (addressCount == (addressIndex + 1)) {
			size_t size = 0;
			while (m_buffer[++n] != '/') {
				address[size++] = m_buffer[n];
			}
			address[size++] = 0x00;
			return true;
		}
	}

	return false;
}

unsigned int OSC_GetArgCount() {
	return m_argCount;
}

OSC_DataTypes OSC_GetArgType(const uint8_t argIndex) {
	if ((!OSC_GetArgCount()) || (argIndex >= OSC_GetArgCount()))
		return -1;

	for (int n = 0; n < m_size; n++) {
		if (m_buffer[n] == ',') {
			// Found start of argument type list
			// Return requested argument type
			switch (m_buffer[n + 1 + argIndex]) {
				case OSC_DATATYPE_INT32:
					return OSC_DATATYPE_INT32;
					break;
				case OSC_DATATYPE_FLOAT32:
					return OSC_DATATYPE_FLOAT32;
					break;
				case OSC_DATATYPE_STRING:
					return OSC_DATATYPE_STRING;
					break;
				case OSC_DATATYPE_BLOB:
					return OSC_DATATYPE_BLOB;
					break;
				case 0:
					return OSC_DATATYPE_ERROR;
				default:
					return OSC_DATATYPE_UNKNOWN;
					break;
			}
		}
	}

	return -1;
}

const void* OSC_GetArgValue(const uint8_t argIndex) {
	if ((!OSC_GetArgCount()) || (argIndex >= OSC_GetArgCount()))
		return NULL;

	for (int n = 0; n < m_size; n++) {
		if (m_buffer[n] == ',') { // Found start of argument type list,
			// End of known arguments type list
			n += OSC_GetArgCount();
			// End of padding
			n += (4 - n % 4); // Multiple of 32 bit (4 bytes)

			// Find argument value slot
			for (unsigned int arg = 0; arg < argIndex; arg++) {
				switch (OSC_GetArgType(argIndex)) {
					case OSC_DATATYPE_FLOAT32:
					case OSC_DATATYPE_INT32:
						n += 4; // 4 Bytes (32bits)
						break;
					case OSC_DATATYPE_STRING:
					{
						const char* string = (char*)&m_buffer[n];
						n += strlen(string) + (4 -  strlen(string) % 4); // Multiple of 32 bit (4 bytes)
						break;
					}
					case OSC_DATATYPE_BLOB:
						// Todo OSC_DATATYPE_BLOB
					default:
						return NULL;
				}
			}

			// Return argument value pointer
			return &m_buffer[n];
		}
	}

	return NULL;
}
