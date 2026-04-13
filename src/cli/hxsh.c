#include "hxsh.h"
#include "hxql.h"
#include "hxsh_table.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static hxptr_error_t info_handler(const char *args, void *out_data);
static hxptr_error_t list_handler(hxsh_t *hx, const char *args, void *out_data);
static hxptr_error_t which_handler(
	hxsh_t *sh, const char *args, void *out_data);

// out_tok map
static const token_map_t token_map[] = {
	{ "info",    TOK_INFO    },
	{ "search",  TOK_SEARCH  },
	{ "goto",    TOK_GOTO    },
	{ "which",   TOK_WHICH   },
	{ "list",    TOK_LIST    },
	{ "help",    TOK_HELP    },
	{ "quit",    TOK_QUIT    },
	// args
	{ "regions", TOK_REGIONS },
	{ "memory",  TOK_MEMORY  },
	{ "stats",   TOK_STATS   },
	{ "diag",    TOK_DIAG    },
	{ "profile", TOK_PROFILE },
};
static const size_t token_map_count = sizeof(token_map) / sizeof(token_map[0]);

static HxshTokenType_t classify_token(
	const char *tok, token_map_t *inputs, int inputs_len)
{
	// keyword table lookup first
	if (inputs) {
		for (uint8_t i = 0; i < inputs_len; i++) {
			if (strcmp(tok, inputs[i].name) == 0) {
				return inputs[i].type;
			}
		}
		// at this point I don't expect the token to be here if it's not already
		return TOK_UNKNOWN;
	}

	// 0x prefixed hex; unambiguous address
	if (tok[0] == '0' && (tok[1] == 'x' || tok[1] == 'X')) {
		const char *p = &tok[2];
		if (*p == '\0') {
			// bare "0x" with nothing after
			return TOK_UNKNOWN;
		}

		while (*p) {
			if (!isxdigit((unsigned char) *p)) {
				return TOK_UNKNOWN;
			}
			p++;
		}
		return TOK_HEX;
	}

	// plain decimal; all digits, no hex chars
	if (isdigit((unsigned char) tok[0])) {
		const char *p = tok;
		int is_dec = 1;

		while (*p) {
			if (!isdigit((unsigned char) *p)) {
				is_dec = 0;
				break;
			}
			p++;
		}

		if (is_dec) {
			return TOK_DEC;
		}

		// has non-decimal digit chars; check if all are hex
		p = tok;
		while (*p) {
			if (!isxdigit((unsigned char) *p)) {
				return TOK_UNKNOWN;
			}
			p++;
		}
		return TOK_BARE_HEX;
	}

	// starts with a-f/A-F; could be a bare hex pattern byte
	if (isxdigit((unsigned char) tok[0])) {
		const char *p = tok;
		while (*p) {
			if (!isxdigit((unsigned char) *p)) {
				return TOK_UNKNOWN;
			}
			p++;
		}
		return TOK_BARE_HEX;
	}

	return TOK_UNKNOWN;
}

static int get_token(char *input, char *out_tok, uint8_t toklen)
{
	uint8_t i = 0, j = 0;

	if (!input) {
		return -1;
	}

	while (input[i] && isspace((unsigned char) input[i])) {
		i++;
	}

	if (!input[i]) {
		return 0;
	}

	while (input[i] && !isspace((unsigned char) input[i])) {
		if (j < toklen - 1) {
			out_tok[j++] = input[i++];
		} else {
			return -1;
		}
	}

	out_tok[j] = '\0';
	return i;
}


