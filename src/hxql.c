#include "hxql.h"
#include "hxsh.h"

hxptr_error_t hxql_execute_cmd(
	hxsh_t *sh, hxsh_req_t *req, hxql_result_t *result)
{
	info_req_t info;


	switch (req->type) {
	case TOK_INFO:
		info = req->req.info_req;
		if (info.args & TOK_STATS) {
			result->type = req->type;
			result->data.info.stats = sh->hx->stats;
		}
		break;
	}

	return hxptr_ok();
}
