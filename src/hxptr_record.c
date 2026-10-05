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

#include "hxptr_record.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// converts a single hex digit char to its 4-bit value; 17 (out of range) on
// anything that isn't 0-9/a-f/A-F
static inline uint8_t hex_char_to_nibble(const char c)
{
	if (c >= '0' && c <= '9') {
		return c - '0';
	} else if (c >= 'a' && c <= 'f') {
		return c - 'a' + 10;
	} else if (c >= 'A' && c <= 'F') {
		return c - 'A' + 10;
	}

	return 17;
}

// converts a 2-char hex pair (str[0], str[1]) into a single byte
static inline uint8_t hex_to_byte(const char *str)
{
	// check return on these function calls
	uint8_t hi = hex_char_to_nibble(str[0]);
	uint8_t lo = hex_char_to_nibble(str[1]);

	return (hi << 4) | lo;
}

// finds the ':' that starts a hex record, decodes every hex pair after it into
// out, and accumulates their sum into *sum for the checksum check later.
// out_count is set to the number of bytes actually decoded.
static HxptrResult_t ihex_to_bytes(
	const char *line, uint8_t *out, size_t *out_count, uint16_t *sum)
{
	size_t idx, hex_chars;

	if (!line || !out || !out_count) {
		return HX_ERR_NULL;
	}

	// find start of record (':')
	while (*line && *line != ':') {
		if (!isspace((unsigned char) *line)) {
			// non-whitespace before ':' is invalid
			return HX_ERR_NO_RECORD;
		}
		line++;
	}

	if (*line != ':') {
		return HX_ERR_NO_RECORD;
	}

	line++; // skip ':'

	// count hex characters (should be even)
	const char *temp = line;

	hex_chars = 0;
	while (*temp && *temp != '\n' && *temp != '\r') {
		if (!isxdigit(*temp)) {
			return HX_ERR_INV_RECORD;
		}
		hex_chars++;
		temp++;
	}

	// must be even number of hex chars, minimum 10
	// format: ll aa aa tt cc (5 bytes = 10 hex chars minimum)
	if (hex_chars < 10 || (hex_chars & 1) == 1) {
		return HX_ERR_INV_RECORD;
	}
	if (hex_chars > (size_t) (MAX_BYTE_COUNT * 2)) {
		return HX_ERR_INV_RECORD;
	}

	// convert hex pairs to bytes
	idx = 0;
	while (*line && *line != '\n' && *line != '\r' && idx < MAX_BYTE_COUNT) {
		// if (!isxdigit(line[0]) || !isxdigit(line[1])) {
		// 	// should not happen but we don't know when a photon
		// 	// from the sun would flip a bit and mess everything up
		// 	// :)
		// 	break;
		// }

		out[idx] = hex_to_byte(line);
		*sum += out[idx];
		idx++;
		line += 2;
	}

	*out_count = idx;

	return HX_OK;
}

// sums every byte in data and checks it wraps to 0 (mod 256), per the ihex
// checksum rule
static inline HxptrResult_t validate_checksum(uint8_t *data, size_t data_len)
{
	uint8_t sum = 0;

	if (!data) {
		return HX_ERR_NULL;
	}
	for (size_t i = 0; i < data_len; i++) {
		sum += data[i];
	}

	return (sum == 0) ? HX_OK : HX_ERR_BAD_CHKSM;
}

// cross-checks the declared byte count (data[0]) against the actual decoded
// length, rejects unknown record types, then hands off to validate_checksum
static HxptrResult_t validate_hex_bytes(uint8_t *data, size_t data_len)
{
	uint8_t bc, type;

	if (!data) {
		return HX_ERR_NULL;
	}

	bc = data[0];
	type = data[3];

	// accounting for 1byte bc, 2byte addr, 1byte type and 1byte checksum
	// if remainder of "physical bytes" is not bc then it's over or under count
	if ((data_len - 5) != bc) {
		return HX_ERR_INV_RECORD;
	}

	if (type > 0x05) {
		return HX_ERR_UKN_TYPE;
	}

	return validate_checksum(data, data_len);
}

HxptrResult_t hex_line_to_record(char *line, hxptr_record_t *out_record)
{
	uint8_t bytes[MAX_BYTE_COUNT] = { 0 };
	uint16_t sum = 0;
	size_t count;
	HxptrResult_t res;

	if (!line || !out_record) {
		return HX_ERR_NULL;
	}

	res = ihex_to_bytes(line, (uint8_t *) bytes, &count, &sum);
	if (res != HX_OK) {
		return res;
	}

	res = validate_hex_bytes((uint8_t *) bytes, count);
	if (res != HX_OK) {
		return res;
	}

	// safe structural allocation resize logic
	if (bytes[0] != out_record->byte_count) {
		uint8_t target_size = bytes[0];
		uint8_t *temp = realloc(out_record->data, target_size);

		// guard against genuine failures while safely ignoring returning
		// NULL when shrinking down to a size of 0 (like an eof record)
		if (!temp && target_size > 0) {
			return HX_ERR_NULL_RECORD_DATA;
		}

		out_record->data = temp;
		out_record->byte_count = target_size;
	}

	// actual record filling happens here
	out_record->address = (uint16_t) ((bytes[1] << 8) | bytes[2]);
	out_record->type = bytes[3];

	// guard memcpy to protect the memory manager if byte_count is 0
	if (out_record->byte_count > 0) {
		memcpy(out_record->data, &bytes[4], out_record->byte_count);
	}

	out_record->byte_sum = sum;
	out_record->checksum = bytes[count - 1];

	return HX_OK;
}
