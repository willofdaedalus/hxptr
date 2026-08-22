#ifndef HXPTR_RECORD_H
#define HXPTR_RECORD_H

// 260 because [len:1]+[addr:2]+[type:1]+[data:255]+[chksm:1]
#define MAX_BYTE_COUNT 0x104
#include "hxptr.h"

// Parses a single line of an Intel HEX file into a record
//
// This validates the line as it goes: the ':' start marker, an even number of
// hex digits, the declared byte count against the actual data length, the record
// type (must be 0x00-0x05), and the record checksum. out_record->data is grown
// with realloc() to fit the parsed byte count on demand, so out_record can (and
// should) be reused across calls to avoid a fresh allocation per line; its
// byte_count is updated to reflect the *declared* count even when validation
// fails partway through, since that's the most accurate value available for
// error reporting at the call site.
//
// @param line		A single line of hex text (leading whitespace before ':'
//			is tolerated; trailing content past the checksum is not
//			required to be present)
// @param out_record	Record to populate; data is reallocated as needed and must
//			be initialized to a valid realloc()-able state (zeroed or
//			previously populated by this function) before the first call
//
// @return	HX_OK on success, HX_ERR_NULL on a NULL argument, HX_ERR_NO_RECORD
//		if no ':' marker is found, HX_ERR_INV_RECORD on a malformed byte
//		count or hex digit, HX_ERR_UKN_TYPE for an out-of-range record
//		type, HX_ERR_BAD_CHKSM on checksum mismatch, or
//		HX_ERR_NULL_RECORD_DATA if growing out_record->data fails
HxptrResult_t hex_line_to_record(char *line, hxptr_record_t *out_record);

#endif
