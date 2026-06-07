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

extern const hxsh_cmd_t cmd_table[];
extern const size_t cmd_table_count;

#endif
