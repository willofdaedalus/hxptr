// Target: mcu_profile_load() -- a completely separate text format
// (directive-based .prof parser) from the IHEX parser. Likely never fuzzed.
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "hxptr.h"
#include "hxptr_profile.h"

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
	if (size > (1 << 16)) {
		return 0;
	}

	char path[] = "/tmp/hxptr_fuzz_prof_XXXXXX";
	int fd = mkstemp(path);
	if (fd < 0) {
		return 0;
	}

	if (size > 0) {
		ssize_t w = write(fd, data, size);
		(void) w;
	}
	close(fd);

	hxptr_error_t err = { 0 };
	mcu_profile_t *prof = mcu_profile_load(path, &err);
	if (prof) {
		mcu_profile_free(prof);
	}

	unlink(path);
	return 0;
}
