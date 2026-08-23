#ifndef SHELL_H
#define SHELL_H

#define PROMPT             "(hxptr) "
#define MIN_DISP_VALS      0x10
#define HXQL_MAX_TOKENS    0x20
#define HXQL_CMD_LEN       0x100
#define HXQL_LIST_DEFAULT  0x0f
#define CMD_MAX            0x80
#define HXSH_MARKS_MAX     (0x20)
#define HXSH_HISTORY_MAX   (0x40)
#define INFO_PROF_MASK     (TOK_REGIONS | TOK_PROFILE | TOK_MEMORY)
#define WHICH_MASK         (TOK_WH_ADDR | TOK_WH_REGION | TOK_WH_MEMORY)


// helper macros
#define ARRAY_COUNT(array) (sizeof((array)) / sizeof((array)[0]))
#define INFO_ALL_ARGS (TOK_REGIONS | TOK_MEMORY | TOK_STATS | TOK_DIAG | TOK_PROFILE)
#define UNUSED(x)     ((void) (x))
#define ADDR_OF(entry)          (sh->hx->memory->entries[(entry)].address)
#define MAX(a, b)               ((a) > (b) ? (a) : (b))
// calculates the difference between the current and the end
// useful for resolving possible "list" commands that exceed the highest address
#define DIFF_COUNT(v)           (sh->hx->stats->total_records - (v))

#define HXSH_SEARCH_MAX_PATTERN 16

#include "hxptr.h"
#include <stdint.h>
#include <termios.h>
#include <unistd.h>

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
	TOK_FIND,
	TOK_GOTO,
	TOK_WHICH,
	TOK_LIST,
	TOK_HELP,
	TOK_QUIT,
	TOK_LOAD,
	TOK_MARK,

	// "which" cmd types
	TOK_WH_ADDR = (1 << 5),
	TOK_WH_REGION = (1 << 6),
	TOK_WH_MEMORY = (1 << 7),


	// "mark" cmd types
	TOK_MK_LIST = (1 << 8),
	TOK_MK_RM = (1 << 9),
	TOK_MK_ADD = (1 << 10),
	TOK_MK_MV = (1 << 11),

	// argument types
	TOK_HEX,
	TOK_RANGE,
	TOK_DOT,
	TOK_NUMBER,
	TOK_BARE_HEX,
	TOK_DEC,
	TOK_NEXT,
	TOK_PREV,
	TOK_FILE,

	// address expression base types
	TOK_START,
	TOK_END,
	TOK_REGION_NAME,
	TOK_MARK_NAME,

	// meta
	TOK_PLUS,
	TOK_MINUS,
	TOK_COLON,
	TOK_EOF,
	TOK_UNKNOWN = -1,
} HxshTokenType_t;

typedef enum {
	SEARCH_NEXT = 0,
	SEARCH_PREV = 1,
	SEARCH_FRESH = 2,
} SearchDirection_t;

typedef struct {
	char name[MAX_OBJ_LEN];
	uint32_t addr;
} hxsh_mark_t;

typedef struct {
	char entries[HXSH_HISTORY_MAX][HXQL_CMD_LEN];
	uint32_t count; // number of entries actually stored, <= HXSH_HISTORY_MAX
	uint32_t cursor; // browse position; == count means "not browsing" (live edit)
} hxsh_history_t;

typedef struct {
	uint8_t pattern[HXSH_SEARCH_MAX_PATTERN];
	uint8_t pattern_len;
	uint32_t range_start_idx;
	uint32_t range_end_idx;
	SearchDirection_t direction;
} search_req_t;

typedef struct {
	uint8_t pattern[HXSH_SEARCH_MAX_PATTERN];
	uint8_t pattern_len;
	uint32_t range_start_idx;
	uint32_t range_end_idx;
	int32_t last_hit_idx; // entry index of last match; -1 if none yet
	uint8_t active; // is there a live search in progress?
} hxsh_search_t;

typedef struct {
	uint32_t start_addr;
	uint32_t end_addr;
	uint16_t count;
	uint8_t is_range;
	uint8_t adv_cur;
} list_req_t;

typedef struct {
	uint32_t addr; // fully resolved by the parser
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
		search_req_t search_req;
	} req_type;

} hxsh_req_t;

typedef struct {
	const char *name;
	HxshTokenType_t type;
} token_map_t;

