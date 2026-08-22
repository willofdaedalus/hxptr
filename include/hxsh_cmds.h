#ifndef HXSH_CMDS_H
#define HXSH_CMDS_H

#include "hxsh.h"

typedef struct {
	char *name;
	char *short_help;
	char *long_help;
	HxshTokenType_t type;
	hxptr_error_t (*handler)(hxsh_t *, const char *, void *);
	uint8_t shell_only;
} hxsh_cmd_t;

// The static command dispatch table backing the interactive shell
//
// Every entry pairs a command name with its token type, handler, and help text.
// shell_only marks commands (load, mark, help, quit) that are handled entirely
// within hxsh and never reach hxql_execute_cmd() - their handler either does the
// work itself or, for quit, is NULL and handled directly by hxsh_repl(). Lookup
// in hxsh_repl() is a linear scan by name; the table is small enough that this
// isn't worth indexing.
extern const hxsh_cmd_t cmd_table[];

// Number of entries in cmd_table, i.e. sizeof(cmd_table)/sizeof(cmd_table[0])
extern const size_t cmd_table_count;

#endif
