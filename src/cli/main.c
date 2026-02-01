#include "hxptr.h"
#include <stdio.h>

int main(int ac, char **av)
{
	hxptr_t *hx;
	HxptrResult_t res;

	if (ac < 2) {
		fprintf(stderr, "hxptr: hxptr <file.hex>\n");
		return -1;
	}

	hx = hxptr_init();
	if (!hx) {
		fprintf(stderr, "hxptr: error initializing hxptr ctx\n");
		return -1;
	}

	res = hxptr_load_file(hx, *++av);
	if (res != HX_OK) {
		// TODO; do extra checks like displaying all errors collected in
		// the context
		return -1;
	}

	res = hxptr_finalize_memory(hx);
	if (res != HX_OK) {
		goto cleanup;
	}

	hxptr_free(hx);

	return 0;

cleanup:
	hxptr_free(hx);
	return -1;
}
