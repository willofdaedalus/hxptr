#ifndef HXPTR_MEMORY_H
#define HXPTR_MEMORY_H

#include "hxptr.h"

#define MEM_CAPACITY 0x1000

// Allocates and initializes a new sparse memory layout
//
// The entries array and data_pool are left unallocated (zeroed) until the first
// call to sm_append_record() or sm_reserve() sizes them; this keeps sm_init()
// cheap for the common case of loading straight into the parser.
//
// @return	An initialized sparse_memory_t pointer, or NULL on allocation failure
sparse_memory_t *sm_init(void);

// Grows the entries array and/or data pool ahead of time to fit known sizes
//
// This is a pre-sizing hint, not a hard requirement; sm_append_record() will grow
// either structure independently as needed regardless of whether sm_reserve() was
// called first. Existing entries/data_offset values are preserved on growth. Only
// grows; passing a hint smaller than the current capacity is a no-op for that
// structure.
//
// @param mem		The sparse memory layout to grow
// @param entry_hint	Minimum entry capacity to reserve
// @param pool_hint	Minimum data pool capacity (in bytes) to reserve
//
// @return	HX_OK on success, HX_ERR_NULL on a NULL argument, HX_ERR_BAD_ALLOC on
//		growth failure
HxptrResult_t sm_reserve(
	sparse_memory_t *mem, uint32_t entry_hint, uint32_t pool_hint);

// Appends a single resolved record to the sparse memory layout
//
// This copies record's byte data into mem's data_pool (growing it if needed) and
// records a new memory_entry_t pointing at that offset; it does not sort or
// otherwise resolve ordering against previously appended records. base_addr is
// the fully resolved address (after any ESA/ELA state has already been applied by
// the caller), not the raw record->address field. Clears mem->finalized, since
// any append invalidates a previous sort.
//
// @param mem		The sparse memory layout to append into
// @param record	The parsed record to append; byte_count may be 0
// @param base_addr	The fully resolved address for this record
//
// @return	HX_OK on success, HX_ERR_NULL on a NULL argument, HX_ERR_BAD_ALLOC on
//		growth failure
HxptrResult_t sm_append_record(
	sparse_memory_t *mem, hxptr_record_t *record, uint32_t addr_prefix);

// Frees a sparse_memory_t, its entries array, and its data pool
//
// @param mem	The sparse memory layout to free; safe to pass NULL
void sm_free(sparse_memory_t *mem);

// Walks the (already sorted) entries array looking for overlaps and gaps
//
// For each adjacent pair of entries, this compares the end of the current entry
// against the start of the next: an overlap is recorded when the next entry
// starts before the current one ends, and a gap is recorded (and counted toward
// stats->largest_gap regardless of threshold) when the next entry starts strictly
// after and the gap size meets or exceeds threshold. Every detected diagnostic is
// appended to ctx->reports via diag_append() and reflected in ctx->stats.
//
// @param ctx		The active engine context; ctx->memory must already be
//			finalized (sorted) before calling
// @param threshold	Minimum gap size, in bytes, worth reporting as a diagnostic
//
// @return	HX_OK on success, HX_ERR_NOT_FINALIZED if ctx->memory hasn't been
//		sorted yet
HxptrResult_t sm_analyze(hxptr_t *ctx, uint32_t threshold);

#endif