static void profile_render(mcu_profile_t *profile, HxshTokenType_t type)
{
	profile_region_t region;
	profile_memory_t mem;
	hxsh_table_t t;

	if (!profile) {
		// do something
		fprintf(stderr, "you shouldn't be here...\n");
		return;
	}

	table_init(&t);
	if (type == TOK_MEMORY) {
		char s_start[16], s_size[16];

		const char *headers[] = { "name", "start", "size" };
		table_set_headers(&t, headers, 3);

		for (uint8_t i = 0; i < profile->mem_count; i++) {
			mem = profile->mem_blocks[i];
			snprintf(s_start, sizeof(s_start), "0x%08x", mem.start);
			snprintf(s_size, sizeof(s_size), "%u", mem.size);
			const char *row[] = { mem.type, s_start, s_size };
			table_add_row(&t, row, 3);
		}

		table_render(&t, "memory info");

	} else if (type == TOK_REGIONS) {
		char s_start[16], s_end[16];

		const char *headers[] = { "name", "start", "end", "protected" };
		table_set_headers(&t, headers, 4);

		for (uint8_t i = 0; i < profile->region_count; i++) {
			region = profile->regions[i];
			snprintf(s_start, sizeof(s_start), "0x%08x", region.start);
			snprintf(s_end, sizeof(s_end), "0x%08x", region.end);
			const char *row[] = { region.name, s_start, s_end,
				region.protected ? "yes" : "no" };
			table_add_row(&t, row, 4);
		}

		table_render(&t, "regions info");
	} else if (type == TOK_PROFILE) {
		const char *headers[] = { "name", "architecture", "vendor" };
		const char *row[] = { profile->name, profile->arch, profile->vendor };

		table_set_headers(&t, headers, 3);
		table_add_row(&t, row, 4);
		table_render(&t, "profile info");
	} else {
		printf("you're not allowed here!\n");
	}
}

static void which_render(hxql_result_t *result)
{
	which_result_t res = result->data.which;

	printf("current memory: %s\n", res.memory);
	printf("current region: %s\n", res.region);
}

static void list_render(hxql_result_t *result)
{
	char hex_map[] = "0123456789abcdef";
	list_result_t res = result->data.list;
	hxsh_table_t t;
	const char *headers[] = { "address", "data" };

	// get max width for proper alignment
	uint8_t max_row_width = 0;
	for (uint16_t j = 0; j < res.count; j++) {
		if (res.start[j].byte_count > max_row_width) {
			max_row_width = res.start[j].byte_count;
		}
	}


	if (max_row_width == 0) {
		max_row_width = 16;
	}

	table_init(&t);
	table_set_headers(&t, headers, 2);

	for (uint16_t i = 0; i < res.count; i++) {
		memory_entry_t *entry = &res.start[i];
		char addr[16];
		// allocate string for exactly the max width found
		char data_str[(max_row_width * 3) + 1];

		snprintf(addr, sizeof(addr), "0x%08x", entry->address);
		for (uint8_t j = 0; j < max_row_width; j++) {
			int offset = j * 3;

			if (j < entry->byte_count) {
				uint8_t b = entry->bytes[j];
				data_str[offset] = hex_map[(b >> 4) & 0x0f];
				data_str[offset + 1] = hex_map[b & 0x0f];
			} else {
				// pad the "empty" space with dashes
				data_str[offset] = '-';
				data_str[offset + 1] = '-';
			}
			// space between bytes, null at the very end of the string
			data_str[offset + 2] = (j == max_row_width - 1) ? '\0' : ' ';
		}

		const char *row[] = { addr, data_str };
		table_add_row(&t, row, 2);
	}

	table_render(&t, "list entries");
}

static void stats_render(hxql_result_t *result)
{
	hxptr_stats_t *s = result->data.info.stats;

	printf("\nstats info\n");
	printf("  %-20s %u\n", "total bytes", s->total_bytes);
	printf("  %-20s %u\n", "total records", s->total_records);
	printf("  %-20s %u\n", "total overlaps", s->total_overlaps);
	printf("  %-20s %u\n", "total gaps", s->total_gaps);
	printf("  %-20s %u bytes\n", "largest gap", s->largest_gap);
	printf("  %-20s 0x%08x\n", "lowest address", s->addr_lowest);
	printf("  %-20s 0x%08x\n", "highest address", s->addr_highest);
}

