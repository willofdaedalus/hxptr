#include "hxptr.h"
#include "hxptr_record.h"

HxptrResult_t hxptr_hex_to_record(char *line, hxptr_record_t *record)
{
	if (!line || !record) {
		return HX_ERR_NULL;
	}

	return hex_line_to_record(line, record);
}
