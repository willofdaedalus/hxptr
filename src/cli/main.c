#include "hxptr.h"
#include <stdio.h>

int main(int ac, char **av)
{
	HxptrResult_t res;
	hxptr_t *hx;

	if (ac < 2) {
		fprintf(stderr, "hxptr: hxptr <file.hex>\n");
		return -1;
	}

	hx = hxptr_init();
	if (!hx) {
		fprintf(stderr, "hxptr: error initializing hxptr object\n");
		return -1;
	}

	res = hxptr_load_profile(hx, "./profiles/rp2040.prof");
	if (res != HX_OK) {
		if (hx->error->set) {
			fprintf(stderr,
				"encountered an issue loading mcu profile\n");
			fprintf(stderr, "%s\n", hx->error->msg);
		}

		goto cleanup;
	}

	res = hxptr_load_file(hx, *++av);
	if (res != HX_OK) {
		// TODO; do extra checks like displaying all errors collected in
		// the context
		if (hx->error->set) {
			fprintf(stderr,
				"encountered an issue loading '%s'\n",
				*av);
			fprintf(stderr, "%s\n", hx->error->msg);
		}
		goto cleanup;
	}

	res = hxptr_finalize_memory(hx);
	if (res != HX_OK) {
		goto cleanup;
	}

	hxptr_print_stats(hx);

	hxptr_free(hx);

	return 0;

cleanup:
	hxptr_free(hx);
	return res;
}
