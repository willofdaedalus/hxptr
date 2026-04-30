#include "hxsh_handlers.h"
#include "hxsh_tokenizer.h"
#include <string.h>


// handles the info command
// args: the optional args passed via the shell for the command
// out_data: the data that will be passed to the executor
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

	// copy the data only if parsing succeeded
	memcpy(out_data, &req, sizeof(req));

	return hxptr_ok();
}

hxptr_error_t which_handler(hxsh_t *sh, const char *args, void *out_data)
{
	// which address
	// which region
	// which memory
	// which
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
		} else {
			// no arguments: request everything
			req.args = WHICH_MASK;
		}
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

	// store the current idx for the query engine
	req.cur_idx = CUR_ADDR(sh);
	memcpy(out_data, &req, sizeof(req));

	return hxptr_ok();
}

hxptr_error_t list_handler(hxsh_t *sh, const char *args, void *out_data)
{
	list_req_t req = { 0 };
	char arg[64];
	char *list = (char *) args;
	int len;
	HxshTokenType_t tok_type;

	token_map_t sub_cmds[] = {
		{ ".", TOK_DOT   },
		{ "+", TOK_PLUS  },
		{ "-", TOK_MINUS },
		{ ":", TOK_COLON },
	};
	int sub_count = sizeof(sub_cmds) / sizeof(sub_cmds[0]);

	if (!sh) {
		return hxptr_err(
			HX_ERR_NULL, "%s:%d: list_handler(): sh is NULL", __FILE__, __LINE__);
	}

	if (!out_data) {
		return hxptr_err(HX_ERR_INVALID_ARG,
			"%s:%d: list_handler(): out_data is NULL", __FILE__, __LINE__);
	}

	// no args
	if (!args || !*args) {
		req.start_addr = CUR_ADDR(sh);
		req.count = HXQL_LIST_DEFAULT;
		req.is_range = 0;
		req.adv_cur = 1;
		memcpy(out_data, &req, sizeof(req));
		return hxptr_ok();
	}

	// token 1
	len = get_token(list, arg, sizeof(arg));
	if (len <= 0) {
		req.start_addr = CUR_ADDR(sh);
		req.count = HXQL_LIST_DEFAULT;
		req.is_range = 0;
		memcpy(out_data, &req, sizeof(req));
		return hxptr_ok();
	}
	list += len;

	tok_type = classify_token(arg, sub_cmds, sub_count);

	// case: list + count  or  list - count
	// no explicit base; uses cur_idx
	if (tok_type == TOK_PLUS || tok_type == TOK_MINUS) {
		int8_t sign = (tok_type == TOK_PLUS) ? +1 : -1;

		char num[64];
		len = get_token(list, num, sizeof(num));
		if (len <= 0) {
			return hxptr_err(
				HX_ERR_BAD_TOKEN, "list: expected a count after '%s'", arg);
		}
		list += len;

		HxshTokenType_t num_type = classify_token(num, NULL, 0);
		uint32_t count;
		if (resolve_num(num_type, num, &count) != 0) {
			return hxptr_err(
				HX_ERR_BAD_TOKEN, "list: '%s' is not a valid count", num);
		}

		if (count == 0) {
			return hxptr_err(HX_ERR_BAD_TOKEN, "list: count must be greater than 0");
		}

		if (sign == -1 && count > CUR_ADDR(sh)) {
			return hxptr_err(HX_ERR_BAD_TOKEN,
				"list: count %u would underflow current address 0x%08x", count,
				CUR_ADDR(sh));
		}

		char extra[64];
		if (get_token(list, extra, sizeof(extra)) > 0) {
			return hxptr_err(HX_ERR_BAD_TOKEN, "list: unexpected token '%s'", extra);
		}

		if (tok_type == TOK_PLUS) {
			req.adv_cur = 1;
		}

		req.start_addr = (sign == +1) ? sh->cur_entry : sh->cur_entry - count;
		req.count = (uint16_t) count;
		req.is_range = 0;
		memcpy(out_data, &req, sizeof(req));
		return hxptr_ok();
	}

	// token 1 must now be addr (TOK_HEX or TOK_DOT)
	if (tok_type != TOK_HEX && tok_type != TOK_DOT) {
		return hxptr_err(HX_ERR_BAD_TOKEN,
			"list: unexpected token '%s', expected address, '.', '+' or '-'", arg);
	}

	uint32_t base;
	if (resolve_base(sh, tok_type, arg, &base) != 0) {
		return hxptr_err(
			HX_ERR_BAD_TOKEN, "list: could not resolve address '%s'", arg);
	}

	// peek token 2
	char tok2[64];
	len = get_token(list, tok2, sizeof(tok2));

	// case: list addr  or  list .  -- nothing follows
	if (len <= 0) {
		req.start_addr = base;
		req.count = HXQL_LIST_DEFAULT;
		req.is_range = 0;
		memcpy(out_data, &req, sizeof(req));
		return hxptr_ok();
	}
	list += len;

	HxshTokenType_t tok2_type = classify_token(tok2, sub_cmds, sub_count);

	// case: list addr + count  or  list . + count
	//       list addr - count  or  list . - count
	if (tok2_type == TOK_PLUS || tok2_type == TOK_MINUS) {
		int8_t sign = (tok2_type == TOK_PLUS) ? +1 : -1;

		char tok3[64];
		len = get_token(list, tok3, sizeof(tok3));
		if (len <= 0) {
			return hxptr_err(
				HX_ERR_BAD_TOKEN, "list: expected a count after '%s'", tok2);
		}
		list += len;

		HxshTokenType_t tok3_type = classify_token(tok3, NULL, 0);
		uint32_t count;
		if (resolve_num(tok3_type, tok3, &count) != 0) {
			return hxptr_err(
				HX_ERR_BAD_TOKEN, "list: '%s' is not a valid count", tok3);
		}

		if (count == 0) {
			return hxptr_err(HX_ERR_BAD_TOKEN, "list: count must be greater than 0");
		}

		if (sign == -1 && count > base) {
			return hxptr_err(HX_ERR_BAD_TOKEN,
				"list: count %u would underflow address 0x%08x", count, base);
		}

		char extra[64];
		if (get_token(list, extra, sizeof(extra)) > 0) {
			return hxptr_err(HX_ERR_BAD_TOKEN, "list: unexpected token '%s'", extra);
		}

		if (tok_type == TOK_DOT && tok2_type == TOK_PLUS) {
			req.adv_cur = 1;
		}

		req.start_addr = (sign == +1) ? base : base - count;
		req.count = (uint16_t) count;
		req.is_range = 0;
		memcpy(out_data, &req, sizeof(req));
		return hxptr_ok();
	}

	// case: list addr : addrb  or  list . : addr  or  list addr : .
	if (tok2_type == TOK_COLON) {
		char tok3[64];
		len = get_token(list, tok3, sizeof(tok3));
		if (len <= 0) {
			return hxptr_err(
				HX_ERR_BAD_TOKEN, "list: expected an address after ':'");
		}
		list += len;

		HxshTokenType_t tok3_type = classify_token(tok3, sub_cmds, sub_count);

		// list . : . -- invalid
		if (tok_type == TOK_DOT && tok3_type == TOK_DOT) {
			return hxptr_err(HX_ERR_BAD_TOKEN,
				"list: '. : .' is invalid, end address must be explicit");
		}

		// list addr : . -- cur must be > addr
		if (tok3_type == TOK_DOT && CUR_ADDR(sh) <= base) {
			return hxptr_err(HX_ERR_BAD_TOKEN,
				"list: current position 0x%08x must be greater than "
				"start address 0x%08x for ': .'",
				CUR_ADDR(sh), base);
		}

		uint32_t end;
		if (resolve_base(sh, tok3_type, tok3, &end) != 0) {
			return hxptr_err(
				HX_ERR_BAD_TOKEN, "list: could not resolve end address '%s'", tok3);
		}

		if (end <= base) {
			return hxptr_err(HX_ERR_BAD_TOKEN,
				"list: end address 0x%08x must be greater than "
				"start address 0x%08x",
				end, base);
		}

		char extra[64];
		if (get_token(list, extra, sizeof(extra)) > 0) {
			return hxptr_err(
				HX_ERR_BAD_TOKEN, "list: unexpected token '%s' after range", extra);
		}

		req.start_addr = base;
		req.end_addr = end;
		req.is_range = 1;
		memcpy(out_data, &req, sizeof(req));
		return hxptr_ok();
	}

	return hxptr_err(
		HX_ERR_BAD_TOKEN, "list: unexpected token '%s' after address", tok2);
}

