// Target: hxql_parse() -- the query language ("goto", "list", "which", ...).
// Setup is split from the fuzzed input: we build a small populated hxsh_t
// once, then fuzz only the args string. We build the hxsh_t by hand rather
// than calling hxsh_init(), since hxsh_init() calls tcgetattr()/tcsetattr()
// on stdin and returns NULL when there's no real TTY (always true for a
// fuzzer/CI runner) -- hxql_parse() itself doesn't need the terminal state.
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include "hxptr.h"
#include "hxsh.h"
#include "hxql.h"

static hxsh_t *build_fixture(void)
{
	static hxsh_t sh;
	static hxptr_t *ctx = NULL;

	if (ctx) {
		return &sh;
	}

	char path[] = "/tmp/hxptr_fuzz_hxql_fixture_XXXXXX";
	int fd = mkstemp(path);
	if (fd < 0) {
		return NULL;
	}

	// two-record fixture: one data record ("Hello" @ 0x0000), one EOF record
	static const char *content = ":0500000048656C6C6F6B\n:00000001FF\n";
	ssize_t w = write(fd, content, strlen(content));
	(void) w;
	close(fd);

	ctx = hxptr_init();
	if (!ctx) {
		unlink(path);
		return NULL;
	}

	hxptr_error_t err = hxptr_load_file(ctx, path);
	unlink(path);
	if (err.code != HX_OK) {
		hxptr_free(ctx);
		ctx = NULL;
		return NULL;
	}
	hxptr_finalize_memory(ctx);

	memset(&sh, 0, sizeof(sh));
	sh.hx = ctx;
	sh.cur_entry = 0;
	sh.running = 1;
	return &sh;
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
	if (size >= HXQL_CMD_LEN) {
		return 0;
	}

	hxsh_t *sh = build_fixture();
	if (!sh) {
		return 0;
	}

	char args[HXQL_CMD_LEN];
	memcpy(args, data, size);
	args[size] = '\0';

	hxql_parse(sh, args);
	return 0;
}
