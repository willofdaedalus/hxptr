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

#ifndef HXPTR_PROFILE_H
#define HXPTR_PROFILE_H

#include "hxptr.h"

// Loads and parses a .prof file into a new mcu_profile_t
//
// The file is a line-oriented directive format ('#' starts a comment, blank
// lines are skipped). Each non-comment line dispatches on its first token to one
// of the mcu/vendor/memory/region directive parsers. mcu (name + arch) and at
// least one memory block are required; every other directive is optional. On any
// parse failure or missing required field, all partial allocations are freed and
// NULL is returned; err is always populated with a line-numbered message in that
// case.
//
// @param path	Path to the .prof file to load
// @param err	Out-param populated with the result; only meaningful (and only
//		guaranteed non-NULL-message) when this function returns NULL
//
// @return	A fully populated mcu_profile_t pointer on success, NULL on failure
mcu_profile_t *mcu_profile_load(const char *path, hxptr_error_t *err);

// Frees an mcu_profile_t and its mem_blocks/regions arrays
//
// @param prof	The profile to free; safe to pass NULL
void mcu_profile_free(mcu_profile_t *prof);

#endif
