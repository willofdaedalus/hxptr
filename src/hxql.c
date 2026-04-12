#include "hxql.h"
#include "hxsh.h"
#include <stdio.h>
#include <string.h>

info_result_t info_res;
which_result_t which_res;
list_result_t list_res;

static memory_entry_t *find_entry_by_addr(sparse_memory_t *mem, uint32_t addr)
{
	if (!mem || mem->count == 0) {
		return NULL;
	}

	uint32_t lo = 0;
	uint32_t hi = mem->count; // one past end

	while (lo < hi) {
		uint32_t mid = lo + (hi - lo) / 2;
		if (mem->entries[mid].address < addr) {
			lo = mid + 1;
		} else {
			hi = mid;
		}
	}

	if (lo >= mem->count) {
		return NULL;
	}

	return &mem->entries[lo];
}


hxptr_error_t hxql_execute_cmd(
	hxptr_t *hx, hxsh_req_t *req, hxql_result_t *result)
{
	// sparse_memory_t *mem;
	info_req_t info_req;
	which_req_t which_req;
	list_req_t list_req;
	memory_entry_t *start;

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
			which_req = req->req_type.which_req;
			result->type = HXQL_RES_WHICH;

			if (which_req.args & TOK_WH_REGION) {
			}

			if (which_req.args & TOK_WH_MEMORY) {
			}

			result->code = HX_OK;
			memcpy(&result->data.which, &which_res, sizeof(which_res));
			break;

		default:
			fprintf(stderr, "not implemented\n");
			break;
	}

	return hxptr_ok();
}
