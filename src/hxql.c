#include "hxql.h"
#include "hxsh.h"
#include "hxsh_tokenizer.h"
#include <string.h>

// token map shared across parse_addr_expr calls
// covers every symbol that can appear in an address expression
static token_map_t expr_tokens[] = {
	{ ".",      TOK_DOT         },
	{ "+",      TOK_PLUS        },
	{ "-",      TOK_MINUS       },
	{ ":",      TOK_COLON       },
	{ "start",  TOK_START       },
	{ "end",    TOK_END         },
	{ "region", TOK_REGION_NAME },
};
static int expr_token_count = sizeof(expr_tokens) / sizeof(expr_tokens[0]);

// same search but returns index instead of pointer. returns -1 on miss.
static inline int32_t find_entry_idx_by_addr(sparse_memory_t *memory, uint32_t addr)
{
	uint32_t lo, hi, mid;
	memory_entry_t *mem;

	if (!memory || memory->count == 0) {
		return -1;
	}

	lo = 0;
	hi = memory->count;

	while (lo < hi) {
		mid = lo + (hi - lo) / 2;
		mem = &memory->entries[mid];
		if ((mem->address + mem->byte_count) > addr) {
			hi = mid;
		} else {
			lo = mid + 1;
		}
	}

	if (lo >= memory->count) {
		return -1;
	}

	return (int32_t) lo;
}

// binary search; returns pointer to first entry with address >= addr.
// returns null if nothing qualifies.
memory_entry_t *find_entry_by_addr(sparse_memory_t *memory, uint32_t addr)
{
	int32_t idx = find_entry_idx_by_addr(memory, addr);
	return (idx >= 0) ? &memory->entries[idx] : NULL;
}

// parse_addr_expr
//
// reads one address expression from *cursor, advancing it past what it
// consumes. stops before TOK_COLON or end of input.
//
// grammar:
//   expr   := base [offset]
//   base   := '.' | hex_addr | 'start' | 'end' | 'region' name
//   offset := ('+' | '-') number
//
// implicit dot rule: leading '+' or '-' with no base means '.'.
//
// returns 0 on success, -1 on syntax error.
static int parse_addr_expr(hxsh_t *sh, char **cursor, hxql_addr_t *out)
{
	char tok[64];
	char tok2[64];
	int len;
	HxshTokenType_t type;
	int sign;
	uint32_t num;

	memset(out, 0, sizeof(*out));
	out->base = TOK_UNKNOWN;

	len = get_token(*cursor, tok, sizeof(tok));
	if (len <= 0) {
		return -1;
	}

	type = classify_token(tok, expr_tokens, expr_token_count);

	// implicit dot: leading sign with no explicit base
	if (type == TOK_PLUS || type == TOK_MINUS) {
		out->base = TOK_DOT;
		// don't advance cursor; the sign is consumed below as the offset
		goto parse_offset;
	}

	// consume the base token now that we know it's not a leading sign
	*cursor += len;

	if (type == TOK_DOT) {
		out->base = TOK_DOT;
	} else if (type == TOK_HEX || type == TOK_BARE_HEX) {
		out->base = TOK_HEX;
		if (resolve_base(sh, type, tok, &out->literal_addr) != 0) {
			return -1;
		}
	} else if (type == TOK_START) {
		out->base = TOK_START;
	} else if (type == TOK_END) {
		out->base = TOK_END;
	} else if (type == TOK_REGION_NAME) {
		// next token is the region name itself
		len = get_token(*cursor, tok2, sizeof(tok2));
		if (len <= 0) {
			return -1;
		}
		*cursor += len;
		out->base = TOK_REGION_NAME;
		strncpy(out->region, tok2, sizeof(out->region) - 1);
	} else {
		return -1;
	}

parse_offset:
	// peek at the next token; only consume if it's +/-
	len = get_token(*cursor, tok, sizeof(tok));
	if (len <= 0) {
		return 0;
	}

	type = classify_token(tok, expr_tokens, expr_token_count);
	if (type != TOK_PLUS && type != TOK_MINUS) {
		// not an offset; leave cursor where it is
		return 0;
	}

	sign = (type == TOK_PLUS) ? 1 : -1;
	*cursor += len;

	// must be followed by a number
	len = get_token(*cursor, tok, sizeof(tok));
	if (len <= 0) {
		return -1;
	}
	*cursor += len;

	type = classify_token(tok, NULL, 0);
	if (resolve_num(type, tok, &num) != 0) {
		return -1;
	}
	out->offset = sign * (int32_t) num;

	return 0;
}

