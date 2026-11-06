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

#ifndef HXPTR_H
#define HXPTR_H

#include <stdint.h>

#define MAX_ARG_LEN           0xff
#define MAX_MEMORY_BLOCKS     0x10
#define MAX_REGIONS           0x10
#define MAX_MCU_NAME_LEN      0x20
#define GAP_THRESHOLD_DEFAULT 0x80
#define ESA_FACTOR            0x10
#define ADDR_COUNT            0x02
#define START_COUNT           0x04
#define MAX_BYTES             0x10
#define DISP_MAX_WIDTH        0x20
#define MAX_DIRECTIVE_LEN     0x10
#define MAX_OBJ_LEN           0x10

typedef enum {
	HX_OK = 0,
	HX_ERR_INV_RECORD = -1,
	HX_ERR_NULL = -2,
	HX_ERR_NO_RECORD = -3,
	HX_ERR_UKN_TYPE = -4,
	HX_ERR_BAD_CHKSM = -5,
	HX_ERR_BAD_APPEND = -6,
	HX_ERR_BAD_FILE = -7,
	HX_ERR_BAD_ALLOC = -8,
	HX_ERR_NOT_FINALIZED = -9,
	HX_ERR_NO_EOF = -10,
	HX_ERR_NULL_RECORD_DATA = -11,

	// parser error codes
	HX_ERR_PARSE = -30,
	HX_ERR_PARSE_REGION = -31,
	HX_ERR_PARSE_MEMORY = -32,
	HX_ERR_PARSE_DEETS = -33,
	HX_ERR_PARSE_BAD_VALUE = -34,
	HX_ERR_OVERFLOW = -35,
	HX_ERR_BAD_CONFIG = -36,
	HX_ERR_BAD_PROFILE = -37,
	HX_ERR_UNKN_DIRECTIVE = -38,

	// shell error codes
	HX_ERR_UNKN_ARG = -50,
	HX_ERR_UNKNOWN_CMD = -51,
	HX_ERR_INVALID_ARG = -52,
	HX_ERR_BAD_TOKEN = -53,
	HX_ERR_MARKS_FULL = -54,
	HX_ERR_MARKS_404 = -55,
	HX_ERR_MARKS_DUP = -56,
} HxptrResult_t;

typedef enum {
	IHEX_DATA = 0x0, // data
	IHEX_EOF = 0x1, // size of file
	IHEX_ESA = 0x2, // extended segment address
	IHEX_SSA = 0x3, // start segment address
	IHEX_ELA = 0x4, // extended linear address
	IHEX_SLA = 0x5 // start linear address
} IHexRecordType_t;

typedef enum {
	DIAG_OVERLAP,
	DIAG_GAP,
	DIAG_OUT_OF_BOUNDS,
	DIAG_PROTECTED_REGION
} DiagnosticType_t;

typedef struct {
	char msg[256];
	HxptrResult_t code;
} hxptr_error_t;

typedef struct {
	uint32_t address; // contains extended hi and lo bytes
	uint32_t linenum; // line number for conflict resolutions
	uint32_t data_offset; // keeps track of the offset within the data pool
	uint8_t byte_count; // number of bytes to help in fast rendering
	uint8_t checksum; // checksum
	uint8_t is_data_record; // tracks data records for printing
	uint16_t byte_sum; // fast way to detect gaps
} memory_entry_t;

typedef struct {
	uint32_t count;
	uint32_t cap;
	memory_entry_t *entries;
	uint8_t *data_pool;
	uint32_t pool_len;
	uint32_t pool_cap;
	uint8_t finalized;
} sparse_memory_t;

typedef struct {
	uint32_t first_idx; // index into memory->entries
	uint32_t second_idx; // for overlaps/gaps

	uint32_t address; // where issue starts
	uint32_t size; // size of issue (overlap/gap)

	DiagnosticType_t type;
} diagnostic_t;

typedef struct {
	diagnostic_t *entries;
	uint32_t count;
	uint32_t capacity;
} reports_t;

typedef struct {
	uint8_t *data;
	uint32_t linenum;
	uint16_t address;
	uint16_t byte_sum; // fast way to detect gaps
	uint8_t byte_count;
	uint8_t checksum;
	IHexRecordType_t type;
} hxptr_record_t;

typedef struct {
	uint32_t gap_start_addr;
	uint32_t gap_stop;
} hxptr_gap_t;

typedef struct {
	uint16_t longest_record; // this is a private entry for address resolves
	uint32_t total_bytes;
	uint32_t total_overlaps;
	uint32_t total_gaps;
	uint32_t total_records;
	uint32_t addr_lowest;
	uint32_t addr_highest;
	uint32_t largest_gap;
} hxptr_stats_t;

typedef struct {
	char type[MAX_OBJ_LEN];
	uint32_t start;
	uint32_t size;
} profile_memory_t;

