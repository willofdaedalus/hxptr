#include "hxql.h"
#include "hxsh.h"
#include <stdio.h>

hxptr_error_t hxql_execute_cmd(
	hxsh_t *sh, hxsh_req_t *req, hxql_result_t *result)
{
	switch (req->type) {
		case TOK_INFO: {
			info_req_t info = req->req.info_req;
			if (info.args & TOK_STATS) {
				result->type = HXQL_RESULT_INFO;
				result->data.info.stats = sh->hx->stats;
			}
			break;
		}

		case TOK_LIST: {
			sparse_memory_t *mem = sh->hx->memory;
			uint32_t printed = 0;

			while (printed < HXQL_LIST_DEFAULT) {
				if (sh->cur_idx >= mem->count) {
					result->data.status.address = 0;
					snprintf(result->data.status.msg,
						sizeof(result->data.status.msg), "(end of entries)");
					result->type = HXQL_RESULT_STATUS;
					return hxptr_ok();
				}

				memory_entry_t *entry = &mem->entries[sh->cur_idx];
				sh->cur_idx++;

				if (!entry->is_data_record) {
					continue;
				}

				printed++;
			}

			// point result at the window we just walked
			result->type = HXQL_RESULT_ENTRIES;
			result->data.list.entries = &mem->entries[sh->cur_idx - printed];
			result->data.list.count = printed;
			break;
		}
	}

	return hxptr_ok();
}
