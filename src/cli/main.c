#include "hxptr.h"
#include <stdio.h>

int main(int ac, char **av)
{
	char buf[512];
	FILE *f;
	hxptr_t *hx;
	HxptrResult_t res;
	hxptr_record_t record = {0};
	uint32_t line_num = 1;

	if (ac < 2) {
		fprintf(stderr, "hxptr: hxptr <file.hex>\n");
		return -1;
	}

	f = fopen(*++av, "r");
	if (!f) {
		perror("failed to open file");
		return -1;
	}

	hx = hxptr_init();
	if (!hx) {
		fprintf(stderr, "hxptr: error initializing hxptr ctx\n");
		fclose(f);
		return -1;
	}

	while (fgets(buf, sizeof(buf), f) != NULL) {
		res = hxptr_hex_to_record(buf, &record);
		if (res != HX_OK) {
			printf("%d\n", res);
			goto cleanup;
		}

		record.line_num = line_num;
		res = hxptr_append_record(hx, &record);
		if (res != HX_OK) {
			// hxptr_handle_err(res);
			printf("%d\n", res);
			goto cleanup;
		}
		line_num++;
	}

	res = hxptr_finalize(hx);
	if (res != HX_OK) {
		goto cleanup;
	}

	// if (hx->reports->count > 0) {
	// 	hxptr_display_overlaps(hx);
	// }

	// printf("Loaded: %d bytes\n", hx->total_bytes);

	hxptr_free(hx);
	fclose(f);

	return 0;

cleanup:
	hxptr_free(hx);
	fclose(f);
	return -1;
}
