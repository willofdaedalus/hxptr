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
