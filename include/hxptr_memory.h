#ifndef HXPTR_MEMORY_H
#define HXPTR_MEMORY_H

#include "hxptr.h"

#define MEM_CAPACITY 4098

sparse_memory_t *sm_init(void);
// HxptrResult_t sm_append_record(sparse_memory_t *mem, hxptr_record_t *record);
HxptrResult_t sm_append_record(
	sparse_memory_t *mem, hxptr_record_t *record, uint32_t addr_prefix);
void sm_free(sparse_memory_t *mem);
HxptrResult_t sm_analyze(hxptr_t *ctx, uint32_t threshold);

#endif
