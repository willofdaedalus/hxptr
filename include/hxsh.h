#ifndef SHELL_H
#define SHELL_H

#define PROMPT            "(hxptr) "
#define MIN_DISP_VALS     0x10
#define HXQL_MAX_TOKENS   0x20
#define HXQL_CMD_LEN      0x100
#define HXQL_LIST_DEFAULT 0x0f
#define INFO_PROF_MASK    (TOK_REGIONS | TOK_PROFILE | TOK_MEMORY)
#define WHICH_MASK        (TOK_WH_ADDR | TOK_WH_REGION | TOK_WH_MEMORY)
#define INFO_ALL_ARGS                                               \
	(TOK_REGIONS | TOK_MEMORY | TOK_STATS | TOK_DIAG | TOK_PROFILE)
#define UNUSED(x) ((void) (x))

#include "hxptr.h"
#include <stdint.h>

typedef hxptr_error_t (*cmd_handler)(const char *args, void *out_data);

typedef enum {
	// info subcommands
	TOK_REGIONS = (1 << 0),
	TOK_MEMORY = (1 << 1),
	TOK_STATS = (1 << 2),
	TOK_DIAG = (1 << 3),
	TOK_PROFILE = (1 << 4),

	// commands
	TOK_INFO,
	TOK_SEARCH,
	TOK_GOTO,
	TOK_WHICH,
	TOK_LIST,
	TOK_HELP,
	TOK_QUIT,

	// "which" cmd types
	TOK_WH_ADDR = (1 << 5),
	TOK_WH_REGION = (1 << 6),
	TOK_WH_MEMORY = (1 << 7),

	// argument types
	TOK_HEX,
	TOK_RANGE,
	TOK_DOT,
	TOK_NUMBER,
	TOK_BARE_HEX,
	TOK_DEC,

	// meta
	TOK_PLUS,
	TOK_MINUS,
	TOK_COLON,
	TOK_EOF,
	TOK_UNKNOWN = -1,
} HxshTokenType_t;

typedef struct {
	uint32_t start_addr;
	uint32_t end_addr;
	uint16_t count;
	uint8_t is_range;
} list_req_t;

typedef struct {
	uint32_t addr;
	uint8_t is_named; // was it goto region/memory <name>
	HxshTokenType_t named_type; // TOK_REGIONS or TOK_MEMORY if is_named
	char name[32]; // region or memory name if is_named
} goto_req_t;

typedef struct {
	uint32_t args;
} info_req_t;

typedef struct {
	uint32_t args;
	uint32_t cur_idx;
} which_req_t;

typedef struct {
	HxshTokenType_t type;
	union {
		which_req_t which_req;
		info_req_t info_req;
		list_req_t list_req;
		goto_req_t goto_req;
	} req_type;

} hxsh_req_t;

typedef struct {
	const char *name;
	HxshTokenType_t type;
} token_map_t;

typedef struct {
	hxptr_t *hx;
	uint32_t cur_idx;
	uint32_t search_addr; // last search position
	char cmd[HXQL_CMD_LEN];
	int running;
} hxsh_t;

hxsh_t *hxsh_init(hxptr_t *core);
hxptr_error_t hxsh_repl(hxsh_t *sh);
void hxsh_free(hxsh_t *hx);

#endif
