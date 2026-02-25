#include "hxptr.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>

int main(int ac, char **av)
{
	HxptrResult_t res;
	hxptr_t *hx;
	char filename[MAX_ARG_LEN], profilename[MAX_ARG_LEN];
	int c, pflag = 0, fflag = 0;

	while ((c = getopt(ac, av, "f:p:")) != -1) {
		switch (c) {
		case 'f':
			if (strlen(optarg) > MAX_ARG_LEN) {
				fprintf(stderr,
					"fatal: hex file name longer than "
					"allowed\n");
				fprintf(stderr,
					"fatal: consider moving the file or "
					"renaming it\n");
				return -1;
			}

			strncpy(filename, optarg, sizeof(filename) - 1);
			filename[sizeof(filename) - 1] = '\0';
			fflag = 1;
			break;

		case 'p':
			if (strlen(optarg) > MAX_ARG_LEN) {
				fprintf(stderr,
					"fatal: profile file name longer than "
					"allowed\n");
				fprintf(stderr,
					"fatal: consider moving the file or "
					"renaming it\n");
				return -1;
			}

			strncpy(profilename, optarg, sizeof(filename) - 1);
			profilename[sizeof(profilename) - 1] = '\0';
			pflag = 1;
			break;
		}
	}
	ac -= optind;
	av += optind;

	if (!fflag) {
		fprintf(stderr, "hxptr: hxptr -f <file.hex>\n");
		return -1;
	}

	if (!pflag) {
		fprintf(stderr,
			"warning: no profile loaded, assuming base of 0x0\n");
		fprintf(stderr,
			"warning: use -p <file.prof> to load a profile\n");
	}

	hx = hxptr_init();
	if (!hx) {
		fprintf(stderr, "hxptr: error initializing hxptr object\n");
		return -1;
	}

	if (pflag) {
		res = hxptr_load_profile(hx, profilename);
		if (res != HX_OK) {
			if (hx->error->set) {
				fprintf(stderr,
					"encountered an issue loading mcu "
					"profile\n");
				fprintf(stderr, "%s\n", hx->error->msg);
			}

			goto cleanup;
		}
	}

	res = hxptr_load_file(hx, filename);
	if (res != HX_OK) {
		// TODO; do extra checks like displaying all errors collected in
		// the context
		if (hx->error->set) {
			fprintf(stderr,
				"encountered an issue loading '%s'\n",
				filename);
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
