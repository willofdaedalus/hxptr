#ifndef HXPTR_H
#define HXPTR_H

#include <stdint.h>

#define MAX_ARG_LEN           0xff
#define MAX_MEMORY_BLOCKS     16
#define MAX_REGIONS           16
#define MAX_MCU_NAME_LEN      32
#define GAP_THRESHOLD_DEFAULT 128
#define ESA_FACTOR            16
#define ADDR_COUNT            2
#define START_COUNT           4
#define MAX_BYTES             0xff
#define DISP_MAX_WIDTH        0x20
#define MAX_DIRECTIVE_LEN     0x10

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
} HxptrResult_t;

typedef enum {
	PR_OK = 0,
} ParserResult_t;

typedef enum {
	LEVEL_INFO,
	LEVEL_WARN,
	LEVEL_ERROR,
	LEVEL_FATAL,
} IssueLevel_t;

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
	uint32_t line_num; // line number for conflict resolutions
	uint8_t bytes[MAX_BYTES]; // actual data
	uint8_t byte_count; // number of bytes to help in fast rendering
	uint8_t checksum; // checksum
	uint8_t is_data_record; // tracks data records for printing
	uint16_t byte_sum; // fast way to detect gaps
} memory_entry_t;

typedef struct {
	uint32_t count;
	uint32_t cap;
	memory_entry_t *entries;
	uint8_t finalized;
} sparse_memory_t;

typedef struct {
	uint32_t first_idx; // index into memory->entries
	uint32_t second_idx; // for overlaps/gaps

	uint32_t address; // where issue starts
	uint32_t size; // size of issue (overlap/gap)

	DiagnosticType_t type;
	IssueLevel_t level;
} diagnostic_t;

typedef struct {
	diagnostic_t *entries;
	uint32_t count;
	uint32_t capacity;
} reports_t;

typedef struct {
	uint8_t data[MAX_BYTES];
	uint32_t linenum;
	uint16_t address;
	uint16_t byte_sum; // fast way to detect gaps
	uint8_t byte_count;
	uint8_t checksum;
	IHexRecordType_t type;
} hxptr_record_t;

typedef struct {
	uint32_t total_bytes;
	uint32_t total_overlaps;
	uint32_t total_gaps;
	uint32_t total_records;
	uint32_t addr_lowest;
	uint32_t addr_highest;
	uint32_t largest_gap;
} hxptr_stats_t;

typedef struct {
	char type[16];
	uint32_t start;
	uint32_t size;
} profile_memory_t;

typedef struct {
	char name[16];
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

typedef struct {
	sparse_memory_t *memory;
	reports_t *reports;
	mcu_profile_t *profile;
	hxptr_stats_t *stats;
	char *filename;
} hxptr_t;

// lifecycle
hxptr_t *hxptr_init(void);
hxptr_error_t hxptr_load_file(hxptr_t *ctx, const char *path);
void hxptr_free(hxptr_t *ctx);
// void hxptr_set_err(hxptr_t *ctx, const char *fmt, ...);

// memory operations
hxptr_error_t hxptr_finalize_memory(hxptr_t *ctx);
hxptr_error_t hxptr_get_byte(hxptr_t *ctx, uint32_t addr, uint8_t *out);
hxptr_error_t hxptr_get_range(
	hxptr_t *ctx, uint32_t start, uint32_t size, uint8_t *buf, uint32_t *len);
HxptrResult_t hxptr_print_stats(hxptr_t *hx);
hxptr_error_t hxptr_load_profile(hxptr_t *ctx, const char *path);

hxptr_error_t hxptr_ok(void);
hxptr_error_t hxptr_err(HxptrResult_t res, const char *msg, ...);
const char *hxptr_strerror(HxptrResult_t err);

// results query
// void sm_get_stats(hxptr_t *ctx, hxptr_stats_t *out);
// HxptrResult_t diag_get_count(hxptr_t *ctx);
// const diagnostic_t *diag_get(hxptr_t *ctx, uint32_t idx);
//
// HxptrResult_t diag_get_overlap_bytes(hxptr_t *ctx,
// 				     uint32_t diag_idx,
// 				     uint8_t **first_data,
// 				     uint8_t **second_data,
// 				     uint32_t *size);

#endif // HXPTR_H
