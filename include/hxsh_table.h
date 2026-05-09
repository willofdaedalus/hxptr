#ifndef HXSH_TABLE_H
#define HXSH_TABLE_H

#include <stdint.h>

#define TABLE_MAX_COLS 4
#define TABLE_MAX_CELL 64

typedef struct {
	uint8_t col_count;
	uint8_t widths[TABLE_MAX_COLS];
	char headers[TABLE_MAX_COLS][TABLE_MAX_CELL];
} hxsh_table_t;

void table_init(hxsh_table_t *t);
void table_set_headers(hxsh_table_t *t, const char **headers, uint8_t count);
void table_set_widths(hxsh_table_t *t, const uint8_t *widths, uint8_t count);
void table_print_header(hxsh_table_t *t, const char *title);
void table_print_row(hxsh_table_t *t, const char **cols);
void table_print_separator(hxsh_table_t *t);

#endif
