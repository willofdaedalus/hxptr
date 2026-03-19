#ifndef HXQL_H
#define HXQL_H

#include "hxptr.h"

// search ff
// goto 08000000
// list 0x80000:0x80010
// list 080000
// list +15

typedef enum {
	// commands
	TOK_INFO = 0xa0,
	TOK_SEARCH = 0xa1,
	TOK_GOTO = 0xa2,
	TOK_FRAME = 0xa3,
	TOK_LIST = 0xa4,
	TOK_QUIT = 0xa5,
	TOK_HELP = 0xa6,

	// info args
	TOK_REGIONS = 0xc0,
	TOK_MEMORY = 0xc1,
	TOK_STATS = 0xc2,
	TOK_DIAGS = 0xc3,
	TOK_PROFILE = 0xc4,

	// arg types
	TOK_ADDR, // 0x08000000
	TOK_RANGE, // 0x0800:0x0900
	TOK_PATTERN, // hex bytes ff or 0xff
	TOM_DOT, // for current position
	TOK_NUMBER, // decimal

	// metadata
	TOK_EOF,
	TOK_UNKNOWN,
} HxqlTokenType_t;

typedef struct {
	HxqlTokenType_t type;
	char literal[64];
} hxql_token_t;

typedef struct {
	hxptr_t *core;

} hxql_t;

#endif
