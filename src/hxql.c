#include "hxql.h"
#include "hxsh.h"
#include <stdio.h>
#include <string.h>

hxptr_error_t hxql_execute_cmd(
	hxsh_t *sh, hxsh_req_t *req, hxql_result_t *result)
{
	// sparse_memory_t *mem;
	info_req_t info_req;
	info_result_t info_res = { 0 };

	switch (req->type) {
		case TOK_INFO:
			info_req = req->req.info_req;
			// mask of profile related values
			result->type = HXQL_RESULT_INFO;

			if (info_req.args & TOK_STATS) {
				info_res.stats = sh->hx->stats;
				info_res.args |= TOK_STATS;
			}
			if (info_req.args & INFO_PROF_MASK) {
				if (sh->hx->profile) {
					info_res.profile = sh->hx->profile;
				} else {
					info_res.profile = NULL;
				}
				info_res.args |= (info_req.args & INFO_PROF_MASK);
			}
			if (info_req.args & TOK_DIAG) {
				info_res.diagnostics = sh->hx->reports->entries;
				info_res.args |= TOK_DIAG;
			}

			result->code = HX_OK;
			memcpy(&result->data.info, &info_res, sizeof(info_res));

			break;

		default:
			fprintf(stderr, "not implemented\n");
			break;
	}

	return hxptr_ok();
}
