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

	res = hxptr_finalize_memory(hx);
	if (res != HX_OK) {
		goto cleanup;
	}

	printf("loaded %u bytes\n", hx->stats->total_bytes);
	printf("%u total overlaps\n", hx->stats->total_overlaps);
	printf("%u total records\n", hx->stats->total_records);
	printf("%u total gaps\n", hx->stats->total_gaps);
	printf("0x%.8x address lo\n", hx->stats->addr_lowest);
	printf("0x%.8x address hi\n", hx->stats->addr_highest);
	printf("%u largest gap\n", hx->stats->largest_gap);

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