// hxql_resolve_addr
//
// resolves a single hxql_addr_t to an entry index.
// returns the index on success, -1 on any error (including out of bounds).
int32_t hxql_resolve_addr(hxsh_t *sh, const hxql_addr_t *addr)
{
	int32_t base_idx;
	int32_t result_idx = 0;
	profile_region_t *region;
	uint8_t i;

	switch (addr->base) {
		case TOK_DOT:
			base_idx = (int32_t) sh->cur_entry;
			break;

		case TOK_HEX:
		case TOK_BARE_HEX:
			base_idx = find_entry_idx_by_addr(sh->hx->memory, addr->literal_addr);
			if (base_idx < 0) {
				return -1;
			}
			break;

		case TOK_START:
			base_idx =
				find_entry_idx_by_addr(sh->hx->memory, sh->hx->stats->addr_lowest);
			if (base_idx < 0) {
				return -1;
			}
			break;

		case TOK_END:
			base_idx =
				find_entry_idx_by_addr(sh->hx->memory, sh->hx->stats->addr_highest);
			if (base_idx < 0) {
				return -1;
			}
			break;

		case TOK_REGION_NAME:
			if (!sh->hx->profile) {
				return -1;
			}

			i = 0;
			while (i < sh->hx->profile->region_count) {
				region = &sh->hx->profile->regions[i];
				if (strcmp(addr->region, region->name) == 0) {
					return find_entry_idx_by_addr(sh->hx->memory, region->start);
				}
				i++;
			}

			// region name not found in profile
			return -1;

		default:
			return -1;
	}

	result_idx = base_idx + addr->offset;

	// hard error in both directions; no clamping
	if (result_idx < 0 || result_idx >= (int32_t) sh->hx->memory->count) {
		return -1;
	}

	return result_idx;
}

// hxql_parse
//
// entry point for all command handlers.
// takes the arg string after the command name has been stripped.
// returns resolved entry indices in hxql_parser_t.
// err is set on any syntax or bounds failure.
hxql_parser_t hxql_parse(hxsh_t *sh, const char *args)
{
	hxql_parser_t result = { 0 };
	hxql_addr_t lhs = { 0 };
	hxql_addr_t rhs = { 0 };
	char buf[HXQL_CMD_LEN];
	char *cursor;
	char peek[64];
	int len;
	HxshTokenType_t type;

	if (!sh || !args) {
		result.res = HX_ERR_NULL;
		return result;
	}

	// work on a local copy so we don't mutate the caller's string
	strncpy(buf, args, sizeof(buf) - 1);
	buf[sizeof(buf) - 1] = '\0';
	cursor = buf;

	// no args: default to current position
	if (!*args) {
		result.start_idx = sh->cur_entry;
		result.end_idx = sh->cur_entry;
		result.has_range = 0;
		result.res = HX_OK;
		return result;
	}

	// parse lhs expression
	if (parse_addr_expr(sh, &cursor, &lhs) != 0) {
		result.res = HX_ERR_PARSE;
		return result;
	}

	// peek for a range separator
	len = get_token(cursor, peek, sizeof(peek));
	if (len > 0) {
		type = classify_token(peek, expr_tokens, expr_token_count);

		if (type == TOK_COLON) {
			cursor += len; // consume ':'

			if (parse_addr_expr(sh, &cursor, &rhs) != 0) {
				result.res = HX_ERR_PARSE;
				return result;
			}

			result.has_range = 1;
		} else {
			// unexpected token after a complete expression
			result.res = HX_ERR_PARSE;
			return result;
		}
	}

	// resolve lhs
	result.start_idx = hxql_resolve_addr(sh, &lhs);
	if (result.start_idx < 0) {
		result.res = HX_ERR_OVERFLOW;
		return result;
	}

	if (result.has_range) {
		result.end_idx = hxql_resolve_addr(sh, &rhs);
		if (result.end_idx < 0) {
			result.res = HX_ERR_OVERFLOW;
			return result;
		}

		// lhs must resolve strictly lower than rhs
		if (result.start_idx >= result.end_idx) {
			result.res = HX_ERR_PARSE;
			return result;
		}

		result.count = (uint16_t) (result.end_idx - result.start_idx + 1);
	} else {
		result.end_idx = result.start_idx;
		result.count = (DIFF_COUNT(result.start_idx) >= 15)
			? HXQL_LIST_DEFAULT
			: DIFF_COUNT(result.start_idx);
	}

	result.res = HX_OK;

	return result;
}

