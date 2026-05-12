#ifndef HXSH_HANDLERS_H
#define HXSH_HANDLERS_H

#include "hxsh.h"

hxptr_error_t info_handler(hxsh_t *sh, const char *args, void *out_data);
hxptr_error_t which_handler(hxsh_t *sh, const char *args, void *out_data);
hxptr_error_t list_handler(hxsh_t *sh, const char *args, void *out_data);
hxptr_error_t goto_handler(hxsh_t *sh, const char *args, void *out_data);
hxptr_error_t find_handler(hxsh_t *sh, const char *args, void *out_data);

#endif
