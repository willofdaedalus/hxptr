// Copyright 2026 Manny Quansah
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://apache.org
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef HXQL_H
#define HXQL_H

// search deadbeef
// goto 08000000
// list 0x80000:0x80010
// list 080000
// list +15

#include "hxptr.h"
#include "hxsh.h"

typedef enum {
	HXQL_RES_LIST = 1,
	HXQL_RES_INFO,
	HXQL_RES_WHICH,
	HXQL_RES_GOTO,
	HXQL_RES_FIND,
} HxqlResultType_t;

typedef struct {
	hxptr_stats_t *stats;
	mcu_profile_t *profile;
	diagnostic_t *diagnostics;
	uint8_t args;
} info_result_t;

typedef struct {
	uint8_t found;
	uint32_t addr;
} goto_result_t;

typedef struct {
	uint32_t cur_addr;
	char region[MAX_OBJ_LEN * 2];
	char memory[MAX_OBJ_LEN * 2];
	uint8_t args;
} which_result_t;

typedef struct {
	memory_entry_t *start;
	uint16_t count;
} list_result_t;

typedef struct {
	uint32_t address;
	char msg[128];
} status_result_t;

typedef struct {
	uint32_t hit_addr;
	uint8_t found;
	uint8_t wrapped; // 1 = match was found after wrapping around
} search_result_t;

typedef struct {
	HxqlResultType_t type;
	HxptrResult_t code;
	char err[128];
	union {
		list_result_t list;
		info_result_t info;
		which_result_t which;
		status_result_t status;
		goto_result_t go;
		search_result_t search;
	} data;
} hxql_result_t;

// represents a single parsed address expression before resolution.
// base is one of: TOK_DOT, TOK_HEX, TOK_BARE_HEX, TOK_START, TOK_END,
// TOK_REGION_NAME
// offset is a signed entry count delta (0 if not present)
// literal_addr is only set when base is TOK_HEX or TOK_BARE_HEX
// region is only set when base is TOK_REGION_NAME
typedef struct {
	HxshTokenType_t base;
	int32_t offset;
	uint32_t literal_addr;
	char region[64];
} hxql_addr_t;

// result of hxql_parse().
// start_idx and end_idx are indices into sh->hx->memory->entries.
// end_idx is only valid when has_range is set.
// count is the abs value of the offset when the expression was base +/- N.
// for ranges, count is (end_idx - start_idx + 1); handlers can use either.
// res is non-zero on any syntax or logic error.
typedef struct {
	int32_t start_idx;
	int32_t end_idx;
	uint16_t count;
	uint8_t has_range;
	hxptr_error_t res;
} hxql_parser_t;

// Parses an address expression (or address:address range) into entry indices
//
// This is the shared entry point every command handler delegates to for address
// parsing, before layering on any command-specific constraints (e.g. goto
// rejecting ranges). With no args, resolves to sh->cur_entry with has_range unset.
// Base expressions ('.', hex literal, start, end, region <name>, mark <name>)
// may each carry a '+'/'-' offset in entry-index space. A range's start must
// resolve strictly lower than its end. Operates entirely in resolved entry-index
// space, not raw addresses; callers read the address back out via
// sh->hx->memory->entries[idx].address.
//
// @param sh	The active shell session
// @param args	The argument string after the command name has been stripped;
//		may be empty
//
// @return	hxql_parser_t with start_idx/end_idx/count populated on success;
//		res.code is non-HX_OK on any syntax or out-of-bounds error, in
//		which case start_idx/end_idx/count are not meaningful
hxql_parser_t hxql_parse(hxsh_t *sh, const char *args);

// Resolves a single parsed address expression to an entry index
//
// Handles all six base kinds independently (current position, hex literal,
// start/end of the hex map, a named profile region, or a named mark), then
// applies addr->offset in entry-index space. There is no clamping in either
// direction: a result that lands before entry 0 or past the last entry is a
// hard error, not silently pinned to a boundary.
//
// @param sh	The active shell session
// @param addr	The parsed (but not yet resolved) address expression
// @param res	Out-param set to hxptr_ok() or an error describing why resolution
//		failed (e.g. unknown mark/region, no profile loaded)
//
// @return	The resolved entry index on success, -1 on any error (including a
//		result that falls outside the bounds of the hex map)
int32_t hxql_resolve_addr(hxsh_t *sh, const hxql_addr_t *addr, hxptr_error_t *res);

// Dispatches a fully-parsed shell request to its executor and fills the result
//
// Each hxsh_req_t.type maps to exactly one internal executor (info, list, which,
// goto, find); the executor fills the matching union member of result and
// result->type/code are set accordingly. This always returns hxptr_ok() itself,
// since per-command failures are surfaced through result->code rather than the
// return value; callers should check result->code, not just the return.
//
// @param sh		The active shell session; passed through (rather than just
//			hxptr_t) so the find/search case can update sh->search
// @param req		The request populated by the matching command handler
// @param result	Out-param filled with the executor's output
//
// @return	hxptr_ok() unconditionally
hxptr_error_t hxql_execute_cmd(hxsh_t *sh, hxsh_req_t *req, hxql_result_t *result);

// Binary searches for the first memory entry whose span contains or follows addr
//
// Specifically, returns the first entry where (entry->address + entry->byte_count)
// > addr, i.e. the first entry that has not already ended by addr. This means a
// query that falls inside a gap resolves to the next entry after the gap, not a
// miss - callers that need to distinguish "exact hit" from "nearest neighbor"
// must compare the returned entry's address against addr themselves.
//
// @param memory	The sparse memory layout to search; must be finalized
//			(sorted) for the binary search to be valid
// @param addr		The address to search for
//
// @return	Pointer to the matching entry, or NULL if memory is empty/NULL or
//		addr falls after every entry
memory_entry_t *find_entry_by_addr(sparse_memory_t *memory, uint32_t addr);

#endif
