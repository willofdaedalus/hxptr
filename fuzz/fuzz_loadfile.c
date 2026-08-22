// Target: hxptr_load_file() -- full pipeline: line parsing + sm_append_record's
// pool/entry growth in hxptr_memory.c. Catches bugs that only surface across
// multi-record sequences (e.g. cumulative pool_len growth), not just one line.
// Slower than fuzz_record (disk I/O per iteration) but exercises more surface.
#include "hxptr.h"
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
	// if (size > (1 << 20)) { // cap at 1MB so runs stay fast
	// 	return 0;
	// }

	char path[] = "/tmp/hxptr_fuzz_load_XXXXXX";
	int fd = mkstemp(path);
	if (fd < 0) {
		return 0;
	}

	if (size > 0) {
		ssize_t w = write(fd, data, size);
		(void) w;
	}
	close(fd);

	hxptr_t *ctx = hxptr_init();
	if (ctx) {
		hxptr_error_t err = hxptr_load_file(ctx, path);
		if (err.code == HX_OK) {
			hxptr_finalize_memory(ctx);
		}
		hxptr_free(ctx);
	}

	unlink(path);
	return 0;
}
