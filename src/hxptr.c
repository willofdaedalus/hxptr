#include "hxptr.h"
#include "hxptr_memory.h"
#include "hxptr_record.h"

HxptrResult_t hxptr_hex_to_record(char *line, hxptr_record_t *record)
{
	if (!line || !record) {
		return HX_ERR_NULL;
	}

	return hex_line_to_record(line, record);
}

HxptrResult_t hxptr_append_record(hxptr_t *ctx, hxptr_record_t *record)
{
	// check type of record
	// update internal hi address
	// push this to the sm_handlers

	if (!record) {
		return HX_ERR_NULL;
	}

	switch (record->type) {
	case IHEX_DATA:
		break;

	case IHEX_EOF:
		break;

	case IHEX_ESA:
		break;

	case IHEX_SSA:
		break;

	case IHEX_ELA:
		break;

	case IHEX_SLA:
		break;
	}
}
