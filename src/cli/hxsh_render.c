#include "hxsh_render.h"
#include "hxql.h"
#include "hxsh_table.h"
#include <stdio.h>


void profile_render(mcu_profile_t *profile, HxshTokenType_t type)
{
	profile_region_t region;
	profile_memory_t mem;
	hxsh_table_t t;

	if (!profile) {
		// do something
		fprintf(stderr, "you shouldn't be here...\n");
		return;
	}

	table_init(&t);
	if (type == TOK_MEMORY) {
		char s_start[16], s_size[16];

		const char *headers[] = { "name", "start", "size" };
		table_set_headers(&t, headers, 3);

		for (uint8_t i = 0; i < profile->mem_count; i++) {
			mem = profile->mem_blocks[i];
			snprintf(s_start, sizeof(s_start), "0x%08x", mem.start);
			snprintf(s_size, sizeof(s_size), "%u", mem.size);
			const char *row[] = { mem.type, s_start, s_size };
			table_add_row(&t, row, 3);
		}

		table_render(&t, "memory info");

	} else if (type == TOK_REGIONS) {
		char s_start[16], s_end[16];

		const char *headers[] = { "name", "start", "end", "protected" };
		table_set_headers(&t, headers, 4);

		for (uint8_t i = 0; i < profile->region_count; i++) {
			region = profile->regions[i];
			snprintf(s_start, sizeof(s_start), "0x%08x", region.start);
			snprintf(s_end, sizeof(s_end), "0x%08x", region.end);
			const char *row[] = { region.name, s_start, s_end,
				region.protected ? "yes" : "no" };
			table_add_row(&t, row, 4);
		}

		table_render(&t, "regions info");
	} else if (type == TOK_PROFILE) {
		const char *headers[] = { "name", "architecture", "vendor" };
		const char *row[] = { profile->name, profile->arch, profile->vendor };

		table_set_headers(&t, headers, 3);
		table_add_row(&t, row, 4);
		table_render(&t, "profile info");
	} else {
		printf("you're not allowed here!\n");
	}
}

void which_render(hxql_result_t *result, uint8_t type)
{
	which_result_t res = result->data.which;

	if (type & TOK_WH_REGION) {
		printf("current region: %s\n", res.region);
	}

	if (type & TOK_WH_ADDR) {
		printf("current address: 0x%08x\n", res.cur_addr);
	}

	if (type & TOK_WH_MEMORY) {
		printf("current memory: %s\n", res.memory);
	}
}

void list_render(hxql_result_t *result)
{
	char hex_map[] = "0123456789abcdef";
	list_result_t res = result->data.list;
	hxsh_table_t t;
	const char *headers[] = { "address", "data" };

	// get max width for proper alignment
	uint8_t max_row_width = 0;
	for (uint16_t j = 0; j < res.count; j++) {
		if (res.start[j].byte_count > max_row_width) {
			max_row_width = res.start[j].byte_count;
		}
	}


	if (max_row_width == 0) {
		max_row_width = 16;
	}

	table_init(&t);
	table_set_headers(&t, headers, 2);

	for (uint16_t i = 0; i < res.count; i++) {
		memory_entry_t *entry = &res.start[i];
		char addr[16];
		// allocate string for exactly the max width found
		char data_str[(max_row_width * 3) + 1];

		snprintf(addr, sizeof(addr), "0x%08x", entry->address);
		for (uint8_t j = 0; j < max_row_width; j++) {
			int offset = j * 3;

			if (j < entry->byte_count) {
				uint8_t b = entry->bytes[j];
				data_str[offset] = hex_map[(b >> 4) & 0x0f];
				data_str[offset + 1] = hex_map[b & 0x0f];
			} else {
				// pad the "empty" space with dashes
				data_str[offset] = '-';
				data_str[offset + 1] = '-';
			}
			// space between bytes, null at the very end of the string
			data_str[offset + 2] = (j == max_row_width - 1) ? '\0' : ' ';
		}

		const char *row[] = { addr, data_str };
		table_add_row(&t, row, 2);
	}

	table_render(&t, "list entries");
}

void stats_render(hxql_result_t *result)
{
	hxptr_stats_t *s = result->data.info.stats;

	printf("\nstats info\n");
	printf("  %-20s %u\n", "total bytes", s->total_bytes);
	printf("  %-20s %u\n", "total records", s->total_records);
	printf("  %-20s %u\n", "total overlaps", s->total_overlaps);
	printf("  %-20s %u\n", "total gaps", s->total_gaps);
	printf("  %-20s %u bytes\n", "largest gap", s->largest_gap);
	printf("  %-20s 0x%08x\n", "lowest address", s->addr_lowest);
	printf("  %-20s 0x%08x\n", "highest address", s->addr_highest);
}

void goto_render(hxql_result_t *result, hxsh_t *sh)
{
	goto_result_t res = result->data.go;
	memory_entry_t *entry;

	if (res.found) {
		entry = find_entry_by_addr(sh->hx->memory, res.addr);
		if (!entry) {
			fprintf(stderr,
				"new address is outside upper boundary of hex file map; resetting "
				"to previous address of 0x%08x",
				CUR_ADDR(sh));

			return;
		}
		sh->cur_entry = entry - sh->hx->memory->entries;

		printf("updated current address to %#08x", res.addr);
		return;
	}
	printf("not found\n");
}

void render(hxql_result_t *result, hxsh_t *sh)
{
	info_result_t res;
	if (!result) {
		return;
	}

	switch (result->type) {
		case HXQL_RES_INFO:
			res = result->data.info;

			if (res.args & INFO_PROF_MASK) {
				if (!res.profile) {
					fprintf(stderr,
						"no mcu profile loaded for arguments 'memory', "
						"'regions' and 'profile'");
				} else {
					if (res.args & TOK_MEMORY) {
						profile_render(res.profile, TOK_MEMORY);
					}
					if (res.args & TOK_REGIONS) {
						profile_render(res.profile, TOK_REGIONS);
					}
					if (res.args & TOK_PROFILE) {
						profile_render(res.profile, TOK_PROFILE);
					}
				}
			}

			if (res.args & TOK_STATS) {
				stats_render(result);
			}
			break;

		case HXQL_RES_LIST:
			list_render(result);
			break;

		case HXQL_RES_WHICH:
			which_render(result, result->data.which.args);
			break;

		case HXQL_RES_GOTO:
			goto_render(result, sh);
			break;

		default:
			fprintf(stderr, "nothing to see here... yet\n");
			break;
	}
	putchar('\n');
}
