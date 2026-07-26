// Target: get_token() -- REPL command-line tokenizer. Cheap, in-memory.
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "hxsh_tokenizer.h"

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
	if (size == 0 || size > 512) {
		return 0;
	}

	char input[513];
	memcpy(input, data, size);
	input[size] = '\0';

	char out_tok[128];
	// toklen param is uint8_t -- must stay <= 255
	get_token(input, out_tok, sizeof(out_tok));
	return 0;
}
