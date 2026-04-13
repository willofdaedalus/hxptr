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
	HXQL_RES_LIST = 1,
	HXQL_RES_INFO,
	HXQL_RES_WHICH,
} HxqlResultType_t;

typedef struct {
	hxptr_stats_t *stats;
	mcu_profile_t *profile;
	diagnostic_t *diagnostics;
	uint8_t args;
} info_result_t;


typedef struct {
	uint32_t cur_addr;
	char region[MAX_OBJ_LEN * 2];
	char memory[MAX_OBJ_LEN * 2];
	uint8_t args;
} which_result_t;

typedef struct {
	memory_entry_t *start;
	uint16_t count;
} list_result_t;

typedef struct {
	uint32_t address;
	char msg[128];
} status_result_t;

typedef struct {
	HxqlResultType_t type;
	HxptrResult_t code;
	char err[128];
	union {
		list_result_t list;
		info_result_t info;
		which_result_t which;
		status_result_t status;
	} data;
} hxql_result_t;

hxptr_error_t hxql_execute_cmd(
	hxptr_t *hx, hxsh_req_t *req, hxql_result_t *result);

#endif