void render(hxql_result_t *result)
{
	info_result_t res;
	if (!result) {
		return;
	}

	switch (result->type) {
		case HXQL_RES_INFO:
			res = result->data.info;

			if (res.args & INFO_PROF_MASK) {
				if (!res.profile) {
					fprintf(stderr,
						"no mcu profile loaded for arguments 'memory', "
						"'regions' and 'profile'\n");
				} else {
					if (res.args & TOK_MEMORY) {
						profile_render(res.profile, TOK_MEMORY);
					}
					if (res.args & TOK_REGIONS) {
						profile_render(res.profile, TOK_REGIONS);
					}
					if (res.args & TOK_PROFILE) {
						profile_render(res.profile, TOK_PROFILE);
					}
				}
			}

			if (res.args & TOK_STATS) {
				stats_render(result);
			}
			break;

		case HXQL_RES_LIST:
			list_render(result);
			break;

		case HXQL_RES_WHICH:
			which_render(result);
			break;

		default:
			fprintf(stderr, "nothing to see here... yet\n");
			break;
	}
	putchar('\n');
}

hxptr_error_t hxsh_repl(hxsh_t *sh)
{
	char cmd[32];
	int len = 0;
	hxsh_req_t req = { 0 };
	HxshTokenType_t op = TOK_UNKNOWN;
	hxptr_error_t err = { 0 };
	hxql_result_t result = { 0 };

	printf(PROMPT);
	if (fgets(sh->cmd, sizeof(sh->cmd), stdin) == NULL) {
		sh->running = 0;
		return hxptr_ok();
	}
	sh->cmd[strcspn(sh->cmd, "\n")] = '\0';

	if (sh->cmd[0] == '\0') {
		return hxptr_ok();
	}

	char *p = sh->cmd;

	// get the initial token which is always considered the command/operation
	len = get_token(p, cmd, sizeof(cmd));
	p += len;

	for (size_t i = 0; i < token_map_count; i++) {
		if (strcmp(cmd, token_map[i].name) == 0) {
			op = token_map[i].type;
		}
	}
	if (op == TOK_UNKNOWN) {
		return hxptr_err(
			HX_ERR_UNKNOWN_CMD, "unknown command '%s'. try 'help'", cmd);
	}

	// trim leading whitespace from remaining args
	while (isspace((unsigned char) *p)) {
		p++;
	}

	req.type = op;
	switch (req.type) {
		case TOK_INFO:
			err = info_handler(p, &req.req_type.info_req);
			break;
		case TOK_LIST:
			err = list_handler(sh, p, &req.req_type.list_req);
			break;
		case TOK_WHICH:
			err = which_handler(sh, p, &req.req_type.which_req);
			if (req.req_type.which_req.args == TOK_WH_ADDR) {
				printf("0x%08x\n", sh->cur_idx);
				return hxptr_ok();
			}
			break;
	}
	if (err.code != HX_OK) {
		return err;
	}

	err = hxql_execute_cmd(sh->hx, &req, &result);
	if (err.code != HX_OK) {
		return err;
	}

	render(&result);

	return hxptr_ok();
}

// handles the info command
// args: the optional args passed via the shell for the command
// out_data: the data that will be passed to the executor
static hxptr_error_t info_handler(const char *args, void *out_data)
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

// resolves TOK_DOT or TOK_HEX to a uint32_t
static int resolve_base(
	hxsh_t *sh, HxshTokenType_t type, const char *raw, uint32_t *out)
{
	if (type == TOK_DOT) {
		*out = sh->cur_idx;
		return 0;
	}
	if (type == TOK_HEX) {
		char *end;
		*out = (uint32_t) strtoul(raw, &end, 16);
		if (end == raw || *end != '\0') {
			return -1;
		}
		return 0;
	}
	return -1;
}

// helper -- resolves TOK_DEC or TOK_HEX to a uint32_t count/offset
static int resolve_num(HxshTokenType_t type, const char *raw, uint32_t *out)
{
	if (type == TOK_DEC) {
		char *end;
		*out = (uint32_t) strtoul(raw, &end, 10);
		if (end == raw || *end != '\0') {
			return -1;
		}
		return 0;
	}
	if (type == TOK_HEX) {
		char *end;
		*out = (uint32_t) strtoul(raw, &end, 16);
		if (end == raw || *end != '\0') {
			return -1;
		}
		return 0;
	}
	return -1;
}

