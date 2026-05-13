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
	char region[32];
} hxql_addr_t;

// result of hxql_parse().
// start_idx and end_idx are indices into sh->hx->memory->entries.
// end_idx is only valid when has_range is set.
// count is the abs value of the offset when the expression was base +/- N.
// for ranges, count is (end_idx - start_idx + 1); handlers can use either.
// err is non-zero on any syntax or logic error.
typedef struct {
	int32_t start_idx;
	int32_t end_idx;
	uint16_t count;
	uint8_t has_range;
	HxptrResult_t err;
} hxql_parser_t;

hxql_parser_t hxql_parse(hxsh_t *sh, const char *args);
int32_t hxql_resolve_addr(hxsh_t *sh, const hxql_addr_t *addr);
// signature takes hxsh_t so the search case can update sh->search state
hxptr_error_t hxql_execute_cmd(hxsh_t *sh, hxsh_req_t *req, hxql_result_t *result);
memory_entry_t *find_entry_by_addr(sparse_memory_t *memory, uint32_t addr);

#endif
