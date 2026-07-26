// Minimal driver: reads a file path from argv[1], feeds it to
// LLVMFuzzerTestOneInput. Lets every harness double as:
//   - a libFuzzer target (compile with -fsanitize=fuzzer,address,...)
//   - a classic AFL/afl-gcc target (afl-fuzz -i corpus -o out -- ./bin @@)
//   - a one-shot repro tool (./bin crashing_input)
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

extern int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size);

int main(int argc, char **argv)
{
	if (argc < 2) {
		fprintf(stderr, "usage: %s <input-file>\n", argv[0]);
		return 1;
	}

	FILE *f = fopen(argv[1], "rb");
	if (!f) {
		perror("fopen");
		return 1;
	}

	fseek(f, 0, SEEK_END);
	long len = ftell(f);
	fseek(f, 0, SEEK_SET);
	if (len < 0) {
		fclose(f);
		return 1;
	}

	uint8_t *buf = malloc((size_t) len);
	if (!buf) {
		fclose(f);
		return 1;
	}

	size_t n = fread(buf, 1, (size_t) len, f);
	fclose(f);

	LLVMFuzzerTestOneInput(buf, n);
	free(buf);
	return 0;
}
