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

#ifndef HXSH_TOKENIZER_H
#define HXSH_TOKENIZER_H

#include "hxsh.h"

// Classifies a raw token string into a HxshTokenType_t
//
// Checks inputs (a caller-supplied keyword table) first, then falls back to
// structural classification: 0x-prefixed hex (TOK_HEX), plain digits
// (TOK_DEC), digits containing a-f/A-F (TOK_BARE_HEX), or a token starting
// with a-f/A-F that is entirely hex digits (also TOK_BARE_HEX). Anything that
// matches none of these is TOK_UNKNOWN.
//
// @param tok		The token to classify (NUL-terminated)
// @param inputs	Optional keyword table checked before structural
//			classification; pass NULL to skip straight to it
// @param inputs_len	Number of entries in inputs
//
// @return	The matching HxshTokenType_t, or TOK_UNKNOWN if nothing matches
HxshTokenType_t classify_token(const char *tok, token_map_t *inputs, int inputs_len);

// Resolves a TOK_DOT or TOK_HEX/TOK_BARE_HEX token to its literal address
//
// TOK_DOT resolves to the shell's current address (ADDR_OF(sh->cur_entry)); hex
// tokens are parsed with strtoul() base 16 regardless of whether they carried a
// 0x prefix.
//
// @param sh	The active shell session; only consulted for TOK_DOT
// @param type	The already-classified token type; anything other than TOK_DOT,
//		TOK_HEX, or TOK_BARE_HEX is rejected
// @param raw	The raw token text to parse (for hex types)
// @param out	Out-param set to the resolved address on success
//
// @return	0 on success, -1 if type is unsupported or raw doesn't fully parse
int resolve_base(hxsh_t *sh, HxshTokenType_t type, const char *raw, uint32_t *out);

// Resolves a TOK_DEC or TOK_HEX token to a plain numeric count/offset
//
// Unlike resolve_base(), this has no notion of "current position" - it's used
// for the numeric part of +/- offsets and similar bare counts, not addresses.
//
// @param type	The already-classified token type; anything other than TOK_DEC
//		or TOK_HEX is rejected
// @param raw	The raw token text to parse
// @param out	Out-param set to the resolved value on success
//
// @return	0 on success, -1 if type is unsupported or raw doesn't fully parse
int resolve_num(HxshTokenType_t type, const char *raw, uint32_t *out);

// Reads the next whitespace-delimited token out of input
//
// Leading whitespace is skipped before the token starts; the token itself stops
// at the next whitespace character or the end of the string. Does not modify
// input.
//
// @param input	The string to read from
// @param out_tok	Buffer the token is copied into, NUL-terminated
// @param toklen	Size of out_tok, including room for the terminator
//
// @return	Number of characters consumed from input (including skipped
//		leading whitespace) on success, 0 if input is empty/all
//		whitespace, -1 if input is NULL or the token would overflow
//		out_tok
int get_token(char *input, char *out_tok, uint8_t toklen);


#endif
