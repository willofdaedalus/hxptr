#include "hxptr_memory.h"
#include <stdio.h>
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
	entry->byte_count = record->byte_count;
	entry->line_num = record->line_num;
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

static void sm_print_entries(const sparse_memory_t *mem)
{
	if (!mem)
		return;

	for (size_t i = 0; i < mem->count; ++i) {
		memory_entry_t *entry = &mem->entries[i];

		// print address in hex
		printf("[0x%08X] ", entry->address);

		// print bytes separated by spaces
		for (uint8_t j = 0; j < entry->byte_count; ++j) {
			printf("%02X ", entry->bytes[j]);
		}

		printf("\n");
	}
}

HxptrResult_t sm_detect_overlaps(hxptr_t *hx)
{
	diagnostic_t diag = {0};
	memory_entry_t *right;
	memory_entry_t *left;
	uint32_t left_end, over_start, over_end, over_size;

	if (!hx->diagnostics) {
		return HX_ERR_NULL;
	}

	if (!hx->memory->finalized) {
		fprintf(stderr,
			"error: call finalize() before detecting overlaps\n");
		return HX_ERR_NULL;
	}

	for (size_t i = 0; i < hx->memory->count - 1; i++) {
		left = &hx->memory->entries[i];
		right = &hx->memory->entries[i + 1];

		left_end = left->address + left->byte_count - 1;

		// check if right starts before left ends
		if (right->address <= left_end) {
			// overlap detected!
			over_start = right->address;
			over_end = (left_end <
				    right->address + right->byte_count - 1)
					   ? left_end
					   : right->address +
						     right->byte_count - 1;
			over_size = over_end - over_start + 1;

			diag = (diagnostic_t) {.line_primary = left->line_num,
					       .left_addr = left->address,
					       .right_addr = right->address,
					       .level = LEVEL_WARN};


			// printf("overlap detected:\n");
			// printf(" range 1: 0x%08x-0x%08x (line %u)\n",
			//        left->address,
			//        left_end,
			//        left->line_num);
			// printf("  range 2: 0x%08x-0x%08x (line %u)\n",
			//        right->address,
			//        right->address + right->byte_count - 1,
			//        right->line_num);
			// printf("  overlap: 0x%08x-0x%08x (%u bytes)\n",
			//        over_start,
			//        over_end,
			//        over_size);
			// printf("\n");
		}
	}

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
