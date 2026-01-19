#include "hxptr.h"
#include "hxptr_memory.h"
#include "hxptr_record.h"
#include <stdlib.h>

hxptr_t *hxptr_init(void)
{
	hxptr_t *hx;

	hx = calloc(1, sizeof(hxptr_t));
	if (!hx) {
		return NULL;
	}

	hx->memory = sm_init();
	if (!hx->memory) {
		free(hx);
		return NULL;
	}

	return hx;
}

void hxptr_free(hxptr_t *hx)
{
	if (!hx) {
		return;
	}

	sm_free(hx->memory);
	free(hx);
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


HxptrResult_t hxptr_sm_finalize(hxptr_t *hx)
{
	if (!hx) {
		return HX_ERR_NULL;
	}

	qsort(hx->memory->entries,
	      hx->memory->count,
	      sizeof(memory_entry_t),
	      sm_comp);

	hx->memory->finalized = 1;

	// more should be done here
	return sm_detect_overlaps(hx);
}

// NOTE; have all hxptr_* functions pass a ctx; this one doesn't
HxptrResult_t hxptr_hex_to_record(char *line, hxptr_record_t *record)
{
	if (!line || !record) {
		return HX_ERR_NULL;
	}

	return hex_line_to_record(line, record);
}

HxptrResult_t hxptr_append_record(hxptr_t *ctx, hxptr_record_t *record)
{
	uint32_t addr;
	HxptrResult_t res;

	if (!ctx || !record) {
		return HX_ERR_NULL;
	}

	// no records after eof
	if (ctx->eof) {
		return HX_ERR_BAD_FILE;
	}

	switch (record->type) {
	case IHEX_DATA:
		if (ctx->cur_type == IHEX_ESA) {
			addr = ((uint32_t) ctx->segment << 4) + record->address;
		} else if (ctx->cur_type == IHEX_ELA) {
			addr = ((uint32_t) ctx->linear << 16) | record->address;
		} else {
			addr = record->address;
		}

		res = sm_append_record(ctx->memory, record, addr);
		if (res != HX_OK) {
			return res;
		}

		ctx->total_bytes += record->byte_count;
		ctx->total_records += 1;
		break;

	case IHEX_EOF:
		// we already hit eof! no more!
		if (ctx->eof) {
			return HX_ERR_BAD_FILE;
		}

		// eof MUST fit the spec
		if (record->byte_count != 0 || record->address != 0x0000) {
			return HX_ERR_INV_RECORD;
		}

		ctx->eof = 1;
		break;

	case IHEX_ESA:
		if (record->byte_count != ADDR_COUNT) {
			return HX_ERR_INV_RECORD;
		}
		ctx->segment =
			(uint16_t) ((record->bytes[0] << 8) | record->bytes[1]);
		// ctx->segment = ((uint16_t) record->bytes[0] << 8) |
		// 	       (uint16_t) record->bytes[1];

		ctx->linear = 0;
		ctx->cur_type = IHEX_ESA;
		break;

	case IHEX_ELA:
		if (record->byte_count != ADDR_COUNT) {
			return HX_ERR_INV_RECORD;
		}

		ctx->linear = ((uint16_t) record->bytes[0] << 8) |
			      (uint16_t) record->bytes[1];

		ctx->segment = 0;
		ctx->cur_type = IHEX_ELA;
		break;

	case IHEX_SSA:
		if (record->byte_count != START_COUNT || record->address != 0) {
			return HX_ERR_INV_RECORD;
		}
		ctx->cs =
			(uint16_t) ((record->bytes[0] << 8) | record->bytes[1]);
		ctx->ip =
			(uint16_t) ((record->bytes[2] << 8) | record->bytes[3]);
		ctx->cur_type = IHEX_SSA;
		break;

	case IHEX_SLA:
		if (record->byte_count != START_COUNT || record->address != 0) {
			return HX_ERR_INV_RECORD;
		}

		ctx->cs =
			(uint16_t) ((record->bytes[0] << 8) | record->bytes[1]);
		ctx->ip =
			(uint16_t) ((record->bytes[2] << 8) | record->bytes[3]);
		ctx->cur_type = IHEX_SLA;
		break;

	default:
		return HX_ERR_INV_RECORD;
	}

	return HX_OK;
}
