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
	}
}

void table_add_row(hxsh_table_t *t, const char **cols, uint8_t count)
{
	if (t->row_count >= TABLE_MAX_ROWS) {
		return;
	}

	for (uint8_t i = 0; i < count && i < t->col_count; i++) {
		strncpy(t->rows[t->row_count][i], cols[i], TABLE_MAX_CELL - 1);
	}

	t->row_count++;
}

void table_render(hxsh_table_t *t, const char *title)
{
	// calculate column widths
	uint8_t widths[TABLE_MAX_COLS] = { 0 };

	for (uint8_t c = 0; c < t->col_count; c++) {
		widths[c] = strlen(t->headers[c]);
		for (uint32_t r = 0; r < t->row_count; r++) {
			uint8_t len = strlen(t->rows[r][c]);
			if (len > widths[c]) {
				widths[c] = len;
			}
		}
	}

	// total width for the separator line
	uint32_t total = 1; // leading '+'
	for (uint8_t c = 0; c < t->col_count; c++) {
		total += widths[c] + 3; // padding + '+'
	}

	// title
	if (title && *title) {
		printf("\n%s\n", title);
	}

// separator helper
#define SEPARATOR()                                     \
	do {                                                \
		putchar('+');                                   \
		for (uint8_t c = 0; c < t->col_count; c++) {    \
			for (uint8_t w = 0; w < widths[c] + 2; w++) \
				putchar('-');                           \
			putchar('+');                               \
		}                                               \
		putchar('\n');                                  \
	} while (0)

	// top border
	SEPARATOR();

	// headers
	putchar('|');
	for (uint8_t c = 0; c < t->col_count; c++) {
		printf(" %-*s |", widths[c], t->headers[c]);
	}
	putchar('\n');

	// header separator
	SEPARATOR();

	// rows
	for (uint32_t r = 0; r < t->row_count; r++) {
		putchar('|');
		for (uint8_t c = 0; c < t->col_count; c++) {
			printf(" %-*s |", widths[c], t->rows[r][c]);
		}
		putchar('\n');
	}

	// bottom border
	SEPARATOR();

#undef SEPARATOR
}
