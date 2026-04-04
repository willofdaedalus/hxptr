#ifndef HXSH_TABLE_H
#define HXSH_TABLE_H

#include <stdint.h>

#define TABLE_MAX_COLS 8
#define TABLE_MAX_ROWS 64
#define TABLE_MAX_CELL 512

typedef struct {
	char headers[TABLE_MAX_COLS][TABLE_MAX_CELL];
	char rows[TABLE_MAX_ROWS][TABLE_MAX_COLS][TABLE_MAX_CELL];
	uint8_t col_count;
	uint32_t row_count;
} hxsh_table_t;

void table_init(hxsh_table_t *t);
void table_set_headers(hxsh_table_t *t, const char **headers, uint8_t count);
void table_add_row(hxsh_table_t *t, const char **cols, uint8_t count);
void table_render(hxsh_table_t *t, const char *title);

#endif
