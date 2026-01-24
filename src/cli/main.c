#include "hxptr.h"
#include <stdio.h>

int main(int ac, char **av)
{
	char buf[512];
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

	res = hxptr_finalize_addr(hx);
	if (res != HX_OK) {
		goto cleanup;
	}

	// res = hxptr_detect_overlaps(hx);
	// if (res != HX_OK) {
	// 	goto cleanup;
	// }

	// if (hx->reports->count > 0) {
	// 	hxptr_display_overlaps(hx);
	// }

	// printf("Loaded: %d bytes\n", hx->total_bytes);

	hxptr_free(hx);

	return 0;

cleanup:
	hxptr_free(hx);
	return -1;
}
