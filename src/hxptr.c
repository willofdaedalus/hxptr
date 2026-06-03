#include "hxptr_diagnostics.h"
#include "hxptr_memory.h"
#include "hxptr_profile.h"
#include "hxptr_record.h"

#include <stdarg.h>
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

static HxptrResult_t hxptr_append_record(
	hxptr_t *ctx, hxptr_record_t *record, hxptr_parse_state_t *state)
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
				addr = ((uint32_t) state->segment << 4) + record->address;
			} else if (state->cur_type == IHEX_ELA) {
				addr = ((uint32_t) state->linear << 16) | record->address;
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

			if (record->byte_count > ctx->stats->longest_record) {
				ctx->stats->longest_record = record->byte_count;
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
			state->segment = (uint16_t) ((record->data[0] << 8) | record->data[1]);
			// state->segment = ((uint16_t) record->bytes[0] << 8) |
			// 	       (uint16_t) record->bytes[1];

			state->linear = 0;
			state->cur_type = IHEX_ESA;
			break;

		case IHEX_ELA:
			if (record->byte_count != ADDR_COUNT) {
				return HX_ERR_INV_RECORD;
			}

			state->linear =
				((uint16_t) record->data[0] << 8) | (uint16_t) record->data[1];

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
	if (entry_a->linenum < entry_b->linenum) {
		return -1;
	} else {
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

hxptr_error_t hxptr_load_profile(hxptr_t *ctx, const char *path)
{
	HxptrResult_t res;
	hxptr_error_t err = { 0 };

	if (!ctx) {
		res = HX_ERR_NULL;
		return hxptr_err(res, hxptr_strerror(res));
	}

	ctx->profile = mcu_profile_load(path, &err);
	if (!ctx->profile) {
		return err;
	}

	return hxptr_ok();
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
		case HX_ERR_UNKN_DIRECTIVE:
			return "unknown directive; expected <directive> <args...>";
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

hxptr_error_t hxptr_load_file(hxptr_t *ctx, const char *path)
{
	FILE *fp;
	const char *filename;
	char buf[512];
	uint32_t linenum = 0;
	HxptrResult_t res;
	hxptr_record_t record = { 0 };
	hxptr_parse_state_t state = { 0 };
	hxptr_error_t err = { 0 };

	if (!ctx || !path) {
		res = HX_ERR_NULL;
		return hxptr_err(res, hxptr_strerror(res));
	}

	fp = fopen(path, "r");
	if (!fp) {
		res = HX_ERR_BAD_FILE;
		return hxptr_err(res, "could not access file at %s", path);
	}

	// get the actual filename
	filename = strrchr(path, '/');
	filename = filename ? filename + 1 : path;

	ctx->filename = strdup(filename);
	if (!ctx->filename) {
		res = HX_ERR_BAD_ALLOC;
		err = hxptr_err(
			res, "bad allocation in %s:%d, hxptr_load_file()", __FILE__, __LINE__);
		goto cleanup;
	}

	while (fgets(buf, sizeof(buf), fp) != NULL) {
		linenum++;

		res = hex_line_to_record(buf, &record);
		if (res != HX_OK) {
			switch (res) {
				case HX_ERR_INV_RECORD:
					err = hxptr_err(res, "line %d: record is invalid", linenum);
					break;
				case HX_ERR_NULL:
					err = hxptr_err(res, hxptr_strerror(res));
					break;
				case HX_ERR_NO_RECORD:
					err = hxptr_err(res, "line %d: no record found", linenum);
					break;
				case HX_ERR_UKN_TYPE:
					err = hxptr_err(res, "line %d: unknown record type '%d'",
						linenum, record.type);
					break;
				default:
					err = hxptr_err(res, hxptr_strerror(res));
					break;
			}

			goto cleanup;
		}

		record.linenum = linenum;
		res = hxptr_append_record(ctx, &record, &state);
		if (res != HX_OK) {
			switch (res) {
				case HX_ERR_INV_RECORD:
					err = hxptr_err(res, "line %d: record is invalid", linenum);
					break;
				case HX_ERR_NULL:
					err = hxptr_err(res, hxptr_strerror(res));
					break;
				case HX_ERR_BAD_FILE:
					err = hxptr_err(res,
						"line %d: one or more records after "
						"EOF is not allowed",
						linenum);
					break;
				case HX_ERR_UKN_TYPE:
					err = hxptr_err(res, "line %d: unknown record type '%d'",
						linenum, record.type);
					break;
				case HX_ERR_BAD_ALLOC:
					err = hxptr_err(res,
						"bad allocation in %s:%d, "
						"hxptr_append_record()",
						__FILE__, __LINE__);
					break;
				default:
					err = hxptr_err(res, hxptr_strerror(res));
					break;
			}
			goto cleanup;
		}
	}

	if (state.eof != 1) {
		res = HX_ERR_NO_EOF;
		err = hxptr_err(res, "line %d: no EOF in the hex file", linenum);
		goto cleanup;
	}

	fclose(fp);

	return hxptr_ok();

cleanup:
	fclose(fp);
	return err;
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

hxptr_error_t hxptr_finalize_memory(hxptr_t *hx)
{
	HxptrResult_t res;

	if (!hx) {
		res = HX_ERR_NULL;
		return hxptr_err(res, hxptr_strerror(res));
	}

	if (hx->memory->count < 1) {
		return hxptr_err(HX_OK, hxptr_strerror(HX_OK));
	}

	qsort(hx->memory->entries, hx->memory->count, sizeof(memory_entry_t), sm_comp);

	hx->memory->finalized = 1;

	// more should be done here
	res = sm_analyze(hx, GAP_THRESHOLD_DEFAULT);
	return hxptr_err(res, hxptr_strerror(res));
}

hxptr_error_t hxptr_err(HxptrResult_t res, const char *msg, ...)
{
	hxptr_error_t err = { 0 };
	err.code = res;

	va_list list;
	va_start(list, msg);
	vsnprintf(err.msg, sizeof(err.msg), msg, list);
	va_end(list);

	return err;
}

hxptr_error_t hxptr_ok(void)
{
	return (hxptr_error_t) { "42", HX_OK };
}

// hxptr_reset_profile
//
// replaces the current profile with a freshly loaded one.
// the old profile is freed only after the new one is confirmed good,
// so a failed load leaves the existing profile intact.
hxptr_error_t hxptr_reset_profile(hxptr_t *ctx, const char *path)
{
	hxptr_error_t err = { 0 };
	mcu_profile_t *new_profile;

	if (!ctx || !path) {
		return hxptr_err(HX_ERR_NULL, hxptr_strerror(HX_ERR_NULL));
	}

	new_profile = mcu_profile_load(path, &err);
	if (!new_profile) {
		return err;
	}

	// only swap after confirmed success; old profile survives a bad load
	mcu_profile_free(ctx->profile);
	ctx->profile = new_profile;

	return hxptr_ok();
}

// hxptr_reset_file
//
// replaces the current hex file state with a fresh load.
// all new allocations are validated before the old state is touched,
// so a failed load leaves the context in a consistent (if stale) state.
hxptr_error_t hxptr_reset_file(hxptr_t *ctx, const char *path)
{
	hxptr_error_t err = { 0 };
	sparse_memory_t *new_mem;
	reports_t *new_reports;
	hxptr_stats_t *new_stats;

	if (!ctx || !path) {
		return hxptr_err(HX_ERR_NULL, hxptr_strerror(HX_ERR_NULL));
	}

	new_mem = sm_init();
	if (!new_mem) {
		return hxptr_err(HX_ERR_BAD_ALLOC, hxptr_strerror(HX_ERR_BAD_ALLOC));
	}

	new_reports = reports_init();
	if (!new_reports) {
		sm_free(new_mem);
		return hxptr_err(HX_ERR_BAD_ALLOC, hxptr_strerror(HX_ERR_BAD_ALLOC));
	}

	new_stats = calloc(1, sizeof(hxptr_stats_t));
	if (!new_stats) {
		sm_free(new_mem);
		reports_free(new_reports);
		return hxptr_err(HX_ERR_BAD_ALLOC, hxptr_strerror(HX_ERR_BAD_ALLOC));
	}
	new_stats->addr_lowest = UINT32_MAX;

	// stash old state, swap in new stores
	sparse_memory_t *old_mem = ctx->memory;
	reports_t *old_reports = ctx->reports;
	hxptr_stats_t *old_stats = ctx->stats;
	char *old_filename = ctx->filename;

	ctx->memory = new_mem;
	ctx->reports = new_reports;
	ctx->stats = new_stats;
	ctx->filename = NULL;

	err = hxptr_load_file(ctx, path);
	if (err.code != HX_OK) {
		goto restore;
	}

	err = hxptr_finalize_memory(ctx);
	if (err.code != HX_OK) {
		goto restore;
	}

	// drop the old state
	sm_free(old_mem);
	reports_free(old_reports);
	free(old_stats);
	free(old_filename);

	return hxptr_ok();

restore:
	sm_free(ctx->memory);
	reports_free(ctx->reports);
	free(ctx->stats);
	free(ctx->filename);

	ctx->memory = old_mem;
	ctx->reports = old_reports;
	ctx->stats = old_stats;
	ctx->filename = old_filename;

	return err;
}
