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

#ifndef HXSH_HANDLERS_H
#define HXSH_HANDLERS_H

#include "hxsh.h"

// Every command handler shares this signature (cmd_handler / hxsh_cmd_t.handler):
// it parses args into the request type its command expects, writes that into
// out_data (which the caller has already sized/typed based on cmd->type), and
// returns hxptr_ok() or a descriptive error. Handlers do not execute anything
// themselves - hxql_execute_cmd() does that once the request is built - except
// for shell_only handlers (mark, load, help), which do their own work directly
// and are never passed to hxql_execute_cmd().

// Parses 'info [stats] [profile] [regions] [memory] [diag]' into an info_req_t
//
// With no args, requests everything (INFO_ALL_ARGS).
//
// @param sh		The active shell session (unused; present for the shared
//			handler signature)
// @param args		The argument string after 'info' has been stripped
// @param out_data	Populated with an info_req_t; must not be NULL
//
// @return	hxptr_ok() on success, HX_ERR_INVALID_ARG if out_data is NULL,
//		HX_ERR_UNKN_ARG on an unrecognized subcommand token
hxptr_error_t info_handler(hxsh_t *sh, const char *args, void *out_data);

// Parses 'which [addr] [region] [memory]' into a which_req_t
//
// With no args, requests all three (WHICH_MASK). Requesting 'region' or 'memory'
// without a loaded profile fails immediately, since neither can be answered.
//
// @param sh		The active shell session
// @param args		The argument string after 'which' has been stripped
// @param out_data	Populated with a which_req_t; must not be NULL
//
// @return	hxptr_ok() on success, HX_ERR_NULL if sh is NULL, HX_ERR_BAD_PROFILE
//		if region/memory was requested with no profile loaded,
//		HX_ERR_UNKN_ARG on an unrecognized subcommand token
hxptr_error_t which_handler(hxsh_t *sh, const char *args, void *out_data);

// Parses 'list [addr | addr:addr]' into a list_req_t
//
// Delegates address expression parsing to hxql_parse(). With no args, lists from
// the current position at the default page size and advances the cursor
// (adv_cur); an explicit range does not advance the cursor.
//
// @param sh		The active shell session
// @param args		The argument string after 'list' has been stripped; may
//			be empty
// @param out_data	Populated with a list_req_t; must not be NULL
//
// @return	hxptr_ok() on success, HX_ERR_NULL if sh is NULL, HX_ERR_INVALID_ARG
//		if out_data is NULL, or whatever hxql_parse() reported on a bad
//		address expression
hxptr_error_t list_handler(hxsh_t *sh, const char *args, void *out_data);

// Parses 'goto <addr>' into a goto_req_t
//
// Delegates address expression parsing to hxql_parse(). Unlike list, an address
// is required and ranges are explicitly rejected.
//
// @param sh		The active shell session
// @param args		The argument string after 'goto' has been stripped
// @param out_data	Populated with a goto_req_t; must not be NULL
//
// @return	hxptr_ok() on success, HX_ERR_NULL if sh is NULL, HX_ERR_INVALID_ARG
//		if out_data is NULL, HX_ERR_BAD_TOKEN if args is empty or resolves
//		to a range, or whatever hxql_parse() reported on a bad expression
hxptr_error_t goto_handler(hxsh_t *sh, const char *args, void *out_data);

// Parses 'find <pattern> [addr | addr:addr]', 'find next', or 'find prev' into a
// search_req_t
//
// 'next'/'prev' reuse the pattern and range from sh->search and fail if there is
// no active search (sh->search.active) to continue. A fresh pattern search
// accepts hex digits with an optional 0x prefix, must be an even number of
// digits, and is capped at HXSH_SEARCH_MAX_PATTERN bytes; an optional trailing
// address expression constrains the search range, defaulting to the full map
// when omitted.
//
// @param sh		The active shell session
// @param args		The argument string after 'find' has been stripped
// @param out_data	Populated with a search_req_t; must not be NULL
//
// @return	hxptr_ok() on success, HX_ERR_NULL if sh is NULL, HX_ERR_INVALID_ARG
//		if out_data is NULL, HX_ERR_BAD_TOKEN on a malformed or missing
//		pattern, or on 'next'/'prev' with no active search
hxptr_error_t find_handler(hxsh_t *sh, const char *args, void *out_data);

// Handles 'load file <path>' and 'load profile <path>' directly (shell_only)
//
// Delegates to hxptr_reset_file()/hxptr_reset_profile() to perform the actual
// swap. On a successful file load, resets shell navigation state (cursor,
// active search) since the old positions no longer mean anything against the
// new map; marks are intentionally left untouched. out_data is unused, since
// this handler does its own work rather than building a request for
// hxql_execute_cmd().
//
// @param sh		The active shell session
// @param args		The argument string after 'load' has been stripped
// @param out_data	Unused
//
// @return	hxptr_ok() on success, HX_ERR_NULL if sh is NULL, HX_ERR_BAD_TOKEN
//		on a missing/unknown subcommand or missing path, or whatever
//		hxptr_reset_file()/hxptr_reset_profile() reported
hxptr_error_t load_handler(hxsh_t *sh, const char *args, void *out_data);

// Handles 'mark add|rm|list|move <name> [addr]' directly (shell_only)
//
// Address resolution for 'add'/'move' is delegated to hxql_parse(); ranges are
// rejected. out_data is unused, since this handler does its own work rather than
// building a request for hxql_execute_cmd().
//
// @param sh		The active shell session
// @param args		The argument string after 'mark' has been stripped
// @param out_data	Unused
//
// @return	hxptr_ok() on success, HX_ERR_NULL if sh is NULL, HX_ERR_BAD_TOKEN
//		on a missing/unknown subcommand or missing name, or whatever the
//		underlying hxsh_mark_add()/hxsh_mark_mv()/hxsh_mark_rm() reported
hxptr_error_t mark_handler(hxsh_t *sh, const char *args, void *out_data);

// Handles 'help' and 'help <command>' directly (shell_only)
//
// With no args, prints every command's short_help. With a command name, prints
// that command's long_help in full. out_data is unused.
//
// @param sh		The active shell session (unused)
// @param args		The argument string after 'help' has been stripped; may
//			be empty
// @param out_data	Unused
//
// @return	hxptr_ok() on success, HX_ERR_UNKNOWN_CMD if the named command
//		doesn't exist in cmd_table
hxptr_error_t help_handler(hxsh_t *sh, const char *args, void *out_data);

#endif
