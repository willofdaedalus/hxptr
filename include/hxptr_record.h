#ifndef HXPTR_RECORD_H
#define HXPTR_RECORD_H

#define MAX_BYTE_COUNT 0x105
#include "hxptr.h"

HxptrResult_t hex_line_to_record(char *line, hxptr_record_t *out_record);

#endif
