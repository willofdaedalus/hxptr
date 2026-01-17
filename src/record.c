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
	}
	if (c >= 'a' && c <= 'f') {
		return c - 'a' + 10;
	}
	if (c >= 'A' && c <= 'F') {
		return c - 'A' + 10;
	}

	return 0;
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

static HxptrResult_t validate_hex_bytes(uint8_t *data, size_t data_len)
{
	uint8_t bc, type;
	uint16_t sum;
	size_t i;

	if (!data) {
		return HX_ERR_NULL;
	}

	bc = data[0];
	type = data[3];

	// accounting for 1byte bc, 2byte addr, 1byte type and 1byte checksum
	// if remainder is not bc then it's over or under count
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
	uint8_t bytes[MAX_BYTE_COUNT] = {0};
	size_t count;
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

	// actual record filling happens here
	out_record->byte_count = bytes[0];
	out_record->address = (uint16_t) ((bytes[1] << 8) | bytes[2]);

	out_record->type = bytes[3];
	memcpy(out_record->bytes, &bytes[4], out_record->byte_count);

	out_record->checksum = line[count - 1];

	return HX_OK;
}
