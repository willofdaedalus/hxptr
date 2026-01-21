#ifndef HXPTR_DIAGNOSTICS_H
#define HXPTR_DIAGNOSTICS_H

#include "hxptr.h"

#define REPORTS_CAPACITY 1024

HxptrResult_t diag_append(hxptr_t *ctx, diagnostic_t *diag);
reports_t *reports_init(void);
void reports_free(reports_t *reports);
void diag_print_compact(hxptr_t *ctx);
void diag_print(hxptr_t *ctx);

#endif
