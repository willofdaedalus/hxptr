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
		if (new_cap == 0)
			new_cap = 16; // initial capacity

		diagnostic_t *new_entries = realloc(
			ctx->reports->entries, new_cap * sizeof(diagnostic_t));
		if (!new_entries) {
			return HX_ERR_BAD_ALLOC;
		}

		ctx->reports->entries = new_entries;
		ctx->reports->capacity = new_cap;
	}

	// append
	memcpy(&ctx->reports->entries[ctx->reports->count],
	       diag,
	       sizeof(diagnostic_t));
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

	// Free each diagnostic's data arrays
	for (size_t i = 0; i < reports->count; i++) {
		free(reports->entries[i].first_data);
		free(reports->entries[i].second_data);
	}

	// Free the entries array
	free(reports->entries);

	// Free the reports struct
	free(reports);
}

void diag_print_compact(hxptr_t *ctx)
{
	if (!ctx || !ctx->reports || ctx->reports->count == 0) {
		return;
	}

	printf("\n%zu overlap(s) detected:\n\n", ctx->reports->count);

	for (size_t i = 0; i < ctx->reports->count; i++) {
		diagnostic_t *d = &ctx->reports->entries[i];
		uint32_t size = d->overlap_end - d->overlap_start + 1;

		printf("  [%zu] 0x%08x-0x%08x (%u bytes) │ l%u → l%u\n",
		       i + 1,
		       d->overlap_start,
		       d->overlap_end,
		       size,
		       d->first_write_line,
		       d->second_write_line);
	}
	printf("\n");
}

void diag_print(hxptr_t *ctx)
{
	if (!ctx || !ctx->reports) {
		return;
	}

	if (ctx->reports->count == 0) {
		printf("✓ No overlaps detected\n");
		return;
	}

	printf("\n");
	printf("===  OVERLAPS DETECTED: %zu ===\n", ctx->reports->count);
	printf("\n");

	for (size_t i = 0; i < ctx->reports->count; i++) {
		diagnostic_t *d = &ctx->reports->entries[i];
		uint32_t overlap_size = d->overlap_end - d->overlap_start + 1;

		printf("overlap #%zu:\n", i + 1);
		printf("  address range:  0x%08x - 0x%08x (%u bytes)\n",
		       d->overlap_start,
		       d->overlap_end,
		       overlap_size);
		printf("  first write:    line %u\n", d->first_write_line);
		printf("  overwritten by: line %u\n", d->second_write_line);

		// optional: show severity
		const char *level_str;
		switch (d->level) {
		case LEVEL_INFO:
			level_str = "INFO";
			break;
		case LEVEL_WARN:
			level_str = "WARNING";
			break;
		case LEVEL_ERROR:
			level_str = "ERROR";
			break;
		case LEVEL_FATAL:
			level_str = "FATAL";
			break;
		default:
			level_str = "?";
			break;
		}
		printf("  severity:       %s\n", level_str);
		printf("\n");
	}

	printf("Total overlaps: %zu\n", ctx->reports->count);
}

void diag_print_with_highlight(hxptr_t *ctx)
{
	if (!ctx || !ctx->reports) {
		return;
	}

	if (ctx->reports->count == 0) {
		printf("✓ No overlaps detected\n");
		return;
	}

	printf("\n%zu overlap(s) detected:\n\n", ctx->reports->count);

	for (size_t i = 0; i < ctx->reports->count; i++) {
		diagnostic_t *d = &ctx->reports->entries[i];
		uint32_t size = d->overlap_end - d->overlap_start + 1;

		printf("overlap #%zu: 0x%08x-0x%08x (%u bytes) line: %u -> "
		       "line :%u\n",
		       i + 1,
		       d->overlap_start,
		       d->overlap_end,
		       size,
		       d->first_write_line,
		       d->second_write_line);

		// Show before (first write) - limit to 16 bytes for readability
		printf("  before: ");
		uint32_t display_size = (size > 16) ? 16 : size;
		for (uint32_t j = 0; j < display_size; j++) {
			printf("%02x ", d->first_data[j]);
		}
		if (size > 16) {
			printf("... (%u more bytes)", size - 16);
		}
		printf("\n");

		// show after (second write) with color highlighting
		printf("  after:  ");
		for (uint32_t j = 0; j < display_size; j++) {
			if (d->first_data[j] != d->second_data[j]) {
				// red for changed bytes
				printf("\033[31m%02x\033[0m ",
				       d->second_data[j]);
			} else {
				printf("%02x ", d->second_data[j]);
			}
		}
		if (size > 16) {
			printf("... (%u more bytes)", size - 16);
		}
		printf("\n\n");
	}
}
// void diag_print_with_highlight(hxptr_t *ctx)
// {
// 	if (!ctx || !ctx->reports || ctx->reports->count == 0) {
// 		printf("✓ No overlaps detected\n");
// 		return;
// 	}
//
// 	printf("\n⚠ %zu overlap(s) detected:\n\n", ctx->reports->count);
//
// 	for (size_t i = 0; i < ctx->reports->count; i++) {
// 		diagnostic_t *d = &ctx->reports->entries[i];
// 		uint32_t size = d->overlap_end - d->overlap_start + 1;
//
// 		printf("Overlap #%zu: 0x%08X-0x%08X (%u bytes) L%u→L%u\n",
// 		       i + 1,
// 		       d->overlap_start,
// 		       d->overlap_end,
// 		       size,
// 		       d->first_write_line,
// 		       d->second_write_line);
//
// 		// Show before (first write)
// 		printf("  Before: ");
// 		for (uint32_t j = 0; j < size && j < 16; j++) {
// 			printf("%02X ", d->first_data[j]);
// 		}
// 		if (size > 16)
// 			printf("...");
// 		printf("\n");
//
// 		// Show after (second write)
// 		printf("  After:  ");
// 		for (uint32_t j = 0; j < size && j < 16; j++) {
// 			if (d->first_data[j] != d->second_data[j]) {
// 				printf("\033[31m%02X\033[0m ",
// 				       d->second_data[j]); // Red
// 			} else {
// 				printf("%02X ", d->second_data[j]);
// 			}
// 		}
// 		if (size > 16)
// 			printf("...");
// 		printf("\n\n");
// 	}
// }
