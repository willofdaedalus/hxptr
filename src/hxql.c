#include "hxql.h"
#include "hxsh.h"
#include <stdio.h>
#include <string.h>

hxptr_error_t hxql_execute_cmd(
	hxsh_t *sh, hxsh_req_t *req, hxql_result_t *result)
{
	// sparse_memory_t *mem;
	uint8_t info_prof = 0;
	info_req_t info_req;
	info_result_t info_res = { 0 };

	switch (req->type) {
		case TOK_INFO:
			info_req = req->req.info_req;
			// mask of profile related values
			info_prof |= (TOK_REGIONS | TOK_PROFILE | TOK_MEMORY);
			result->type = HXQL_RESULT_INFO;

			if (info_req.args & TOK_STATS) {
				info_res.stats = sh->hx->stats;
				info_res.args |= TOK_STATS;
			}
			if (info_req.args & info_prof) {
				if (sh->hx->profile) {
					info_res.profile = sh->hx->profile;
				} else {
					info_res.profile = NULL;
				}
				info_res.args |= (info_req.args & info_prof);
			}
			if (info_req.args & TOK_DIAG) {
				info_res.diagnostics = sh->hx->reports->entries;
				info_res.args |= TOK_DIAG;
			}

			result->code = HX_OK;
			memcpy(&result->data.info, &info_res, sizeof(info_res));

			break;

			// case TOK_LIST:
			// 	mem = sh->hx->memory;
			// 	uint32_t printed = 0;
			//
			// 	while (printed < HXQL_LIST_DEFAULT) {
			// 		if (sh->cur_idx >= mem->count) {
			// 			result->data.status.address = 0;
			// 			snprintf(result->data.status.msg,
			// 				sizeof(result->data.status.msg), "(end of
			// entries)"); 			result->type = HXQL_RESULT_STATUS;
			// return hxptr_ok();
			// 		}
			//
			// 		memory_entry_t *entry = &mem->entries[sh->cur_idx];
			// 		sh->cur_idx++;
			//
			// 		if (!entry->is_data_record) {
			// 			continue;
			// 		}
			//
			// 		printed++;
			// 	}
			//
			// 	// point result at the window we just walked
			// 	result->type = HXQL_RESULT_ENTRIES;
			// 	result->data.list.entries = &mem->entries[sh->cur_idx -
			// printed]; 	result->data.list.count = printed; 	break;


		default:
			fprintf(stderr, "not implemented\n");
			break;
	}

	return hxptr_ok();
}
