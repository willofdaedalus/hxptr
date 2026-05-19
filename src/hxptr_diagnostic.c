#include "hxptr_diagnostics.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

HxptrResult_t diag_append(hxptr_t *ctx, diagnostic_t *diag)
{
	if (!ctx || !ctx->reports || !diag) {
		return HX_ERR_NULL;
	}

	// grow if needed
	if (ctx->reports->count >= ctx->reports->capacity) {
		size_t new_cap = ctx->reports->capacity * 2;
		if (new_cap == 0) {
			new_cap = 16; // initial capacity
		}

		diagnostic_t *new_entries =
			realloc(ctx->reports->entries, new_cap * sizeof(diagnostic_t));
		if (!new_entries) {
			return HX_ERR_BAD_ALLOC;
		}

		ctx->reports->entries = new_entries;
		ctx->reports->capacity = new_cap;
	}

	// append
	memcpy(&ctx->reports->entries[ctx->reports->count], diag, sizeof(diagnostic_t));
	ctx->reports->count++;
	// ctx->reports->entries[ctx->reports->count++] = *diag;

	return HX_OK;
}

reports_t *reports_init(void)
{
	reports_t *reports = NULL;

	reports = calloc(1, sizeof(reports_t));
	if (!reports) {
		return NULL;
	}

	reports->capacity = REPORTS_CAPACITY;
	reports->entries = malloc(reports->capacity * sizeof(diagnostic_t));
	if (!reports->entries) {
		free(reports);
		return NULL;
	}

	return reports;
}

void reports_free(reports_t *reports)
{
	if (!reports) {
		return;
	}

	free(reports->entries);
	free(reports);
}
