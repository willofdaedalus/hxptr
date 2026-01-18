#ifndef HXPTR_MEMORY_H
#define HXPTR_MEMORY_H

#include <stdint.h>

#define MEM_CAPACITY 1024

typedef struct {
	uint32_t address;   // contains extended hi and lo bytes
	uint8_t byte_count; // number of bytes to help in fast rendering
	uint8_t *bytes;	    // actual data
	uint8_t checksum;   // checksum
} memory_entry_t;

typedef struct {
	uint32_t count;
	uint32_t capacity;
	memory_entry_t *entries;
	uint8_t finalized;
} sparse_memory_t;

#endif