// offset by byte count or groups of 16
hxptr_error_t goto_handler(hxsh_t *sh, const char *args, void *out_data)
{
	goto_req_t req = { 0 };
	char arg[64];
	char *list = (char *) args;
	int len;
	HxshTokenType_t tok_type;

	token_map_t sub_cmds[] = {
		{ ".",      TOK_DOT     },
		{ "+",      TOK_PLUS    },
		{ "-",      TOK_MINUS   },
		{ ":",      TOK_COLON   },
		{ "region", TOK_REGIONS },
		{ "memory", TOK_MEMORY  },
	};
	int count = sizeof(sub_cmds) / sizeof(sub_cmds[0]);

	if (!sh) {
		return hxptr_err(
			HX_ERR_NULL, "%s:%d: goto_handler(): sh is NULL", __FILE__, __LINE__);
	}

	if (!out_data) {
		return hxptr_err(HX_ERR_INVALID_ARG,
			"%s:%d: goto_handler(): out_data is NULL", __FILE__, __LINE__);
	}

	// goto with no args is invalid needs a destination
	if (!args || !*args) {
		return hxptr_err(
			HX_ERR_BAD_TOKEN, "goto: expected an address, region, or memory name");
	}

	// token 1
	len = get_token(list, arg, sizeof(arg));
	if (len <= 0) {
		return hxptr_err(HX_ERR_BAD_TOKEN, "goto: failed to read argument");
	}
	list += len;

	tok_type = classify_token(arg, sub_cmds, count);

	// case: goto region <name> or goto memory <name>
	if (tok_type == TOK_REGIONS || tok_type == TOK_MEMORY) {
		if (!sh->hx->profile) {
			return hxptr_err(HX_ERR_BAD_PROFILE,
				"no profile loaded; unable to access memory/region "
				"information. restart hxptr with -p flag");
		}

		char name[64];
		len = get_token(list, name, sizeof(name));
		if (len <= 0) {
			return hxptr_err(HX_ERR_BAD_TOKEN,
				"goto: expected a region/memory name after '%s'", arg);
		}
		list += len;

		// no trailing tokens allowed
		char extra[64];
		if (get_token(list, extra, sizeof(extra)) > 0) {
			return hxptr_err(HX_ERR_BAD_TOKEN, "goto: unexpected token '%s'", extra);
		}

		req.is_named = 1;
		req.named_type = tok_type;
		strncpy(req.name, name, sizeof(req.name) - 1);
		memcpy(out_data, &req, sizeof(req));
		return hxptr_ok();
	}

	// case: goto 0x08000000; plain absolute address
	if (tok_type == TOK_HEX) {
		uint32_t addr;
		if (resolve_base(sh, tok_type, arg, &addr) != 0) {
			return hxptr_err(
				HX_ERR_BAD_TOKEN, "goto: could not resolve address '%s'", arg);
		}

		// no trailing tokens allowed; should catch goto addr : addr
		char extra[64];
		if (get_token(list, extra, sizeof(extra)) > 0) {
			return hxptr_err(HX_ERR_BAD_TOKEN,
				"goto: unexpected token '%s' ranges are not valid for goto", extra);
		}

		req.addr = addr;
		memcpy(out_data, &req, sizeof(req));
		return hxptr_ok();
	}

	// case: goto . or goto . +/- <num>
	if (tok_type == TOK_DOT) {
		uint32_t base = sh->cur_entry;

		// peek at token 2
		char tok2[64];
		len = get_token(list, tok2, sizeof(tok2));

		// bare goto . just go to current position
		// not super useful but not invalid either
		if (len <= 0) {
			req.addr = base;
			memcpy(out_data, &req, sizeof(req));
			return hxptr_ok();
		}
		list += len;

		HxshTokenType_t tok2_type = classify_token(tok2, sub_cmds, count);

		// must be + or -
		if (tok2_type != TOK_PLUS && tok2_type != TOK_MINUS) {
			return hxptr_err(HX_ERR_BAD_TOKEN,
				"goto: unexpected token '%s' after '.', expected '+' or "
				"'-'",
				tok2);
		}

		int8_t sign = (tok2_type == TOK_PLUS) ? +1 : -1;

		// token 3: the offset
		char tok3[64];
		len = get_token(list, tok3, sizeof(tok3));
		if (len <= 0) {
			return hxptr_err(
				HX_ERR_BAD_TOKEN, "goto: expected a number after '%s'", tok2);
		}
		list += len;

		HxshTokenType_t tok3_type = classify_token(tok3, NULL, 0);
		uint32_t offset;
		if (resolve_num(tok3_type, tok3, &offset) != 0) {
			return hxptr_err(
				HX_ERR_BAD_TOKEN, "goto: '%s' is not a valid number", tok3);
		}

		// underflow guard
		if (sign == -1 && offset > base) {
			return hxptr_err(HX_ERR_BAD_TOKEN,
				"goto: offset %u would underflow address 0x%08x", offset, base);
		}

		// no trailing tokens
		char extra[64];
		if (get_token(list, extra, sizeof(extra)) > 0) {
			return hxptr_err(HX_ERR_BAD_TOKEN, "goto: unexpected token '%s'", extra);
		}

		req.addr = (sign == +1) ? base + offset : base - offset;
		memcpy(out_data, &req, sizeof(req));
		return hxptr_ok();
	}

	// catch all colon and anything else
	if (tok_type == TOK_COLON) {
		return hxptr_err(
			HX_ERR_BAD_TOKEN, "goto: ranges are not valid, use an explicit address");
	}

	return hxptr_err(HX_ERR_BAD_TOKEN,
		"goto: unexpected token '%s', expected address, region, or memory", arg);
}
