#include "hxptr.h"
#include "hxptr_diagnostics.h"
#include "hxptr_memory.h"
#include "hxptr_profile.h"
#include "hxptr_record.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static HxptrResult_t hxptr_append_record(hxptr_t *ctx, hxptr_record_t *record)
{
	static uint8_t lowest_set = 0;
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

		// track highest and lowest addr
		if (!lowest_set) {
			ctx->stats->addr_lowest = addr;
			lowest_set = 1;
		}

		if (addr > ctx->stats->addr_highest) {
			ctx->stats->addr_highest = addr;
		}


		ctx->stats->total_bytes += record->byte_count;
		ctx->stats->total_records += 1;
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
			(uint16_t) ((record->data[0] << 8) | record->data[1]);
		// ctx->segment = ((uint16_t) record->bytes[0] << 8) |
		// 	       (uint16_t) record->bytes[1];

		ctx->linear = 0;
		ctx->cur_type = IHEX_ESA;
		break;

	case IHEX_ELA:
		if (record->byte_count != ADDR_COUNT) {
			return HX_ERR_INV_RECORD;
		}

		ctx->linear = ((uint16_t) record->data[0] << 8) |
			      (uint16_t) record->data[1];

		ctx->segment = 0;
		ctx->cur_type = IHEX_ELA;
		break;

	case IHEX_SSA:
		if (record->byte_count != START_COUNT || record->address != 0) {
			return HX_ERR_INV_RECORD;
		}
		ctx->cs = (uint16_t) ((record->data[0] << 8) | record->data[1]);
		ctx->ip = (uint16_t) ((record->data[2] << 8) | record->data[3]);
		ctx->cur_type = IHEX_SSA;
		break;

	case IHEX_SLA:
		if (record->byte_count != START_COUNT || record->address != 0) {
			return HX_ERR_INV_RECORD;
		}

		ctx->cs = (uint16_t) ((record->data[0] << 8) | record->data[1]);
		ctx->ip = (uint16_t) ((record->data[2] << 8) | record->data[3]);
		ctx->cur_type = IHEX_SLA;
		break;

	default:
		return HX_ERR_INV_RECORD;
	}

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
	if (entry_a->line_num < entry_b->line_num) {
		return -1;
	}
	if (entry_a->line_num > entry_b->line_num) {
		return 1;
	}

	return 0;
}


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

	hx->stats = calloc(1, sizeof(hxptr_stats_t));
	if (!hx->stats) {
		free(hx->memory);
		free(hx);
		return NULL;
	}

	hx->reports = reports_init();
	if (!hx->reports) {
		free(hx->memory);
		free(hx->stats);
		free(hx);
		return NULL;
	}

	hx->profile = profile_load();
	if (!hx->profile) {
		free(hx->reports);
		free(hx->memory);
		free(hx->stats);
		free(hx);
		return NULL;
	}

	return hx;
}

HxptrResult_t hxptr_load_file(hxptr_t *ctx, const char *path)
{
	FILE *fp;
	const char *filename;
	char buf[512];
	uint32_t linenum = 1;
	HxptrResult_t res;
	hxptr_record_t record = {0};

	if (!ctx || !path) {
		return HX_ERR_NULL;
	}

	fp = fopen(path, "r");
	if (!fp) {
		perror("fopen");
		return HX_ERR_BAD_FILE;
	}

	// get the actual filename name
	filename = strrchr(path, '/');
	filename = filename ? filename + 1 : path;

	ctx->filename = strdup(filename);
	if (!ctx->filename) {
		printf("bad alloc\n");
		return HX_ERR_BAD_ALLOC;
	}

	while (fgets(buf, sizeof(buf), fp) != NULL) {
		res = hex_line_to_record(buf, &record);
		if (res != HX_OK) {
			printf("%d\n", res);
			return res;
		}

		record.linenum = linenum;
		res = hxptr_append_record(ctx, &record);
		if (res != HX_OK) {
			printf("%d\n", res);
			goto cleanup;
		}

		linenum++;
	}

	fclose(fp);

	return HX_OK;

cleanup:
	fclose(fp);
	return res;
}

void hxptr_free(hxptr_t *hx)
{
	if (!hx) {
		return;
	}

	free(hx->stats);
	free(hx->filename);
	mcu_profile_free(hx->profile);
	sm_free(hx->memory);
	reports_free(hx->reports);
	free(hx);
}

HxptrResult_t hxptr_finalize_memory(hxptr_t *hx)
{
	if (!hx) {
		return HX_ERR_NULL;
	}

	if (hx->memory->count < 1) {
		return HX_OK;
	}

	qsort(hx->memory->entries,
	      hx->memory->count,
	      sizeof(memory_entry_t),
	      sm_comp);

	hx->memory->finalized = 1;

	// more should be done here
	return sm_analyze(hx, 128);
}

HxptrResult_t hxptr_print_stats(hxptr_t *hx)
{
	printf("hxptr stats for '%s'\n", hx->filename);
	printf("read %u total bytes\n", hx->stats->total_bytes);
	printf("read %u total records\n", hx->stats->total_records);
	printf("read %u total overlaps\n", hx->stats->total_overlaps);
	printf("read %u total gaps\n", hx->stats->total_gaps);
	printf("address starts at 0x%06x\n", hx->stats->addr_lowest);
	printf("address ends at 0x%06x\n", hx->stats->addr_highest);

	return HX_OK;
}