static int search_entry(
	memory_entry_t *entry, const uint8_t *pattern, uint8_t pattern_len)
{
	if (entry->byte_count < pattern_len) {
		return -1;
	}

	uint8_t lim = entry->byte_count - pattern_len;
	for (uint8_t off = 0; off <= lim; off++) {
		if (memcmp(&entry->bytes[off], pattern, pattern_len) == 0) {
			return (int) off;
		}
	}

	return -1;
}

static inline void info_executor(
	hxptr_t *hx, info_req_t info_req, info_result_t *info_res)
{
	if (info_req.args & TOK_STATS) {
		info_res->stats = hx->stats;
		info_res->args |= TOK_STATS;
	}

	if (info_req.args & INFO_PROF_MASK) {
		info_res->profile = hx->profile;
		info_res->args |= (info_req.args & INFO_PROF_MASK);
	}

	if (info_req.args & TOK_DIAG) {
		info_res->diagnostics = hx->reports->entries;
		info_res->args |= TOK_DIAG;
	}
}

static void which_executor(hxptr_t *hx, which_req_t *req, which_result_t *res)
{
	uint8_t i = 0;
	mcu_profile_t *profile = hx->profile;
	profile_region_t *region;
	profile_memory_t *memory;
	res->cur_addr = req->cur_idx;

	if (req->args & TOK_WH_REGION) {
		strncpy(res->region, "no region found", (MAX_OBJ_LEN * 2) - 1);

		while (i < profile->region_count) {
			region = &profile->regions[i];
			if (req->cur_idx >= region->start && req->cur_idx <= region->end) {
				strncpy(res->region, region->name, (MAX_OBJ_LEN * 2) - 1);
				break;
			}
			i++;
		}
	}

	i = 0;
	if (req->args & TOK_WH_MEMORY) {
		strncpy(res->memory, "no memory found", (MAX_OBJ_LEN * 2) - 1);

		while (i < profile->mem_count) {
			memory = &profile->mem_blocks[i];
			if (req->cur_idx >= memory->start &&
				req->cur_idx <= memory->start + memory->size) {
				strncpy(res->memory, memory->type, (MAX_OBJ_LEN * 2) - 1);
				break;
			}
			i++;
		}
	}

	res->args = req->args;
}

static inline void goto_executor(hxptr_t *hx, goto_req_t *req, goto_result_t *res)
{
	UNUSED(hx);
	res->addr = req->addr;
	res->found = 1;
}

