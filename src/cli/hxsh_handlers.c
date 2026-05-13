#include "hxsh_handlers.h"
#include "hxql.h"
#include "hxsh_tokenizer.h"
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

hxptr_error_t find_handler(hxsh_t *sh, const char *args, void *out_data)
{
	char tok[64];
	int len;
	HxshTokenType_t tok_type;
	search_req_t req = { 0 };
	hxql_parser_t parsed = { 0 };
	char *cursor = (char *) args;
	const char *hex;
	size_t hex_len;

	if (!sh) {
		return hxptr_err(
			HX_ERR_NULL, "%s:%d: find_handler(): sh is NULL", __FILE__, __LINE__);
	}
	if (!out_data) {
		return hxptr_err(HX_ERR_INVALID_ARG,
			"%s:%d: find_handler(): out_data is NULL", __FILE__, __LINE__);
	}
	if (!args || !*args) {
		return hxptr_err(
			HX_ERR_BAD_TOKEN, "find: expected a hex pattern, 'next', or 'prev'");
	}

	len = get_token(cursor, tok, sizeof(tok));
	if (len <= 0) {
		return hxptr_err(
			HX_ERR_BAD_TOKEN, "find: expected a hex pattern, 'next', or 'prev'");
	}

	if (strcmp(tok, "next") == 0 || strcmp(tok, "prev") == 0) {
		if (!sh->search.active) {
			return hxptr_err(HX_ERR_BAD_TOKEN,
				"find: no active search. run 'search <pattern>' first");
		}
		req.direction = (strcmp(tok, "next") == 0) ? SEARCH_NEXT : SEARCH_PREV;
		req.pattern_len = sh->search.pattern_len;
		req.range_start_idx = sh->search.range_start_idx;
		req.range_end_idx = sh->search.range_end_idx;
		memcpy(req.pattern, sh->search.pattern, sh->search.pattern_len);
		memcpy(out_data, &req, sizeof(req));
		return hxptr_ok();
	}

	cursor += len;
	tok_type = classify_token(tok, NULL, 0);

	if (tok_type != TOK_HEX && tok_type != TOK_BARE_HEX && tok_type != TOK_DEC) {
		return hxptr_err(HX_ERR_BAD_TOKEN,
			"find: pattern must be a hex value (e.g. 0xDEADBEEF or DEADBEEF)");
	}

	hex = tok;
	if (hex[0] == '0' && (hex[1] == 'x' || hex[1] == 'X')) {
		hex += 2;
	}

	hex_len = strlen(hex);
	if (hex_len == 0 || hex_len > (HXSH_SEARCH_MAX_PATTERN * 2)) {
		return hxptr_err(HX_ERR_BAD_TOKEN, "find: pattern too long (max %d bytes)",
			HXSH_SEARCH_MAX_PATTERN);
	}
	if (hex_len & 1) {
		return hxptr_err(
			HX_ERR_BAD_TOKEN, "find: pattern has an odd number of hex digits");
	}

	req.pattern_len = (uint8_t) (hex_len / 2);
	for (uint8_t i = 0; i < req.pattern_len; i++) {
		// walk the string in pairs
		char byte_str[3] = { hex[i * 2], hex[i * 2 + 1], '\0' };
		req.pattern[i] = (uint8_t) strtoul(byte_str, NULL, 16);
	}

	req.direction = SEARCH_FRESH;

	while (isspace((unsigned char) *cursor)) {
		cursor++;
	}

	if (*cursor != '\0') {
		parsed = hxql_parse(sh, cursor);
		if (parsed.err != HX_OK) {
			return hxptr_err(parsed.err, "find: bad address expression");
		}
		req.range_start_idx = (uint32_t) parsed.start_idx;
		req.range_end_idx = parsed.has_range
			? (uint32_t) parsed.end_idx
			: (uint32_t) (sh->hx->memory->count - 1);
	} else {
		// no address given: full range, scan starts from cur_entry in engine
		req.range_start_idx = 0;
		req.range_end_idx = sh->hx->memory->count - 1;
	}

	memcpy(out_data, &req, sizeof(req));
	return hxptr_ok();
}

// handles the info command
// args: optional args passed via the shell
// out_data: populated info_req_t passed to the executor
hxptr_error_t info_handler(hxsh_t *sh, const char *args, void *out_data)
{
	UNUSED(sh);
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
		req.count = (DIFF_COUNT(sh->cur_entry) >= 15) ? HXQL_LIST_DEFAULT
													  : DIFF_COUNT(sh->cur_entry);
		req.is_range = 0;
		req.adv_cur = 1;
		memcpy(out_data, &req, sizeof(req));

		return hxptr_ok();
	}

	parsed = hxql_parse(sh, args);
	if (parsed.err != HX_OK) {
		return hxptr_err(parsed.err,
			"list: region/memory is outside the bounds of the hex map. skipping");
	}

	if (parsed.has_range) {
		// range: start and end are both resolved indices
		req.start_addr = ADDR_OF(parsed.start_idx);
		req.end_addr = ADDR_OF(parsed.end_idx);
		req.count = (uint16_t) (parsed.end_idx - parsed.start_idx + 1);
		req.is_range = 1;
	} else {
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
		return hxptr_err(parsed.err,
			"goto: region/memory is outside the bounds of the hex map. skipping");
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
