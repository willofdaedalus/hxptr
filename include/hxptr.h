#ifndef HXPTR_H
#define HXPTR_H

#include <stdint.h>

#define ESA_FACTOR 16
#define ADDR_COUNT 2
#define START_COUNT 4

typedef enum {
	HX_OK = 0,
	HX_ERR_INV_RECORD = -1,
	HX_ERR_NULL = -2,
	HX_ERR_NO_RECORD = -3,
	HX_ERR_UKN_TYPE = -4,
	HX_ERR_BAD_CHKSM = -5,
	HX_ERR_BAD_APPEND = -6,
	HX_ERR_BAD_FILE = -7,
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
	uint32_t address;   // contains extended hi and lo bytes
	uint32_t line_num;  // line number for conflict resolutions
	uint8_t byte_count; // number of bytes to help in fast rendering
	uint8_t *bytes;	    // actual data
	uint8_t checksum;   // checksum
} memory_entry_t;

typedef struct {
	uint32_t count;
	uint32_t cap;
	memory_entry_t *entries;
	uint8_t finalized;
} sparse_memory_t;

typedef struct {
	uint32_t line_num;
	uint16_t address;
	uint8_t byte_count;
	uint8_t bytes[256];
	uint8_t checksum;
	IHexRecordType_t type;
} hxptr_record_t;

typedef struct {
	uint32_t total_records;
	uint32_t total_bytes;

	uint32_t segment;
	uint32_t linear;
	uint32_t cs;
	uint32_t ip;

	sparse_memory_t *memory;
	uint8_t eof;
	uint8_t cur_type;
} hxptr_t;

hxptr_t *hxptr_init(void);
void hxptr_free(hxptr_t *hx);
HxptrResult_t hxptr_hex_to_record(char *line, hxptr_record_t *record);
HxptrResult_t hxptr_append_record(hxptr_t *ctx, hxptr_record_t *record);
void record_to_str(hxptr_record_t *record);

#endif