static void search_executor(
	hxsh_t *sh, search_req_t *req, search_result_t *res, hxql_result_t *result)
{
	sparse_memory_t *mem = sh->hx->memory;
	int32_t scan_start, scan_idx, hit_off;
	int32_t range_start = (int32_t) req->range_start_idx;
	int32_t range_end = (int32_t) req->range_end_idx;
	uint8_t wrapped = 0;

	// determine scan_start and initial wrapped flag
	switch (req->direction) {
		case SEARCH_NEXT:
			if (sh->search.last_hit_idx < 0 ||
				sh->search.last_hit_idx >= range_end) {
				scan_start = range_start;
				wrapped = 1;
			} else {
				scan_start = sh->search.last_hit_idx + 1;
			}
			break;

		case SEARCH_PREV:
			if (sh->search.last_hit_idx <= range_start) {
				scan_start = range_end;
				wrapped = 1;
			} else {
				scan_start = sh->search.last_hit_idx - 1;
			}
			break;

		case SEARCH_FRESH:
			__attribute__((fallthrough));
		default:
			scan_start = (int32_t) sh->cur_entry;
			if (scan_start < range_start || scan_start > range_end) {
				scan_start = range_start;
			}
			break;
	}

	// scan two segments: [scan_start .. far_end], then wrap [wrap_start ..
	// scan_start-step] for PREV the direction is reversed; segments are symmetric
	int32_t step = (req->direction == SEARCH_PREV) ? -1 : 1;
	int32_t seg1_end = (req->direction == SEARCH_PREV) ? range_start : range_end;
	int32_t seg2_start = (req->direction == SEARCH_PREV) ? range_end : range_start;
	int32_t seg2_end = scan_start - step; // exclusive stop for wrap segment

	scan_idx = scan_start;
	while (1) {
		hit_off =
			search_entry(&mem->entries[scan_idx], req->pattern, req->pattern_len);

		if (hit_off >= 0) {
			res->hit_addr = mem->entries[scan_idx].address + (uint32_t) hit_off;
			res->found = 1;
			res->wrapped = wrapped;

			sh->search.active = 1;
			sh->search.last_hit_idx = scan_idx;
			sh->search.range_start_idx = (uint32_t) range_start;
			sh->search.range_end_idx = (uint32_t) range_end;
			sh->search.pattern_len = req->pattern_len;
			memcpy(sh->search.pattern, req->pattern, req->pattern_len);
			result->code = HX_OK;
			return;
		}

		if (scan_idx != seg1_end) {
			scan_idx += step;
		} else if (!wrapped && scan_start != seg2_start) {
			// transition to wrap segment
			scan_idx = seg2_start;
			wrapped = 1;
		} else {
			break;
		}

		// stop wrap segment before we re-visit scan_start
		if (wrapped && scan_idx == seg2_end + step) {
			break;
		}
	}

	// nothing found
	res->found = 0;
	sh->search.active = 0;
	result->code = HX_OK;
	memcpy(&result->data.search, res, sizeof(search_result_t));
}


// hxql_execute_cmd
hxptr_error_t hxql_execute_cmd(hxsh_t *sh, hxsh_req_t *req, hxql_result_t *result)
{
	hxptr_t *hx = sh->hx;
	memory_entry_t *start;

	switch (req->type) {
		case TOK_INFO:
			info_result_t info_res = { 0 };
			result->type = HXQL_RES_INFO;
			info_executor(hx, req->req_type.info_req, &info_res);
			result->code = HX_OK;
			memcpy(&result->data.info, &info_res, sizeof(info_res));
			break;

		case TOK_LIST:
			list_req_t *list_req = &req->req_type.list_req;
			list_result_t list_res = { 0 };
			result->type = HXQL_RES_LIST;

			start = find_entry_by_addr(hx->memory, list_req->start_addr);
			if (!start) {
				result->code = HX_ERR_NO_RECORD;
				break;
			}

			list_res.start = start;
			list_res.count = list_req->count;
			result->code = HX_OK;
			memcpy(&result->data.list, &list_res, sizeof(list_res));
			break;

		case TOK_WHICH:
			which_result_t which_res = { 0 };
			result->type = HXQL_RES_WHICH;
			which_executor(hx, &req->req_type.which_req, &which_res);
			result->code = HX_OK;
			memcpy(&result->data.which, &which_res, sizeof(which_res));
			break;

		case TOK_GOTO:
			goto_result_t goto_res = { 0 };
			result->type = HXQL_RES_GOTO;
			goto_executor(NULL, &req->req_type.goto_req, &goto_res);
			result->code = HX_OK;
			memcpy(&result->data.go, &goto_res, sizeof(goto_res));
			break;

		case TOK_FIND:
			search_result_t search_res = { 0 };
			result->type = HXQL_RES_FIND;
			search_executor(sh, &req->req_type.search_req, &search_res, result);
			memcpy(&result->data.search, &search_res, sizeof(search_res));
			break;

		default:
			break;
	}

	return hxptr_ok();
}
