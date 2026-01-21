#include "hxptr_memory.h"
#include "hxptr_diagnostics.h"
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
		free(mem);
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

HxptrResult_t sm_detect_overlaps(hxptr_t *ctx)
{
	if (!ctx || !ctx->memory || !ctx->reports) {
		return HX_ERR_NULL;
	}

	if (!ctx->memory->finalized) {
		fprintf(stderr,
			"Error: Call finalize() before detecting overlaps\n");
		return HX_ERR_NOT_FINALIZED;
	}

	for (size_t i = 0; i < ctx->memory->count - 1; i++) {
		memory_entry_t *first = &ctx->memory->entries[i];
		memory_entry_t *second = &ctx->memory->entries[i + 1];

		uint32_t first_end = first->address + first->byte_count - 1;

		// check if second range starts before first ends
		if (second->address <= first_end) {
			// calculate overlap boundaries
			uint32_t overlap_start = second->address;
			uint32_t overlap_end = first_end;

			// if second range ends before first, use that end
			uint32_t second_end =
				second->address + second->byte_count - 1;
			if (second_end < first_end) {
				overlap_end = second_end;
			}

			// create diagnostic
			diagnostic_t diag = {
				.level = LEVEL_WARN,
				.overlap_start = overlap_start,
				.overlap_end = overlap_end,
				.first_write_line = first->line_num,
				.second_write_line = second->line_num};

			HxptrResult_t res = diag_append(ctx, &diag);
			if (res != HX_OK) {
				return res;
			}
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
