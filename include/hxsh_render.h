#ifndef HXSH_RENDER_H
#define HXSH_RENDER_H

#include "hxql.h"
#include "hxsh.h"

// Prints one section of a loaded mcu_profile_t as a formatted table
//
// type selects which section to print (TOK_MEMORY, TOK_REGIONS, or TOK_PROFILE);
// any other type prints an error to stderr instead. Column widths are computed
// per call by scanning the relevant array, so the table is always sized to its
// widest cell.
//
// @param profile	The profile to render; must not be NULL
// @param type		Which section to print: TOK_MEMORY, TOK_REGIONS, or
//			TOK_PROFILE
void profile_render(mcu_profile_t *profile, HxshTokenType_t type);

// Prints a 'which' result as a formatted table
//
// type is the same WHICH_MASK bitmask used by which_handler/which_result_t;
// only the rows corresponding to set bits (TOK_WH_ADDR/TOK_WH_REGION/
// TOK_WH_MEMORY) are printed.
//
// @param result	The engine result to render; must be of type HXQL_RES_WHICH
// @param type		Bitmask of which rows to print (subset of WHICH_MASK)
void which_render(hxql_result_t *result, uint8_t type);

// Prints a 'list' result as a formatted table of address/data rows
//
// Row width is derived from the widest entry actually present in the result
// (falling back to 16 bytes if every entry is empty), so short and long records
// don't force every row to the maximum possible width. Reads the raw bytes for
// each entry out of mem->data_pool at entry->data_offset.
//
// @param result	The engine result to render; must be of type HXQL_RES_LIST
// @param mem		The sparse memory layout backing the listed entries, used
//			to resolve each entry's data_offset into actual bytes
void list_render(hxql_result_t *result, sparse_memory_t *mem);

// Prints an 'info stats' result as a labeled key/value listing
//
// @param result	The engine result to render; must carry a populated
//			result->data.info.stats
void stats_render(hxql_result_t *result);

// Prints a 'goto' result and, on success, updates the shell's current position
//
// This is the one render function with a side effect: on a found result it
// looks the resolved address back up via find_entry_by_addr() and updates
// sh->cur_entry. If that lookup fails (address landed outside the map), the
// cursor is left unchanged and a warning is printed instead.
//
// @param result	The engine result to render; must be of type HXQL_RES_GOTO
// @param sh		The active shell session; sh->cur_entry may be updated
void goto_render(hxql_result_t *result, hxsh_t *sh);

// Prints a 'find' result: the matching address, or "pattern not found"
//
// @param result	The engine result to render; must be of type HXQL_RES_FIND
void search_render(hxql_result_t *result);

// Dispatches an hxql_result_t to the render function matching its type
//
// This is the single entry point hxsh_repl() calls after hxql_execute_cmd();
// individual render functions above are not normally called directly outside of
// this dispatch.
//
// @param result	The engine result to render
// @param sh		The active shell session; passed through since goto_render()
//			needs it to update the cursor
void render(hxql_result_t *result, hxsh_t *sh);

#endif
