// Copyright 2026 Manny Quansah
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://apache.org
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "hxptr.h"
#include "hxsh.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>

// bootstraps a fresh hxptr_t: allocates it, optionally loads a profile,
// loads/parses the hex file, then finalizes memory (sort + gap/overlap
// analysis). bails on the first failure, leaving *hx allocated so the caller
// can still free it.
static HxptrResult_t init_core(hxptr_t **hx, int pflag, char *prof, char *file)
{
	hxptr_error_t err = { 0 };

	*hx = hxptr_init();
	if (!*hx) {
		fprintf(stderr, "hxptr: error initializing hxptr object\n");
		return HX_ERR_NULL;
	}

	if (pflag) {
		err = hxptr_load_profile(*hx, prof);
		if (err.code != HX_OK) {
			fprintf(stderr, "%s\n", err.msg);
			return err.code;
		}
	} else {
		(*hx)->profile = NULL;
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
	hxptr_error_t err;
	HxptrResult_t res;
	hxptr_t *hx = NULL;
	hxsh_t *sh;
	char filename[MAX_ARG_LEN], profilename[MAX_ARG_LEN];
	int c, pflag = 0, fflag = 0;

	while ((c = getopt(ac, av, "f:p:")) != -1) {
		switch (c) {
			case 'f':
				if (strlen(optarg) > MAX_ARG_LEN) {
					fprintf(stderr, "fatal: hex file name longer than allowed\n");
					fprintf(
						stderr, "fatal: consider moving the file or renaming it\n");
					return -1;
				}

				strncpy(filename, optarg, sizeof(filename) - 1);
				filename[sizeof(filename) - 1] = '\0';
				fflag = 1;
				break;

			case 'p':
				if (strlen(optarg) > MAX_ARG_LEN) {
					fprintf(
						stderr, "fatal: profile file name longer than allowed\n");
					fprintf(
						stderr, "fatal: consider moving the file or renaming it\n");
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
		fprintf(stderr, "warning: no profile loaded, assuming base of 0x0\n");
		fprintf(stderr, "warning: use -p <file.prof> to load a profile\n");
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

	if (!hx->profile) {
		printf("no profile loaded. rerun hxptr with -p flag and the path to your "
			   "mcu's profile\n");
	}

	while (sh->running) {
		err = hxsh_repl(sh);
		if (err.code != HX_OK) {
			fprintf(stderr, "%s\n", err.msg);
		}
	}
	hxsh_free(sh);
	hxptr_free(hx);

	printf("goodbye...\n");

	return 0;

cleanup:
	hxptr_free(hx);
	return res;
}
