#include "hxptr_memory.h"
#include "hxptr.h"
#include <stdlib.h>

sparse_memory_t *sm_init(void)
{
	sparse_memory_t *mem;

	mem = calloc(1, sizeof(sparse_memory_t));
	if (!mem) {
		return NULL;
	}

	mem->capacity = MEM_CAPACITY;
	mem->entries = malloc(mem->capacity * sizeof(memory_entry_t));
	if (!mem->entries) {
		return NULL;
	}

	return mem;
}

HxptrResult_t sm_append_record(sparse_memory_t *mem, hxptr_record_t *record)
{
	if (!record || !mem) {
		return HX_ERR_NULL;
	}

	// marshal record into memory_entry_t
}
