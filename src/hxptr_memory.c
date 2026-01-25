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
		if (!mem->entries) {
			// TODO; make sure to free this from the caller side of
			// things
			return HX_ERR_NULL;
		}
	}

	entry = &mem->entries[mem->count];
	// make sure this address is never more than the length of the mcu
	// profile
	entry->address = base_addr;
	entry->byte_count = record->byte_count;
	entry->line_num = record->linenum;
	entry->checksum = record->checksum;
	entry->bytes = malloc(record->byte_count * sizeof(uint8_t));
	if (!entry->bytes) {
		return HX_ERR_BAD_APPEND;
	}
	memcpy(entry->bytes, record->bytes, record->byte_count);
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
	uint32_t start_end, overlap_start, overlap_end, overlap_sz, next_end;
	// gap check variables
	uint32_t cur_end_bound, gap_size;

	if (!ctx->memory->finalized) {
		return HX_ERR_NOT_FINALIZED;
	}

	stats = ctx->stats;
	for (uint32_t i = 0; i < ctx->memory->count; i++) {
		cur = &ctx->memory->entries[i];

		if (i == 0) {
			stats->addr_lowest = cur->address;
		}
		if (i == ctx->memory->count - 1) {
			stats->addr_highest =
				cur->address + cur->byte_count - 1;
		}

		// make sure not to exceed actual last entry of the sparse
		// memory structure
		// overlap detection
		if (i < ctx->memory->count - 1) {
			next = &ctx->memory->entries[i + 1];
			start_end = cur->address + cur->byte_count - 1;

			if (next->address <= start_end) {
				overlap_start = next->address;
				overlap_end = start_end;

				next_end = next->address + next->byte_count + 1;
				if (next_end < start_end) {
					overlap_end = next_end;
				}

				overlap_sz = overlap_end - overlap_start + 1;

				diagnostic_t d = {
					.type = DIAG_OVERLAP,
					.first_idx = i,
					.second_idx = i + 1,
					.address = overlap_start,
					.size = overlap_sz,
				};

				diag_append(ctx, &d);
				stats->total_overlaps++;
			} else {
				cur_end_bound = cur->address + cur->byte_count;
				gap_size = next->address - cur_end_bound;

				if (gap_size >= threshold) {
					diagnostic_t d = {
						.type = DIAG_GAP,
						.first_idx = i,
						.second_idx = i + 1,
						.address = cur_end_bound,
						.size = gap_size,
					};
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

static void sm_print_entries(const sparse_memory_t *mem)
{
	if (!mem)
		return;

	for (size_t i = 0; i < mem->count; ++i) {
		memory_entry_t *entry = &mem->entries[i];

		if (entry->is_data_record) {
			// print address in hex
			printf("[0x%08x] ", entry->address);

			// print bytes separated by spaces
			for (uint8_t j = 0; j < entry->byte_count; ++j) {
				printf("%02x ", entry->bytes[j]);
			}
			printf("\n");
		}
	}
}

// HxptrResult_t sm_detect_overlaps(hxptr_t *ctx)
// {
// 	if (!ctx || !ctx->memory || !ctx->reports) {
// 		return HX_ERR_NULL;
// 	}
// 	// sm_print_entries(ctx->memory);
// 	if (!ctx->memory->finalized) {
// 		fprintf(stderr,
// 			"error: call finalize() before detecting overlaps\n");
// 		return HX_ERR_NOT_FINALIZED;
// 	}
//
// 	for (size_t i = 0; i < ctx->memory->count - 1; i++) {
// 		memory_entry_t *first = &ctx->memory->entries[i];
// 		memory_entry_t *second = &ctx->memory->entries[i + 1];
//
// 		uint32_t first_end = first->address + first->byte_count - 1;
//
// 		if (second->address <= first_end) {
// 			// calculate overlap
// 			uint32_t overlap_start = second->address;
// 			uint32_t overlap_end = first_end;
//
// 			uint32_t second_end =
// 				second->address + second->byte_count - 1;
// 			if (second_end < first_end) {
// 				overlap_end = second_end;
// 			}
//
// 			uint32_t overlap_size = overlap_end - overlap_start + 1;
//
// 			// Create diagnostic with INDICES, not data
// 			diagnostic_t diag = {.level = LEVEL_WARN,
// 					     .first_range_idx = i,
// 					     .second_range_idx = i + 1,
// 					     .overlap_start = overlap_start,
// 					     .overlap_size = overlap_size};
//
// 			HxptrResult_t res = diag_append(ctx, &diag);
// 			if (res != HX_OK) {
// 				return res;
// 			}
// 		}
// 	}
//
// 	return HX_OK;
// }

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
