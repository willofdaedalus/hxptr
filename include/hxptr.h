#ifndef HXPTR_H
#define HXPTR_H

#include <stdint.h>

typedef enum {
	HX_OK = 0,
	HX_ERR_INV_RECORD = -1,
	HX_ERR_NULL = -2,
	HX_ERR_NO_RECORD = -3,
} HxptrResult_t;

typedef enum {
	IHEX_DATA = 0x0, // data
	IHEX_EOF = 0x1,	 // end of file
	IHEX_ESA = 0x2,	 // extended segment address
	IHEX_SSA = 0x3,	 // start segment address
	IHEX_ELA = 0x4,	 // extended linear address
	IHEX_SLA = 0x5	 // start linear address
} IHexRecordType_t;

typedef struct {
	uint8_t byte_count;
	uint16_t address;
	IHexRecordType_t type;
	uint8_t *bytes;
	uint8_t checksum;
} hxptr_record_t;

#endif
