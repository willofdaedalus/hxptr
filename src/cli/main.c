#include "hxptr.h"
#include "hxsh.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static void sm_print_entries(const sparse_memory_t *mem)
{
	if (!mem)
		return;

	for (size_t i = 0; i < mem->count; ++i) {
		memory_entry_t *entry = &mem->entries[i];

		if (entry->is_data_record) {
			// print address in hex
			printf("[0x%08x] ", entry->address);

			// print bytes separated by spaces
			for (uint8_t j = 0; j < entry->byte_count; ++j) {
				printf("%02x ", entry->bytes[j]);
			}
			printf("\n");
		}
	}
}

static HxptrResult_t init_core(hxptr_t **hx, int pflag, char *prof, char *file)
{
	hxptr_error_t err = {0};

	*hx = hxptr_init();
	if (!hx) {
		fprintf(stderr, "hxptr: error initializing hxptr object\n");
		return HX_ERR_NULL;
	}

	if (pflag) {
		err = hxptr_load_profile(*hx, prof);
		if (err.code != HX_OK) {
			fprintf(stderr, "%s\n", err.msg);
			return err.code;
		}
	}

	err = hxptr_load_file(*hx, file);
	if (err.code != HX_OK) {
		// TODO; do extra checks like displaying all errors collected in
		// the context
		fprintf(stderr, "%s\n", err.msg);
		return err.code;
	}

	err = hxptr_finalize_memory(*hx);
	if (err.code != HX_OK) {
		fprintf(stderr, "%s\n", err.msg);
		return err.code;
	}

	return HX_OK;
}

int main(int ac, char **av)
{
	HxptrResult_t res;
	hxptr_t *hx = NULL;
	hxsh_t *sh;
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

	res = init_core(&hx, pflag, profilename, filename);
	if (res != HX_OK) {
		if (hx) {
			goto cleanup;
		}

		return res;
	}

	sh = hxsh_init(hx);
	if (!sh) {
		res = HX_ERR_NULL;
		goto cleanup;
	}

	while (sh->running) {
		printf(PROMPT);

		if (fgets(sh->cmd, sizeof(sh->cmd), stdin) == NULL) {
			sh->running = 0;
			break;
		}

		printf("%s", sh->cmd);
	}
	printf("goodbye...\n");

	hxsh_free(sh);

	sm_print_entries(hx->memory);

	hxptr_free(hx);

	return 0;

cleanup:
	hxptr_free(hx);
	return res;
}
