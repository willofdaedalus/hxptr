#ifndef HXPTR_DIAGNOSTICS_H
#define HXPTR_DIAGNOSTICS_H

#include "hxptr.h"

#define REPORTS_CAPACITY 1024

// Appends a diagnostic entry to the context's report list
//
// This grows ctx->reports->entries geometrically (doubling, 16 initial capacity)
// whenever count reaches capacity. Growth failures leave the existing entries
// untouched and are reported back to the caller rather than silently dropped.
//
// @param ctx	The active engine context
// @param diag	The diagnostic entry to copy into the report list
//
// @return	HX_OK on success, HX_ERR_NULL on a NULL argument, HX_ERR_BAD_ALLOC on
//		growth failure
HxptrResult_t diag_append(hxptr_t *ctx, diagnostic_t *diag);

// Allocates and initializes a new diagnostics report list
//
// The entries array is pre-allocated to REPORTS_CAPACITY; diag_append() takes
// over growth from there once that initial capacity is exhausted.
//
// @return	An initialized reports_t pointer, or NULL on allocation failure
reports_t *reports_init(void);

// Frees a reports_t and its backing entries array
//
// @param reports	The report list to free; safe to pass NULL
void reports_free(reports_t *reports);

#endif
