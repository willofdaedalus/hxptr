#include "hxptr.h"
#include "hxptr_record.h"
#include <ctype.h>
#include <string.h>

// take input as str
// convert that to a byte array
// pass that byte array for checksum validation
// if it passes return a rec

static inline uint8_t hex_char_to_nibble(const char c)
{
	if (c >= '0' && c <= '9') {
		return c - '0';
	} else if (c >= 'a' && c <= 'f') {
		return c - 'a' + 10;
	} else if (c >= 'A' && c <= 'F') {
		return c - 'A' + 10;
	}
}

static inline uint8_t hex_to_byte(const char *str)
{
	uint8_t hi = hex_char_to_nibble(str[0]);
	uint8_t lo = hex_char_to_nibble(str[1]);

	return (hi << 4) | lo;
}

static HxptrResult_t
ihex_to_bytes(const char *line, uint8_t *out, size_t *out_count)
{
	size_t idx, hex_chars;

	if (!line || !out || !out_count) {
		return HX_ERR_NULL;
	}

	// find start of record (':')
	while (*line && *line != ':') {
		if (!isspace(*line)) {
			// non-whitespace before ':' is invalid
			return HX_ERR_INV_RECORD;
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

	// convert hex pairs to bytes
	idx = 0;
	while (*line && *line != '\n' && *line != '\r' &&
	       idx < MAX_BYTE_COUNT) {
		// if (!isxdigit(line[0]) || !isxdigit(line[1])) {
		// 	// should not happen but we don't know when a photon
		// 	// from the sun would flip a bit and mess everything up
		// 	// :)
		// 	break;
		// }

		out[idx++] = hex_to_byte(line);
		line += 2;
	}

	*out_count = idx;

	return HX_OK;
}

static HxptrResult_t validate_hex_bytes(uint8_t *data, uint8_t len)
{
	if (!data) {
		return HX_ERR_NULL;
	}
}

HxptrResult_t hex_line_to_record(char *line, hxptr_record_t *out_record)
{
	uint8_t bytes[MAX_BYTE_COUNT] = {0};
	uint8_t count;
	HxptrResult_t res;


	if (!line) {
		return HX_ERR_NULL;
	}
	res = ihex_to_bytes(line, (uint8_t *) bytes, &count);
	if (res != HX_OK) {
		return res;
	}

	res = validate_hex_bytes((uint8_t *) bytes, count);
	if (res != HX_OK) {
		return res;
	}
}
