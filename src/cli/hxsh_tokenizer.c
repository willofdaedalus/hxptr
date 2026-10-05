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

#include "hxsh_tokenizer.h"
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

HxshTokenType_t classify_token(const char *tok, token_map_t *inputs, int inputs_len)
{
	// keyword table lookup first
	if (inputs) {
		for (uint8_t i = 0; i < inputs_len; i++) {
			if (strcmp(tok, inputs[i].name) == 0) {
				return inputs[i].type;
			}
		}
		// at this point I don't expect the token to be here if it's not already
		// return TOK_UNKNOWN;
	}

	// 0x prefixed hex; unambiguous address
	if (tok[0] == '0' && (tok[1] == 'x' || tok[1] == 'X')) {
		const char *p = &tok[2];
		if (*p == '\0') {
			// bare "0x" with nothing after
			return TOK_UNKNOWN;
		}

		while (*p) {
			if (!isxdigit((unsigned char) *p)) {
				return TOK_UNKNOWN;
			}
			p++;
		}
		return TOK_HEX;
	}

	// plain decimal; all digits, no hex chars
	if (isdigit((unsigned char) tok[0])) {
		const char *p = tok;
		int is_dec = 1;

		while (*p) {
			if (!isdigit((unsigned char) *p)) {
				is_dec = 0;
				break;
			}
			p++;
		}

		if (is_dec) {
			return TOK_DEC;
		}

		// has non-decimal digit chars; check if all are hex
		p = tok;
		// should we scan from the top again??
		while (*p) {
			if (!isxdigit((unsigned char) *p)) {
				return TOK_UNKNOWN;
			}
			p++;
		}
		return TOK_BARE_HEX;
	}

	// starts with a-f/A-F; could be a bare hex pattern byte
	if (isxdigit((unsigned char) tok[0])) {
		const char *p = tok;
		while (*p) {
			if (!isxdigit((unsigned char) *p)) {
				return TOK_UNKNOWN;
			}
			p++;
		}
		return TOK_BARE_HEX;
	}

	return TOK_UNKNOWN;
}

// resolves TOK_DOT or TOK_HEX to a uint32_t
int resolve_base(hxsh_t *sh, HxshTokenType_t type, const char *raw, uint32_t *out)
{
	if (type == TOK_DOT) {
		*out = ADDR_OF(sh->cur_entry);
		return 0;
	}
	if (type == TOK_HEX || type == TOK_BARE_HEX) {
		char *end;
		*out = (uint32_t) strtoul(raw, &end, 16);
		if (end == raw || *end != '\0') {
			return -1;
		}
		return 0;
	}
	return -1;
}

// helper resolves TOK_DEC or TOK_HEX to a uint32_t count/offset
int resolve_num(HxshTokenType_t type, const char *raw, uint32_t *out)
{
	if (type == TOK_DEC) {
		char *end;
		*out = (uint32_t) strtoul(raw, &end, 10);
		if (end == raw || *end != '\0') {
			return -1;
		}
		return 0;
	}
	if (type == TOK_HEX) {
		char *end;
		*out = (uint32_t) strtoul(raw, &end, 16);
		if (end == raw || *end != '\0') {
			return -1;
		}
		return 0;
	}
	return -1;
}

int get_token(char *input, char *out_tok, uint8_t toklen)
{
	uint8_t i = 0, j = 0;

	if (!input) {
		return -1;
	}

	while (input[i] && isspace((unsigned char) input[i])) {
		i++;
	}

	if (!input[i]) {
		return 0;
	}

	// TODO; either drain the input to the next space and tell the user input is
	// truncated OR
	// return an error if the input is too long
	while (input[i] && !isspace((unsigned char) input[i])) {
		if (j < toklen - 1) {
			out_tok[j++] = input[i++];
		} else {
			return -1;
		}
	}

	out_tok[j] = '\0';
	return i;
}
