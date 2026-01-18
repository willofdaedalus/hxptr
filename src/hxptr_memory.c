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
	entry->byte_count = record->byte_count;
	entry->line_num = record->line_num;
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

static int sm_comp(const void *a, const void *b)
{
	memory_entry_t *entry_a = (memory_entry_t *) a;
	memory_entry_t *entry_b = (memory_entry_t *) b;

	if (entry_a->address < entry_b->address) {
		return -1;
	}
	if (entry_a->address > entry_b->address) {
		return 1;
	}

	// tie-breaker; compare by the line_num as most recent should always win
	if (entry_a->line_num < entry_b->address) {
		return -1;
	}
	if (entry_a->line_num > entry_b->address) {
		return 1;
	}
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

void detect_overlaps(sparse_memory_t *mem)
{
	memory_entry_t *right;
	memory_entry_t *left;
	uint32_t left_end, over_start, over_end, over_size;

	if (!mem->finalized) {
		fprintf(stderr,
			"error: call finalize() before detecting overlaps\n");
		return;
	}

	for (size_t i = 0; i < mem->count - 1; i++) {
		left = &mem->entries[i];
		right = &mem->entries[i + 1];

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

			printf("overlap detected:\n");
			printf(" range 1: 0x%08x-0x%08x (line %u)\n",
			       left->address,
			       left_end,
			       left->line_num);
			printf("  range 2: 0x%08x-0x%08x (line %u)\n",
			       right->address,
			       right->address + right->byte_count - 1,
			       right->line_num);
			printf("  overlap: 0x%08x-0x%08x (%u bytes)\n",
			       over_start,
			       over_end,
			       over_size);
			printf("\n");
		}
	}
}

HxptrResult_t sm_finalize(sparse_memory_t *mem)
{
	if (!mem) {
		return HX_ERR_NULL;
	}

	qsort(mem->entries, mem->count, sizeof(memory_entry_t), sm_comp);
	mem->finalized = 1;
	detect_overlaps(mem);
	// sm_print_entries(mem);


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
