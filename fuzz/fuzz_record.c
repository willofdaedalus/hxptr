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

// Target: hex_line_to_record() -- single IHEX line -> record.
// Pure in-memory, no I/O. Cheapest/fastest target, run this the most.
#include "hxptr.h"
#include "hxptr_record.h"
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
	if (size == 0 || size > 4096) {
		return 0;
	}

	char line[4097];
	memcpy(line, data, size);
	line[size] = '\0';

	hxptr_record_t rec = { 0 };
	hex_line_to_record(line, &rec);
	free(rec.data); // rec.data is heap-allocated now (see hxptr_record.c) -- was a
	                // no-op when it used to be a fixed-size array
	return 0;
}