typedef struct {
	char name[MAX_OBJ_LEN];
	uint32_t start;
	uint32_t end;
	uint8_t protected;
} profile_region_t;

typedef struct {
	char name[MAX_MCU_NAME_LEN];
	char arch[MAX_MCU_NAME_LEN];
	char vendor[MAX_MCU_NAME_LEN];
	profile_memory_t *mem_blocks;
	profile_region_t *regions;
	uint8_t region_count;
	uint8_t mem_count;
} mcu_profile_t;

// Main application context for the hxptr engine
// Tracks the core sparse memory model, analytics and profiles
typedef struct {
	// The sparse memory layout holding resolved hex data in a linear format sorted
	// by ascending address manner using the in-built qsort function
	sparse_memory_t *memory;

	// Reports contains any diagnostics or issues including address gaps above a
	// certain threshold, detected while parsing a hex file. boundary violations and
	// address overlaps
	reports_t *reports;

	// Profile holds the definitions of the target microcontroller's hardware,
	// address boundaries, named regions and memory blocks
	mcu_profile_t *profile;

	// Contains the statistics accumulated while parsing a hex file including record
	// count, bytes processed, overlaps counts and more
	hxptr_stats_t *stats;

	// The name of the hex file under inspection without the relative/absolute path
	char *filename;
} hxptr_t;

// Initializes a new dynamically allocated context engine instance
//
// This sets up the root structures including the sparse memory layout,
// internal tracking metrics, and diagnostic reporting subsystems.
//
// @return	An initialized hxptr context pointer or NULL on allocation failure.
hxptr_t *hxptr_init(void);

// Loads and parses a hex file into records for the sparse memory layout
//
// This function loads up the hex file under inspection and parses each line in the
// file into internal records for keeping in the sparse memory layout. The function
// parses each record in order and does not sort or arrange the records into a serial
// form in the sparse memory.
//
// @param ctx 	The active engine context
// @param path 	Path to the hex file to load and parse
// @return	- hxptr_error_t containing an error message and code
hxptr_error_t hxptr_load_file(hxptr_t *ctx, const char *path);

// Safely frees the hxptr context engine including any allocations made in hxptr_init
//
// @ctx 	- The active engine context to be freed
void hxptr_free(hxptr_t *ctx);

// Replaces the current profile with a fresh loaded one
//
// This function frees and replaces the old profile successfully only when the new
// one loads without issues otherwise leaving the old profile intact in the case of a
// failed load
//
// @param ctx	The active engine context
// @param path	The path to the new profile
//
// @return	hxptr_error_t containing an error message and error code
hxptr_error_t hxptr_reset_profile(hxptr_t *ctx, const char *path);

// Replaces the current hex file state with a fresh one
//
// This function only swaps out the old hex file's state when all new allocations are
// validated. A unsuccessful load leaves the old state intact without any issues.
//
// @param ctx	The active engine context
// @param path	The path to the new profile
//
// @return	hxptr_error_t containing an error message and error code
hxptr_error_t hxptr_reset_file(hxptr_t *ctx, const char *path);

// Resolves all address ordering to achieve a linear memory map of the hex file's
// data
//
// This function sorts all records in the sparse memory by address to get a linear
// ascending structure of all the records. This helps to resolve out of order
// addresses that might be present in a hex file
//
// @param ctx	The active engine context
//
// @return	hxptr_error_t containing an error message and error code
hxptr_error_t hxptr_finalize_memory(hxptr_t *ctx);

// Loads a profile containing MCU hardware definitions and memory layout
//
// @param ctx	The active engine context
// @param path	The path to the new profile
//
// @return	hxptr_error_t containing an error message and error code
hxptr_error_t hxptr_load_profile(hxptr_t *ctx, const char *path);

// Builds a "no error" result
//
// This is a convenience constructor for the common case of returning success from
// a function whose signature is hxptr_error_t. The returned value always carries
// HX_OK and an empty message.
//
// @return	hxptr_error_t with code HX_OK
hxptr_error_t hxptr_ok(void);

// Builds an error result with a formatted message
//
// This is the single construction point for every non-OK hxptr_error_t returned
// throughout the codebase. msg is a printf-style format string; the resulting
// message is truncated to fit hxptr_error_t.msg if it would otherwise overflow.
//
// @param res	The error code to attach to the result
// @param msg	printf-style format string describing the error
// @param ...	Format arguments for msg
//
// @return	hxptr_error_t with code res and the formatted message
hxptr_error_t hxptr_err(HxptrResult_t res, const char *msg, ...);

// Translates an HxptrResult_t code into a short, static, human-readable string
//
// This does not include any contextual information (line numbers, file paths,
// etc); callers that want that should prefer the message on an hxptr_error_t
// built via hxptr_err() instead.
//
// @param err	The error code to translate
//
// @return	A static string describing err; "unknown error" for unrecognized codes
const char *hxptr_strerror(HxptrResult_t err);

#endif // HXPTR_H
