// Target: hex_line_to_record() -- single IHEX line -> record.
// Pure in-memory, no I/O. Cheapest/fastest target, run this the most.
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include "hxptr.h"
#include "hxptr_record.h"

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
