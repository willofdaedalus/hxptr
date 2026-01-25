#ifndef HXPTR_H
#define HXPTR_H

#include <stdint.h>

#define ESA_FACTOR 16
#define ADDR_COUNT 2
#define START_COUNT 4
#define MAX_BYTES 32

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
} HxptrResult_t;

typedef enum {
	LEVEL_INFO,
	LEVEL_WARN,
	LEVEL_ERROR,
	LEVEL_FATAL,
} IssueLevel_t;


typedef enum {
	IHEX_DATA = 0x0, // data
	IHEX_EOF = 0x1,	 // end of file
	IHEX_ESA = 0x2,	 // extended segment address
	IHEX_SSA = 0x3,	 // start segment address
	IHEX_ELA = 0x4,	 // extended linear address
	IHEX_SLA = 0x5	 // start linear address
} IHexRecordType_t;

typedef enum {
	DIAG_OVERLAP,
	DIAG_GAP,
	DIAG_OUT_OF_BOUNDS,
	DIAG_PROTECTED_REGION
} DiagnosticType_t;


typedef struct {
	uint32_t address;	  // contains extended hi and lo bytes
	uint32_t line_num;	  // line number for conflict resolutions
	uint8_t bytes[MAX_BYTES]; // actual data
	uint8_t byte_count;	  // number of bytes to help in fast rendering
	uint8_t checksum;	  // checksum
	uint8_t is_data_record;	  // tracks data records for printing
} memory_entry_t;

typedef struct {
	uint32_t count;
	uint32_t cap;
	memory_entry_t *entries;
	uint8_t finalized;
} sparse_memory_t;

typedef struct {
	uint32_t first_idx;  // index into memory->entries
	uint32_t second_idx; // for overlaps/gaps

	uint32_t address; // where issue starts
	uint32_t size;	  // size of issue (overlap/gap)

	DiagnosticType_t type;
	IssueLevel_t level;
} diagnostic_t;

typedef struct {
	diagnostic_t *entries;
	uint32_t count;
	uint32_t capacity;
} reports_t;


typedef struct {
	uint8_t bytes[256];
	uint32_t linenum;
	uint16_t address;
	uint8_t byte_count;
	uint8_t checksum;
	IHexRecordType_t type;
} hxptr_record_t;

typedef struct {
	uint32_t total_bytes;
	uint32_t total_ranges;
	uint32_t total_overlaps;
	uint32_t total_gaps;
	uint32_t total_records;
	uint32_t addr_lowest;
	uint32_t addr_highest;
	uint32_t largest_gap;
} hxptr_stats_t;

typedef struct {
	sparse_memory_t *memory;
	reports_t *reports;
	char *filename;

	hxptr_stats_t *stats;

	uint32_t segment;
	uint32_t linear;
	uint32_t cs;
	uint32_t ip;

	uint32_t diags_count;
	uint8_t eof;
	uint8_t cur_type;
} hxptr_t;

// lifecycle
hxptr_t *hxptr_init(void);
HxptrResult_t hxptr_load_file(hxptr_t *ctx, const char *path);
void hxptr_free(hxptr_t *ctx);

// memory operations
HxptrResult_t hxptr_finalize_memory(hxptr_t *ctx);
HxptrResult_t hxptr_get_byte(hxptr_t *ctx, uint32_t addr, uint8_t *out);
HxptrResult_t hxptr_get_range(hxptr_t *ctx,
			      uint32_t start,
			      uint32_t end,
			      uint8_t *buf,
			      uint32_t *len);

// analysis
HxptrResult_t hxptr_detect_overlaps(hxptr_t *ctx);
HxptrResult_t hxptr_detect_gaps(hxptr_t *ctx, uint32_t threshold);

// results query
// void sm_get_stats(hxptr_t *ctx, hxptr_stats_t *out);
HxptrResult_t diag_get_count(hxptr_t *ctx);
const diagnostic_t *diag_get(hxptr_t *ctx, uint32_t idx);

HxptrResult_t diag_get_overlap_bytes(hxptr_t *ctx,
				     uint32_t diag_idx,
				     uint8_t **first_data,
				     uint8_t **second_data,
				     uint32_t *size);

#endif // HXPTR_H
