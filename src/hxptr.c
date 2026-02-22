#include "hxptr.h"
#include "hxptr_diagnostics.h"
#include "hxptr_memory.h"
#include "hxptr_profile.h"
#include "hxptr_record.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
	uint32_t segment;
	uint32_t linear;
	uint32_t code_segment;
	uint32_t instr_pointer;
	uint8_t cur_type;
	uint8_t eof;
} hxptr_parse_state_t;

static HxptrResult_t hxptr_append_record(hxptr_t *ctx,
					 hxptr_record_t *record,
					 hxptr_parse_state_t *state)
{
	uint32_t addr;
	HxptrResult_t res;

	if (!ctx || !record || !state) {
		return HX_ERR_NULL;
	}

	// no records after eof
	if (state->eof) {
		return HX_ERR_BAD_FILE;
	}

	switch (record->type) {
	case IHEX_DATA:
		if (state->cur_type == IHEX_ESA) {
			addr = ((uint32_t) state->segment << 4) +
			       record->address;
		} else if (state->cur_type == IHEX_ELA) {
			addr = ((uint32_t) state->linear << 16) |
			       record->address;
		} else {
			addr = record->address;
		}

		res = sm_append_record(ctx->memory, record, addr);
		if (res != HX_OK) {
			return res;
		}

		// track highest and lowest addr
		if (addr < ctx->stats->addr_lowest) {
			ctx->stats->addr_lowest = addr;
		}

		if (addr > ctx->stats->addr_highest) {
			ctx->stats->addr_highest = addr;
		}


		ctx->stats->total_bytes += record->byte_count;
		ctx->stats->total_records += 1;
		break;

	case IHEX_EOF:
		// we already hit eof! no more!
		if (state->eof) {
			return HX_ERR_BAD_FILE;
		}

		// eof MUST fit the spec
		if (record->byte_count != 0 || record->address != 0x0000) {
			return HX_ERR_INV_RECORD;
		}

		state->eof = 1;
		break;

	case IHEX_ESA:
		if (record->byte_count != ADDR_COUNT) {
			return HX_ERR_INV_RECORD;
		}
		state->segment =
			(uint16_t) ((record->data[0] << 8) | record->data[1]);
		// state->segment = ((uint16_t) record->bytes[0] << 8) |
		// 	       (uint16_t) record->bytes[1];

		state->linear = 0;
		state->cur_type = IHEX_ESA;
		break;

	case IHEX_ELA:
		if (record->byte_count != ADDR_COUNT) {
			return HX_ERR_INV_RECORD;
		}

		state->linear = ((uint16_t) record->data[0] << 8) |
				(uint16_t) record->data[1];

		state->segment = 0;
		state->cur_type = IHEX_ELA;
		break;

	case IHEX_SSA:
		if (record->byte_count != START_COUNT || record->address != 0) {
			return HX_ERR_INV_RECORD;
		}
		state->code_segment =
			(uint16_t) ((record->data[0] << 8) | record->data[1]);
		state->instr_pointer =
			(uint16_t) ((record->data[2] << 8) | record->data[3]);
		state->cur_type = IHEX_SSA;
		break;

	case IHEX_SLA:
		if (record->byte_count != START_COUNT || record->address != 0) {
			return HX_ERR_INV_RECORD;
		}

		state->code_segment =
			(uint16_t) ((record->data[0] << 8) | record->data[1]);
		state->instr_pointer =
			(uint16_t) ((record->data[2] << 8) | record->data[3]);
		state->cur_type = IHEX_SLA;
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
		goto cleanup_hx;
	}

	hx->stats = calloc(1, sizeof(hxptr_stats_t));
	if (!hx->stats) {
		goto cleanup_mem;
	}

	hx->reports = reports_init();
	if (!hx->reports) {
		goto cleanup_stats;
	}
	hx->stats->addr_lowest = UINT32_MAX;

	return hx;

cleanup_stats:
	free(hx->stats);
cleanup_mem:
	free(hx->memory);
cleanup_hx:
	free(hx);

	return NULL;
}