static hxptr_error_t which_handler(hxsh_t *sh, const char *args, void *out_data)
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
		return hxptr_err(HX_ERR_NULL, "%s:%d: which_handler(): sh is NULL",
			__FILE__, __LINE__);
	}

	if (!args || !*args) {
		// no arguments: request everything
		req.args = TOK_WH_ADDR | TOK_WH_REGION | TOK_WH_MEMORY;
	} else {
		while ((len = get_token(list, arg, sizeof(arg))) > 0) {
			tok_type = classify_token(arg, sub_cmds, count);

			switch (tok_type) {
				case TOK_WH_ADDR:
				case TOK_WH_REGION:
				case TOK_WH_MEMORY:
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
	req.cur_idx = sh->cur_idx;
	memcpy(out_data, &req, sizeof(req));

	return hxptr_ok();
}

static hxptr_error_t list_handler(hxsh_t *sh, const char *args, void *out_data)
{
	list_req_t req = { 0 };
	char arg[64];
	char *list = (char *) args;
	int len;
	HxshTokenType_t tok_type;

	// token map for structural tokens only
	// keywords handled by classify_token natively
	token_map_t sub_cmds[] = {
		{ ".", TOK_DOT   },
		{ "+", TOK_PLUS  },
		{ "-", TOK_MINUS },
		{ ":", TOK_COLON },
	};
	int count = sizeof(sub_cmds) / sizeof(sub_cmds[0]);

	if (!sh) {
		return hxptr_err(HX_ERR_NULL, "%s:%d: list_handler(): sh is NULL",
			__FILE__, __LINE__);
	}

	if (!out_data) {
		return hxptr_err(HX_ERR_INVALID_ARG,
			"%s:%d: list_handler(): out_data is NULL", __FILE__, __LINE__);
	}

	// no args; default behaviour
	if (!args || !*args) {
		goto default_action;
	}

	// token 1: must be +/-/./addr
	len = get_token(list, arg, sizeof(arg));
	if (len <= 0) {
		// return hxptr_err(
		// 	HX_ERR_BAD_TOKEN, "list: failed to read first argument");
		goto default_action;
	}
	list += len;

	tok_type = classify_token(arg, sub_cmds, count);

	// case: list + <num> or list - <num>
	if (tok_type == TOK_PLUS || tok_type == TOK_MINUS) {
		char num[64];
		len = get_token(list, num, sizeof(num));
		if (len <= 0) {
			return hxptr_err(
				HX_ERR_BAD_TOKEN, "list: expected a number after '%s'", arg);
		}
		list += len;

		HxshTokenType_t num_type = classify_token(num, NULL, 0);
		uint32_t offset;
		if (resolve_num(num_type, num, &offset) != 0) {
			return hxptr_err(
				HX_ERR_BAD_TOKEN, "list: '%s' is not a valid number", num);
		}

		// no trailing tokens allowed
		char extra[64];
		if (get_token(list, extra, sizeof(extra)) > 0) {
			return hxptr_err(HX_ERR_BAD_TOKEN,
				"list: unexpected token '%s' after offset", extra);
		}

		// start
		req.start_addr =
			(tok_type == TOK_PLUS) ? sh->cur_idx : sh->cur_idx - offset;
		req.count = (uint16_t) offset;
		memcpy(out_data, &req, sizeof(req));

		return hxptr_ok();
	}

	// case: list . or list addr -- resolve base first
	if (tok_type != TOK_DOT && tok_type != TOK_HEX) {
		return hxptr_err(HX_ERR_BAD_TOKEN,
			"list: unexpected token '%s', expected address or '.'", arg);
	}

	uint32_t base;
	if (resolve_base(sh, tok_type, arg, &base) != 0) {
		return hxptr_err(
			HX_ERR_BAD_TOKEN, "list: could not resolve address '%s'", arg);
	}

	// peek at token 2
	char tok2[64];
	len = get_token(list, tok2, sizeof(tok2));

	// case: list . or list addr -- nothing follows
	if (len <= 0) {
		req.start_addr = base;
		req.count = HXQL_LIST_DEFAULT;
		req.is_range = 0;
		memcpy(out_data, &req, sizeof(req));
		return hxptr_ok();
	}
	list += len;

	HxshTokenType_t tok2_type = classify_token(tok2, sub_cmds, count);

	// case: list . : addr or list addr : addr or list addr : .
	if (tok2_type == TOK_COLON) {
		char tok3[64];
		len = get_token(list, tok3, sizeof(tok3));
		if (len <= 0) {
			return hxptr_err(
				HX_ERR_BAD_TOKEN, "list: expected address after ':'");
		}
		list += len;

		HxshTokenType_t tok3_type = classify_token(tok3, sub_cmds, count);

		// list . : . is invalid -- no meaningful end point
		if (tok3_type == TOK_DOT && base == sh->cur_idx) {
			return hxptr_err(HX_ERR_BAD_TOKEN,
				"list: '. : .' is ambiguous, use an explicit end address");
		}

		uint32_t end;
		if (resolve_base(sh, tok3_type, tok3, &end) != 0) {
			return hxptr_err(HX_ERR_BAD_TOKEN,
				"list: could not resolve end address '%s'", tok3);
		}

		if (end <= base) {
			return hxptr_err(HX_ERR_BAD_TOKEN,
				"list: end address 0x%08x must be greater than "
				"start address 0x%08x",
				end, base);
		}

		// no trailing tokens allowed
		char extra[64];
		if (get_token(list, extra, sizeof(extra)) > 0) {
			return hxptr_err(HX_ERR_BAD_TOKEN,
				"list: unexpected token '%s' after range", extra);
		}

		req.start_addr = base;
		req.end_addr = end;
		req.is_range = 1;
		memcpy(out_data, &req, sizeof(req));
		return hxptr_ok();
	}

	// case: list . + <num> or list . - <num>
	//       list addr + <num> or list addr - <num>
	if (tok2_type == TOK_PLUS || tok2_type == TOK_MINUS) {
		int8_t sign = (tok2_type == TOK_PLUS) ? +1 : -1;

		char tok3[64];
		len = get_token(list, tok3, sizeof(tok3));
		if (len <= 0) {
			return hxptr_err(
				HX_ERR_BAD_TOKEN, "list: expected a number after '%s'", tok2);
		}
		list += len;

		HxshTokenType_t tok3_type = classify_token(tok3, NULL, 0);
		uint32_t offset;
		if (resolve_num(tok3_type, tok3, &offset) != 0) {
			return hxptr_err(
				HX_ERR_BAD_TOKEN, "list: '%s' is not a valid number", tok3);
		}

		// no trailing tokens allowed
		char extra[64];
		if (get_token(list, extra, sizeof(extra)) > 0) {
			return hxptr_err(
				HX_ERR_BAD_TOKEN, "list: unexpected token '%s'", extra);
		}

		// case: list addr + <num> or list addr - <num>
		req.start_addr = (sign == +1) ? base : base - offset;
		req.count = (uint16_t) offset;
		req.is_range = 0;
		memcpy(out_data, &req, sizeof(req));
		return hxptr_ok();
	}

	// anything else after the base address is an error
	return hxptr_err(
		HX_ERR_BAD_TOKEN, "list: unexpected token '%s' after address", tok2);

default_action:
	req.start_addr = sh->cur_idx;
	req.count = HXQL_LIST_DEFAULT;
	req.is_range = 0;
	memcpy(out_data, &req, sizeof(req));

	return hxptr_ok();
}

hxsh_t *hxsh_init(hxptr_t *core)
{
	hxsh_t *hx;
	if (!core) {
		return NULL;
	}

	hx = calloc(1, sizeof(hxsh_t));
	if (!hx) {
		return NULL;
	}

	hx->hx = core;
	hx->cur_idx = core->stats->addr_lowest;
	hx->search_addr = 0;
	hx->running = 1;

	printf("\033[2J");
	printf("\033[H");
	printf("\033[?1049h");

	return hx;
}

void hxsh_free(hxsh_t *hx)
{
	if (!hx) {
		return;
	}

	printf("\033[?1049l");
	printf("\033[2J");
	printf("\033[H");
	free(hx);
}
