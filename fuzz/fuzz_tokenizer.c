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