HxptrResult_t hxptr_load_profile(hxptr_t *ctx, const char *path, int *line_num)
{
	HxptrResult_t res;
	if (!ctx) {
		return HX_ERR_NULL;
	}

	ctx->profile = mcu_profile_load(path, &res, line_num);
	if (!ctx->profile) {
		return res;
	}

	return HX_OK;
}

const char *hxptr_strerror(HxptrResult_t err)
{
	switch (err) {
	case HX_OK:
		return "ok";
	case HX_ERR_INV_RECORD:
		return "invalid record";
	case HX_ERR_NULL:
		return "null argument";
	case HX_ERR_NO_RECORD:
		return "no record found";
	case HX_ERR_UKN_TYPE:
		return "unknown record type";
	case HX_ERR_BAD_CHKSM:
		return "bad checksum";
	case HX_ERR_BAD_APPEND:
		return "bad append";
	case HX_ERR_BAD_FILE:
		return "bad or unreadable file";
	case HX_ERR_BAD_ALLOC:
		return "allocation failed";
	case HX_ERR_NOT_FINALIZED:
		return "memory not finalized";
	case HX_ERR_PARSE:
		return "parsing issues";
	case HX_ERR_OVERFLOW:
		return "too many memory/regions";
	case HX_ERR_BAD_CONFIG:
		return "unexpected symbol after directives";
	case HX_ERR_PARSE_REGION:
		return "bad region definition; expected region <name> <start> "
		       "<end> [protected]";
	case HX_ERR_PARSE_MEMORY:
		return "bad memory definition; expected memory <type> <start> "
		       "<end>";
	case HX_ERR_UNKN_MEM_TYPE:
		return "bad memory type; options are flash, ram, rom, eeprom";
	case HX_ERR_PARSE_DEETS:
		return "bad details; expected mcu <name> <architecture>";
	case HX_ERR_BAD_PROFILE:
		return "couldn't open profile description file; double check "
		       "path";
	case HX_ERR_NO_EOF:
		return "no eof in hex file. exiting";
	default:
		return "unknown error";
	}
}


HxptrResult_t hxptr_load_file(hxptr_t *ctx, const char *path)
{
	FILE *fp;
	const char *filename;
	char buf[512];
	uint32_t linenum = 1;
	HxptrResult_t res;
	hxptr_record_t record = {0};
	hxptr_parse_state_t state = {0};

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
		res = HX_ERR_BAD_ALLOC;
		goto cleanup;
	}

	while (fgets(buf, sizeof(buf), fp) != NULL) {
		res = hex_line_to_record(buf, &record);
		if (res != HX_OK) {
			if (res == HX_ERR_INV_RECORD) {
				fprintf(stderr,
					"line %d: byte count %d doesn't match "
					"actual data bytes\n",
					linenum,
					record.byte_count);
			}
			goto cleanup;
		}

		record.linenum = linenum;
		res = hxptr_append_record(ctx, &record, &state);
		if (res != HX_OK) {
			goto cleanup;
		}

		linenum++;
	}

	if (state.eof != 1) {
		res = HX_ERR_NO_EOF;
		goto cleanup;
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
	return sm_analyze(hx, GAP_THRESHOLD_DEFAULT);
}

HxptrResult_t hxptr_print_stats(hxptr_t *hx)
{
	printf("hxptr stats for '%s'\n", hx->filename);
	printf("read %u total bytes\n", hx->stats->total_bytes);
	printf("read %u total records\n", hx->stats->total_records);
	printf("read %u total overlaps\n", hx->stats->total_overlaps);
	printf("read %u total gaps\n", hx->stats->total_gaps);
	printf("address starts at 0x%08x\n", hx->stats->addr_lowest);
	printf("address ends at 0x%08x\n", hx->stats->addr_highest);

	return HX_OK;
}
