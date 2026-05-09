#include "hxsh_table.h"
#include <stdio.h>
#include <string.h>

void table_init(hxsh_table_t *t)
{
	memset(t, 0, sizeof(hxsh_table_t));
}

void table_set_headers(hxsh_table_t *t, const char **headers, uint8_t count)
{
	t->col_count = count;
	for (uint8_t i = 0; i < count; i++) {
		strncpy(t->headers[i], headers[i], TABLE_MAX_CELL - 1);
		t->widths[i] = strlen(headers[i]); // minimum width = header width
	}
}

// Call this when you already know the column widths (e.g. fixed-format output)
void table_set_widths(hxsh_table_t *t, const uint8_t *widths, uint8_t count)
{
	for (uint8_t i = 0; i < count && i < t->col_count; i++) {
		if (widths[i] > t->widths[i]) {
			t->widths[i] = widths[i];
		}
	}
}

void table_print_separator(hxsh_table_t *t)
{
	putchar('+');
	for (uint8_t c = 0; c < t->col_count; c++) {
		for (uint8_t w = 0; w < t->widths[c] + 2; w++) {
			putchar('-');
		}
		putchar('+');
	}
	putchar('\n');
}

void table_print_header(hxsh_table_t *t, const char *title)
{
	if (title && *title) {
		printf("\n%s\n", title);
	}

	table_print_separator(t);

	putchar('|');
	for (uint8_t c = 0; c < t->col_count; c++) {
		printf(" %-*s |", t->widths[c], t->headers[c]);
	}
	putchar('\n');

	table_print_separator(t);
}

void table_print_row(hxsh_table_t *t, const char **cols)
{
	putchar('|');
	for (uint8_t c = 0; c < t->col_count; c++) {
		printf(" %-*s |", t->widths[c], cols[c] ? cols[c] : "");
	}
	putchar('\n');
}
