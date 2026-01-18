#include "hxptr_memory.h"
#include <stdlib.h>
#include <string.h>

sparse_memory_t *sm_init(void)
{
	sparse_memory_t *mem;

	mem = calloc(1, sizeof(sparse_memory_t));
	if (!mem) {
		return NULL;
	}

	mem->cap = MEM_CAPACITY;
	mem->entries = malloc(mem->cap * sizeof(memory_entry_t));
	if (!mem->entries) {
		return NULL;
	}

	return mem;
}

HxptrResult_t sm_append_record(sparse_memory_t *mem,
			       hxptr_record_t *record,
			       uint32_t base_addr)
{
	memory_entry_t *entry;

	if (!record || !mem) {
		return HX_ERR_NULL;
	}

	// check and double the capacity when necessary
	if (mem->count >= mem->cap) {
		mem->cap *= 2;
		mem->entries = realloc(mem->entries,
				       mem->cap * sizeof(memory_entry_t));
	}

	entry = &mem->entries[mem->count];
	// make sure this address is never more than the length of the mcu
	// profile
	entry->address = base_addr;
	entry->checksum = record->checksum;
	entry->bytes = malloc(record->byte_count * sizeof(uint8_t));
	if (!entry->bytes) {
		return HX_ERR_BAD_APPEND;
	}
	memcpy(entry->bytes, record->bytes, record->byte_count);

	mem->count++;
	mem->finalized = 0;

	return HX_OK;
}

void sm_free(sparse_memory_t *mem)
{
	if (!mem) {
		return;
	}

	for (uint32_t i = 0; i < mem->count; i++) {
		free(mem->entries[i].bytes);
	}

	free(mem->entries);
	free(mem);
}
