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

// Zeroes an hxsh_table_t, ready for a fresh table
//
// @param t	The table to initialize
void table_init(hxsh_table_t *t);

// Sets a table's column headers, seeding each column's width to its header's
// length
//
// This must be called before table_set_widths(), since table_set_widths() only
// ever widens a column relative to whatever width is already set.
//
// @param t		The table to set headers on; must already be initialized
// @param headers	Array of header strings, one per column
// @param count	Number of columns; also sets t->col_count
void table_set_headers(hxsh_table_t *t, const char **headers, uint8_t count);

// Widens columns to fit known content widths
//
// Call this once header widths are known but before any rows are printed (e.g.
// after pre-scanning the data to render). Each column's width is only ever
// increased, never shrunk below its header width; a column's width can be
// widened by multiple calls if needed.
//
// @param t		The table to widen; must already have headers set
// @param widths	Minimum width for each column, in the same order as headers
// @param count	Number of entries in widths; capped at t->col_count
void table_set_widths(hxsh_table_t *t, const uint8_t *widths, uint8_t count);

// Prints a title line (if non-empty) followed by a header row and its separators
//
// @param t		The table to print the header for; headers and widths
//			must already be set
// @param title	Optional title printed above the table; pass NULL or "" to
//			omit it
void table_print_header(hxsh_table_t *t, const char *title);

// Prints a single data row
//
// @param t		The table being printed to
// @param cols		Array of cell strings, one per column; a NULL entry
//			renders as an empty cell
void table_print_row(hxsh_table_t *t, const char **cols);

// Prints a '+---+---+' style separator line sized to the table's current widths
//
// @param t	The table to print a separator for
void table_print_separator(hxsh_table_t *t);

#endif
