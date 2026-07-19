#ifndef HXSH_RENDER_H
#define HXSH_RENDER_H

#include "hxql.h"
#include "hxsh.h"

void profile_render(mcu_profile_t *profile, HxshTokenType_t type);
void which_render(hxql_result_t *result, uint8_t type);
void list_render(hxql_result_t *result, sparse_memory_t *mem);
void stats_render(hxql_result_t *result);
void goto_render(hxql_result_t *result, hxsh_t *sh);
void search_render(hxql_result_t *result);
void render(hxql_result_t *result, hxsh_t *sh);

#endif
