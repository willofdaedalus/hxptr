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

HxptrResult_t sm_reserve(
	sparse_memory_t *mem, uint32_t entry_hint, uint32_t pool_hint)
{
	memory_entry_t *new_entries;
	uint8_t *new_pool;

	if (!mem) {
		return HX_ERR_NULL;
	}

	if (entry_hint > mem->cap) {
		new_entries = realloc(mem->entries, entry_hint * sizeof(memory_entry_t));
		if (!new_entries) {
			return HX_ERR_BAD_ALLOC;
		}
		mem->entries = new_entries;
		mem->cap = entry_hint;
	}

	if (pool_hint > mem->pool_cap) {
		new_pool = realloc(mem->data_pool, pool_hint);
		if (!new_pool) {
			return HX_ERR_BAD_ALLOC;
		}

		mem->data_pool = new_pool;
		mem->pool_cap = pool_hint;
	}

	return HX_OK;
}

HxptrResult_t sm_append_record(
	sparse_memory_t *mem, hxptr_record_t *record, uint32_t base_addr)
{
	memory_entry_t *entry, *temp;
	uint8_t *temp_pool;

	if (!record || !mem) {
		return HX_ERR_NULL;
	}

	// grow entries array; independent of pool
	if (mem->count >= mem->cap) {
		size_t new_cap = mem->cap ? mem->cap * 2 : 1;
		temp = realloc(mem->entries, new_cap * sizeof(memory_entry_t));
		if (!temp) {
			return HX_ERR_BAD_ALLOC;
		}
		mem->cap = new_cap;
		mem->entries = temp;
	}

	// grow pool; independent of entries, sized by bytes actually needed
	if (mem->pool_len + record->byte_count > mem->pool_cap) {
		size_t new_pool_cap = mem->pool_cap ? mem->pool_cap * 2 : 4096;
		while (new_pool_cap < mem->pool_len + record->byte_count) {
			new_pool_cap *= 2;
		}
		temp_pool = realloc(mem->data_pool, new_pool_cap);
		if (!temp_pool) {
			return HX_ERR_BAD_ALLOC;
		}
		mem->pool_cap = new_pool_cap;
		mem->data_pool = temp_pool;
	}

	entry = &mem->entries[mem->count];
	entry->address = base_addr;
	entry->byte_count = record->byte_count;
	entry->linenum = record->linenum;
	entry->checksum = record->checksum;
	entry->byte_sum = record->byte_sum;
	entry->data_offset = mem->pool_len;

	if (record->byte_count > 0) {
		memcpy(mem->data_pool + mem->pool_len, record->data, record->byte_count);
	}
	if (record->type == IHEX_DATA) {
		entry->is_data_record = 1;
	}

	mem->count++;
	mem->pool_len += record->byte_count;
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
	if (ctx->memory->count > 0) {
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
	}

	return HX_OK;
}

void sm_free(sparse_memory_t *mem)
{
	if (!mem) {
		return;
	}

	free(mem->entries);
	free(mem->data_pool);
	free(mem);
}
