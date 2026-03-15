#include "hxptr_memory.h"
#include "hxptr_diagnostics.h"
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

HxptrResult_t sm_append_record(
	sparse_memory_t *mem, hxptr_record_t *record, uint32_t base_addr)
{
	memory_entry_t *entry, *temp;

	if (!record || !mem) {
		return HX_ERR_NULL;
	}

	// check and double the capacity when necessary
	if (mem->count >= mem->cap) {
		mem->cap *= 2;
		temp = realloc(mem->entries, mem->cap * sizeof(memory_entry_t));
		if (!temp) {
			// TODO; make sure to free this from the caller side of
			// things
			return HX_ERR_BAD_ALLOC;
		}
		mem->entries = temp;
	}

	entry = &mem->entries[mem->count];
	// make sure this address is never more than the length of the mcu
	// profile
	entry->address = base_addr;
	entry->byte_count = record->byte_count;
	entry->line_num = record->linenum;
	entry->checksum = record->checksum;
	entry->byte_sum = record->byte_sum;
	memcpy(entry->bytes, record->data, record->byte_count);
	if (record->type == IHEX_DATA) {
		entry->is_data_record = 1;
	}

	mem->count++;
	mem->finalized = 0;

	return HX_OK;
}

HxptrResult_t sm_analyze(hxptr_t *ctx, uint32_t threshold)
{
	hxptr_stats_t *stats;
	memory_entry_t *cur, *next;
	// overlap variables
	uint32_t cur_end, overlap_sz;
	// gap check variables
	uint32_t gap_size;

	if (!ctx->memory->finalized) {
		return HX_ERR_NOT_FINALIZED;
	}

	stats = ctx->stats;
	for (uint32_t i = 0; i < ctx->memory->count - 1; i++) {
		cur = &ctx->memory->entries[i];
		next = &ctx->memory->entries[i + 1];

		cur_end = cur->address + cur->byte_count;

		if (next->address < cur_end) {
			// overlap detected
			overlap_sz = cur_end - next->address;
			diagnostic_t d = { .type = DIAG_OVERLAP,
				.address = next->address,
				.size = overlap_sz,
				.first_idx = i,
				.second_idx = i + 1 };
			diag_append(ctx, &d);
			stats->total_overlaps++;
		} else if (next->address > cur_end) {
			// gap detected
			gap_size = next->address - cur_end;
			if (gap_size >= threshold) {
				diagnostic_t d = { .type = DIAG_GAP,
					.address = cur_end,
					.size = gap_size,
					.first_idx = i,
					.second_idx = i + 1 };
				diag_append(ctx, &d);
				stats->total_gaps++;
			}

			if (gap_size > stats->largest_gap) {
				stats->largest_gap = gap_size;
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

	free(mem->entries);
	free(mem);
}
