#ifndef SHELL_H
#define SHELL_H

#define PROMPT "(hxptr) "
#define MIN_DISP_VALS (0x10)

#include "hxptr.h"

typedef struct {
	memory_entry_t *entries;

} hxsh_view_t;

typedef struct {
	hxptr_t *hx;
	uint32_t cur_idx;

	char cmd[16];

	int running;
} hxsh_t;


hxsh_t *hxsh_init(hxptr_t *core);
void hxsh_free(hxsh_t *hx);

#endif
