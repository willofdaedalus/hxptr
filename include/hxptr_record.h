#ifndef HXPTR_RECORD_H
#define HXPTR_RECORD_H

// 260 because [len:1]+[addr:2]+[type:1]+[data:255]+[chksm:1]
#define MAX_BYTE_COUNT 0x104
#include "hxptr.h"

HxptrResult_t hex_line_to_record(char *line, hxptr_record_t *out_record);

#endif
