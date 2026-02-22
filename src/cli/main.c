#include "hxptr.h"
#include <stdio.h>

int main(int ac, char **av)
{
	int line_num;
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

	res = hxptr_load_profile(hx, "./profiles/atmega328p.prof", &line_num);
	if (res != HX_OK) {
		if (res != HX_ERR_BAD_PROFILE) {
			fprintf(stderr,
				"line %d: %s",
				line_num,
				hxptr_strerror(res));
		} else {
			fprintf(stderr, "%s\n", hxptr_strerror(res));
		}

		goto cleanup;
	}

	res = hxptr_load_file(hx, *++av);
	if (res != HX_OK) {
		// TODO; do extra checks like displaying all errors collected in
		// the context
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
