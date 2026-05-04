#include "hxsh_handlers.h"
#include "hxql.h"
#include "hxsh_tokenizer.h"
#include <stdio.h>
#include <string.h>

// handles the info command
// args: optional args passed via the shell
// out_data: populated info_req_t passed to the executor
hxptr_error_t info_handler(const char *args, void *out_data)
{
	if (!out_data) {
		return hxptr_err(HX_ERR_INVALID_ARG,
			"%s:%d: info_handler(): out_data is NULL", __FILE__, __LINE__);
	}

	char *list = (char *) args;
	int len;
	HxshTokenType_t tok_type;
	char arg[64];
	info_req_t req = { 0 };
	uint8_t count;

	token_map_t sub_cmds[] = {
		{ "regions", TOK_REGIONS },
		{ "memory",  TOK_MEMORY  },
		{ "stats",   TOK_STATS   },
		{ "diag",    TOK_DIAG    },
		{ "profile", TOK_PROFILE },
	};
	count = sizeof(sub_cmds) / sizeof(sub_cmds[0]);

	if (!args || !*args) {
		// no arguments: request everything
		req.args = INFO_ALL_ARGS;
	} else {
		while ((len = get_token(list, arg, sizeof(arg))) > 0) {
			tok_type = classify_token(arg, sub_cmds, count);

			switch (tok_type) {
				case TOK_REGIONS:
				case TOK_MEMORY:
				case TOK_STATS:
				case TOK_DIAG:
				case TOK_PROFILE:
					req.args |= tok_type;
					break;

				default:
					return hxptr_err(HX_ERR_UNKN_ARG,
						"unknown argument '%s' for info command", arg);
			}

			list += len;
		}
	}

	memcpy(out_data, &req, sizeof(req));
	return hxptr_ok();
}

// handles the which command
// which / which addr / which region / which memory
hxptr_error_t which_handler(hxsh_t *sh, const char *args, void *out_data)
{
	int len;
	char *list = (char *) args;
	HxshTokenType_t tok_type;
	which_req_t req = { 0 };
	char arg[64];

	token_map_t sub_cmds[] = {
		{ "address", TOK_WH_ADDR   },
		{ "addr",    TOK_WH_ADDR   },
		{ "region",  TOK_WH_REGION },
		{ "memory",  TOK_WH_MEMORY },
	};
	int count = sizeof(sub_cmds) / sizeof(sub_cmds[0]);

	if (!sh) {
		return hxptr_err(
			HX_ERR_NULL, "%s:%d: which_handler(): sh is NULL", __FILE__, __LINE__);
	}

	if (!args || !*args) {
		if (!sh->hx->profile) {
			return hxptr_err(HX_ERR_BAD_PROFILE,
				"unable to load region/memory information; no profile "
				"loaded. restart hxptr and load a profile with -p flag");
		}
		req.args = WHICH_MASK;
	} else {
		while ((len = get_token(list, arg, sizeof(arg))) > 0) {
			tok_type = classify_token(arg, sub_cmds, count);

			switch (tok_type) {
				case TOK_WH_MEMORY:
				case TOK_WH_REGION:
					if (!sh->hx->profile) {
						return hxptr_err(HX_ERR_BAD_PROFILE,
							"unable to load region/memory information; no "
							"profile loaded. restart hxptr and load a "
							"profile with -p flag");
					}
					__attribute__((fallthrough));
				case TOK_WH_ADDR:
					req.args |= tok_type;
					break;

				default:
					return hxptr_err(HX_ERR_UNKN_ARG,
						"unknown argument '%s' for which command", arg);
			}

			list += len;
		}
	}

	req.cur_idx = ADDR_OF(sh->cur_entry);
	memcpy(out_data, &req, sizeof(req));
	return hxptr_ok();
}

// handles the list command
// delegates address expression parsing to hxql_parse.
// validates the result and bridges to list_req_t for the engine.
hxptr_error_t list_handler(hxsh_t *sh, const char *args, void *out_data)
{
	hxql_parser_t parsed;
	list_req_t req = { 0 };

	if (!sh) {
		return hxptr_err(
			HX_ERR_NULL, "%s:%d: list_handler(): sh is NULL", __FILE__, __LINE__);
	}

	if (!out_data) {
		return hxptr_err(HX_ERR_INVALID_ARG,
			"%s:%d: list_handler(): out_data is NULL", __FILE__, __LINE__);
	}

	// no args: list from current, default count
	if (!args || !*args) {
		req.start_addr = ADDR_OF(sh->cur_entry);
		req.count = HXQL_LIST_DEFAULT;
		req.is_range = 0;
		req.adv_cur = 1;
		memcpy(out_data, &req, sizeof(req));

		return hxptr_ok();
	}

	parsed = hxql_parse(sh, args);
	if (parsed.err != HX_OK) {
		return hxptr_err(parsed.err, "list: invalid address expression");
	}

	if (parsed.has_range) {
		// range: start and end are both resolved indices
		req.start_addr = ADDR_OF(parsed.start_idx);
		req.end_addr = ADDR_OF(parsed.end_idx);
		req.count = (uint16_t) (parsed.end_idx - parsed.start_idx + 1);
		req.is_range = 1;
		printf("%d count\n", req.count);
	} else {
		// single expression: start_idx holds the resolved base.
		// the sign of the original offset determines direction and
		// whether current is included, but hxql_parse has already
		// resolved everything to an absolute index, so we just use it.
		req.start_addr = ADDR_OF(parsed.start_idx);
		req.count = parsed.count;
		req.is_range = 0;
		req.adv_cur = 1;
	}

	memcpy(out_data, &req, sizeof(req));
	return hxptr_ok();
}

// handles the goto command
// delegates address expression parsing to hxql_parse.
// ranges are not valid for goto.
hxptr_error_t goto_handler(hxsh_t *sh, const char *args, void *out_data)
{
	hxql_parser_t parsed;
	goto_req_t req = { 0 };

	if (!sh) {
		return hxptr_err(
			HX_ERR_NULL, "%s:%d: goto_handler(): sh is NULL", __FILE__, __LINE__);
	}

	if (!out_data) {
		return hxptr_err(HX_ERR_INVALID_ARG,
			"%s:%d: goto_handler(): out_data is NULL", __FILE__, __LINE__);
	}

	// goto with no args is invalid
	if (!args || !*args) {
		return hxptr_err(
			HX_ERR_BAD_TOKEN, "goto: expected an address, region, or memory name");
	}

	parsed = hxql_parse(sh, args);
	if (parsed.err != HX_OK) {
		return hxptr_err(parsed.err, "goto: invalid address expression");
	}

	// ranges are not valid for goto
	if (parsed.has_range) {
		return hxptr_err(
			HX_ERR_BAD_TOKEN, "goto: ranges are not valid, use an explicit address");
	}

	req.addr = ADDR_OF(parsed.start_idx);
	memcpy(out_data, &req, sizeof(req));
	return hxptr_ok();
}
