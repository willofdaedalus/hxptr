#include "hxsh.h"
#include "hxptr.h"
#include <stdio.h>
#include <stdlib.h>

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
	hx->cur_idx = 0;
	hx->running = 1;

	printf("\033[2J");
	printf("\033[H");
	printf("\033[?1049h");

	return hx;
}

HxptrResult_t hxsh_process_input(hxsh_t *sh)
{
}

void hxsh_free(hxsh_t *hx)
{
	if (!hx) {
		return;
	}

	// not freeing the core here because hxsh_init didn't initialize the
	// core; this should reduce the risk of a double free
	printf("\033[?1049l");
	printf("\033[2J");
	printf("\033[H");
	free(hx);
}