typedef struct {
	hxsh_mark_t marks[HXSH_MARKS_MAX];
	uint32_t mark_used;

	hxptr_t *hx;
	uint32_t cur_entry;
	uint32_t search_addr;
	hxsh_search_t search;
	hxsh_history_t history;
	char cmd[HXQL_CMD_LEN];
	int8_t running;
	struct termios orig_termios;
} hxsh_t;

// Allocates and initializes a new shell session over an already-loaded engine
//
// Puts the terminal into raw (cbreak) mode via termios, stashing the original
// settings in sh->orig_termios for hxsh_free() to restore, and clears the
// screen. core is not owned by the returned session; hxsh_free() never frees it.
//
// @param core	An already-initialized and loaded hxptr engine context
//
// @return	An initialized hxsh_t pointer, or NULL if core is NULL, allocation
//		fails, or the terminal can't be put into raw mode
hxsh_t *hxsh_init(hxptr_t *core);

// Reads, parses, and executes a single command line, rendering its result
//
// Reads one line via the raw-mode line editor, looks it up in cmd_table, then
// (for anything other than 'quit') runs the matching handler to build a
// hxsh_req_t, hands that to hxql_execute_cmd(), and renders the result. Shell-only
// commands (cmd->shell_only) skip the hxql_execute_cmd()/render() step entirely,
// since they don't map onto an engine-level request. An empty line or a line
// that is entirely whitespace is a silent no-op, not an error.
//
// @param sh	The active shell session
//
// @return	hxptr_ok() after a successful (or no-op) command; an error result
//		describing what went wrong otherwise. sh->running is cleared on
//		ctrl+d or 'quit' rather than being reflected in the return value
hxptr_error_t hxsh_repl(hxsh_t *sh);

// Adds a new named bookmark at mark->addr
//
// Rejects a duplicate name outright (HX_ERR_MARKS_DUP) rather than overwriting;
// use hxsh_mark_mv() to reassign an existing name to a new address.
//
// @param sh	The active shell session
// @param mark	The mark to add; mark->addr must already be resolved
//
// @return	hxptr_ok() on success, HX_ERR_NULL on a NULL argument,
//		HX_ERR_MARKS_DUP if the name is already in use, or HX_ERR_MARKS_FULL
//		if HXSH_MARKS_MAX marks are already tracked
hxptr_error_t hxsh_mark_add(hxsh_t *sh, const hxsh_mark_t *mark);

// Moves an existing mark to a new address, preserving its name
//
// Implemented as remove-then-add; if the name doesn't already exist this fails
// with the same HX_ERR_MARKS_404 that hxsh_mark_rm() would produce, rather than
// falling back to creating it.
//
// @param sh	The active shell session
// @param mark	The mark name and its new address
//
// @return	hxptr_ok() on success, HX_ERR_NULL on a NULL argument,
//		HX_ERR_MARKS_404 if no mark with that name exists
hxptr_error_t hxsh_mark_mv(hxsh_t *sh, const hxsh_mark_t *mark);

// Removes a mark by name
//
// @param sh	The active shell session
// @param name	The name of the mark to remove
//
// @return	hxptr_ok() on success, HX_ERR_NULL on a NULL argument,
//		HX_ERR_MARKS_404 if no mark with that name exists
hxptr_error_t hxsh_mark_rm(hxsh_t *sh, const char *name);

// Prints every currently set mark as a formatted table
//
// Prints "no marks set" and returns without emitting a table if no marks are
// currently tracked.
//
// @param sh	The active shell session
//
// @return	hxptr_ok() on success, HX_ERR_NULL if sh is NULL
hxptr_error_t hxsh_mark_list(hxsh_t *sh);

// Looks up the address stored under a named mark
//
// @param sh		The active shell session
// @param mark		The name of the mark to look up
// @param out_addr	Out-param set to the mark's address on success; untouched
//			on failure
//
// @return	hxptr_ok() on success, HX_ERR_NULL on a NULL argument,
//		HX_ERR_MARKS_404 if no mark with that name exists
hxptr_error_t hxsh_mark_find(hxsh_t *sh, const char *mark, uint32_t *out_idx);

// Removes any mark(s) currently pointing at addr
//
// Not yet implemented.
//
// @param sh	The active shell session
// @param addr	The address whose marks should be removed
hxsh_t hxsh_mark_rm_addr(hxsh_t *sh, uint32_t addr);

// Restores the terminal to its original (pre-hxsh_init) mode and frees the session
//
// Does not free hx->hx (the underlying hxptr engine context), since hxsh_init()
// never took ownership of it in the first place.
//
// @param hx	The shell session to free; safe to pass NULL
void hxsh_free(hxsh_t *hx);

#endif
