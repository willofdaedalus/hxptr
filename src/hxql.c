#include "hxql.h"
#include "hxsh.h"
#include <stdio.h>
#include <string.h>

static memory_entry_t *find_entry_by_addr(
	sparse_memory_t *memory, uint32_t addr)
{
	if (!memory || memory->count == 0) {
		return NULL;
	}

	uint32_t lo = 0;
	uint32_t hi = memory->count; // one past end

	while (lo < hi) {
		uint32_t mid = lo + (hi - lo) / 2;
		if (memory->entries[mid].address < addr) {
			lo = mid + 1;
		} else {
			hi = mid;
		}
	}

	if (lo >= memory->count) {
		return NULL;
	}

	return &memory->entries[lo];
}


hxptr_error_t hxql_execute_cmd(
	hxptr_t *hx, hxsh_req_t *req, hxql_result_t *result)
{
	info_result_t info_res = { 0 };
	which_result_t which_res = { 0 };
	list_result_t list_res = { 0 };

	// sparse_memory_t *memory;
	info_req_t info_req;
	which_req_t which_req;
	list_req_t list_req;
	memory_entry_t *start;
	mcu_profile_t *profile;
	profile_region_t region;
	profile_memory_t memory;
	uint8_t i;

	switch (req->type) {
		case TOK_INFO:
			info_req = req->req_type.info_req;
			result->type = HXQL_RES_INFO;

			if (info_req.args & TOK_STATS) {
				info_res.stats = hx->stats;
				info_res.args |= TOK_STATS;
			}

			// mask of profile related values
			if (info_req.args & INFO_PROF_MASK) {
				if (hx->profile) {
					info_res.profile = hx->profile;
				} else {
					info_res.profile = NULL;
				}
				// extract only those that were set in the request into the
				// result
				info_res.args |= (info_req.args & INFO_PROF_MASK);
			}
			if (info_req.args & TOK_DIAG) {
				info_res.diagnostics = hx->reports->entries;
				info_res.args |= TOK_DIAG;
			}

			result->code = HX_OK;
			memcpy(&result->data.info, &info_res, sizeof(info_res));
			break;

		case TOK_LIST:
			list_req = req->req_type.list_req;
			result->type = HXQL_RES_LIST;

			// subtract start_addr from addr_lowest
			start = find_entry_by_addr(hx->memory, list_req.start_addr);
			if (!start) {
				printf("not found\n");
				break;
			}

			list_res.start = start;
			list_res.count = list_req.count;

			result->code = HX_OK;
			memcpy(&result->data.list, &list_res, sizeof(list_res));
			break;

		case TOK_WHICH:
			i = 0;
			profile = hx->profile;
			which_req = req->req_type.which_req;
			which_res.cur_addr = which_req.cur_idx;
			result->type = HXQL_RES_WHICH;

			if (which_req.args & TOK_WH_REGION) {
				strncpy(
					which_res.region, "no region found", (MAX_OBJ_LEN * 2) - 1);

				while (i < profile->region_count) {
					region = profile->regions[i];

					if (which_req.cur_idx >= region.start &&
						which_req.cur_idx <= region.end) {
						strncpy(which_res.region, region.name,
							(MAX_OBJ_LEN * 2) - 1);
						break;
					}
					i++;
				}
			}

			i = 0;
			if (which_req.args & TOK_WH_MEMORY) {
				strncpy(
					which_res.memory, "no memory found", (MAX_OBJ_LEN * 2) - 1);

				while (i < profile->mem_count) {
					memory = profile->mem_blocks[i];

					if (which_req.cur_idx >= memory.start &&
						(which_req.cur_idx <= memory.start + memory.size)) {
						strncpy(which_res.memory, memory.type,
							(MAX_OBJ_LEN * 2) - 1);
						break;
					}
					i++;
				}
			}

			which_res.args = which_req.args;
			result->code = HX_OK;
			memcpy(&result->data.which, &which_res, sizeof(which_res));

			break;

		default:
			break;
	}

	return hxptr_ok();
}
