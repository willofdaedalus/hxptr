#ifndef HXQL_H
#define HXQL_H

#include "hxptr.h"
#include "hxsh.h"

// search deadbeef
// goto 08000000
// list 0x80000:0x80010
// list 080000
// list +15

typedef enum {
	HXQL_RESULT_ENTRIES,
	HXQL_RESULT_SEARCH,
	HXQL_RESULT_INFO,
	HXQL_RESULT_STATUS,
} HxqlResultType_t;

typedef struct {
	hxptr_stats_t *stats;

} info_result_t;

typedef struct {
	HxqlResultType_t type;
	HxptrResult_t code;
	char err[128];
	union {
		struct {
			memory_entry_t *entries;
			uint32_t count;
		} entries;
		struct {
			memory_entry_t *matches;
			uint32_t count;
			uint32_t next_addr;
		} search;
		info_result_t info;
		struct {
			uint32_t address;
			char msg[128];
		} status;
	} data;
} hxql_result_t;

hxptr_error_t hxql_execute_cmd(
	hxsh_t *sh, hxsh_req_t *req, hxql_result_t *result);

#endif
