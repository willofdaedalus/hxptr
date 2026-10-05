// Copyright 2026 Manny Quansah
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://apache.org
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "hxsh_render.h"
#include "hxql.h"
#include "hxsh_table.h"
#include <stdio.h>
#include <string.h>

void profile_render(mcu_profile_t *profile, HxshTokenType_t type)
{
	hxsh_table_t t;

	if (!profile) {
		fprintf(stderr, "you shouldn't be here...\n");
		return;
	}

	table_init(&t);

	if (type == TOK_MEMORY) {
		const char *headers[] = { "name", "start", "size" };
		table_set_headers(&t, headers, 3);

		// pre-scan name width; start is always 10, size varies
		uint8_t name_w = strlen("name");
		uint8_t size_w = strlen("size");
		char s_size[16];

		for (uint8_t i = 0; i < profile->mem_count; i++) {
			uint8_t nw = strlen(profile->mem_blocks[i].type);
			if (nw > name_w) {
				name_w = nw;
			}

			snprintf(s_size, sizeof(s_size), "%u", profile->mem_blocks[i].size);
			uint8_t sw = strlen(s_size);
			if (sw > size_w) {
				size_w = sw;
			}
		}

		const uint8_t widths[] = { name_w, 10, size_w };
		table_set_widths(&t, widths, 3);
		table_print_header(&t, "memory info");

		char s_start[12];
		for (uint8_t i = 0; i < profile->mem_count; i++) {
			profile_memory_t *mem = &profile->mem_blocks[i];
			snprintf(s_start, sizeof(s_start), "0x%08x", mem->start);
			snprintf(s_size, sizeof(s_size), "%u", mem->size);
			const char *row[] = { mem->type, s_start, s_size };
			table_print_row(&t, row);
		}

		table_print_separator(&t);

	} else if (type == TOK_REGIONS) {
		const char *headers[] = { "name", "start", "end", "protected" };
		table_set_headers(&t, headers, 4);

		// start/end always 10, protected always "protected" (9) width
		uint8_t name_w = strlen("name");
		for (uint8_t i = 0; i < profile->region_count; i++) {
			uint8_t nw = strlen(profile->regions[i].name);
			if (nw > name_w) {
				name_w = nw;
			}
		}

		const uint8_t widths[] = { name_w, 10, 10, 9 };
		table_set_widths(&t, widths, 4);
		table_print_header(&t, "regions info");

		char s_start[12], s_end[12];
		for (uint8_t i = 0; i < profile->region_count; i++) {
			profile_region_t *reg = &profile->regions[i];
			snprintf(s_start, sizeof(s_start), "0x%08x", reg->start);
			snprintf(s_end, sizeof(s_end), "0x%08x", reg->end);
			const char *row[] = { reg->name, s_start, s_end,
				reg->protected ? "yes" : "no" };
			table_print_row(&t, row);
		}

		table_print_separator(&t);

	} else if (type == TOK_PROFILE) {
		const char *headers[] = { "name", "architecture", "vendor" };
		table_set_headers(&t, headers, 3);

		// single row; widths are just the max of header vs value per column
		uint8_t widths[] = {
			(uint8_t) MAX(strlen("name"), strlen(profile->name)),
			(uint8_t) MAX(strlen("architecture"), strlen(profile->arch)),
			(uint8_t) MAX(strlen("vendor"), strlen(profile->vendor)),
		};
		table_set_widths(&t, widths, 3);
		table_print_header(&t, "profile info");

		const char *row[] = { profile->name, profile->arch, profile->vendor };
		table_print_row(&t, row);
		table_print_separator(&t);

	} else {
		fprintf(stderr, "you're not allowed here!\n");
	}
}

void which_render(hxql_result_t *result, uint8_t type)
{
	which_result_t res = result->data.which;
	hxsh_table_t t;
	uint8_t widths[] = { strlen("address"), 10 };
	const char *headers[] = { "current", "value" };

	char addr_buf[12];
	const char *row[2];
	table_init(&t);
	table_set_headers(&t, headers, 2);
	table_set_widths(&t, widths, 2);
	table_print_header(&t, "which info");

	if (type & TOK_WH_REGION) {
		row[0] = "region";
		row[1] = res.region;
		table_print_row(&t, row);
	}

	if (type & TOK_WH_ADDR) {
		snprintf(addr_buf, sizeof(addr_buf), "0x%08x", res.cur_addr);
		const char *row[] = { "address", addr_buf };
		table_print_row(&t, row);
	}

	if (type & TOK_WH_MEMORY) {
		row[0] = "memory";
		row[1] = res.memory;
		table_print_row(&t, row);
	}

	table_print_separator(&t);
}

void list_render(hxql_result_t *result, sparse_memory_t *mem)
{
	static const char hex_map[] = "0123456789abcdef";
	list_result_t res = result->data.list;
	hxsh_table_t t;
	const char *headers[] = { "address", "data" };
	char addr[12];

	uint8_t max_row_width = 0;
	for (uint16_t j = 0; j < res.count; j++) {
		if (res.start[j].byte_count > max_row_width) {
			max_row_width = res.start[j].byte_count;
		}
	}
	if (max_row_width == 0) {
		max_row_width = 16;
	}

	// widths are fully known: address is always "0x%08x" = 10 chars,
	// data is "XX XX ... XX" = (n*3)-1 chars
	uint8_t data_width = (max_row_width * 3) - 1;
	char data_str[data_width + 1]; // bounded, not a VLA inside the loop

	table_init(&t);
	table_set_headers(&t, headers, 2);
	const uint8_t widths[] = { 10, data_width };
	table_set_widths(&t, widths, 2);
	table_print_header(&t, "\rlist entries");

	for (uint16_t i = 0; i < res.count; i++) {
		memory_entry_t *entry = &res.start[i];

		snprintf(addr, sizeof(addr), "0x%08x", entry->address);
		for (uint8_t j = 0; j < max_row_width; j++) {
			int offset = j * 3;
			if (j < entry->byte_count) {
				uint8_t b = mem->data_pool[entry->data_offset + j];
				data_str[offset] = hex_map[(b >> 4) & 0x0f];
				data_str[offset + 1] = hex_map[b & 0x0f];
			} else {
				data_str[offset] = '-';
				data_str[offset + 1] = '-';
			}
			data_str[offset + 2] = (j == max_row_width - 1) ? '\0' : ' ';
		}

		const char *row[] = { addr, data_str };
		table_print_row(&t, row);
	}

	table_print_separator(&t);
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
				ADDR_OF(sh->cur_entry));

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
			list_render(result, sh->hx->memory);
			break;

		case HXQL_RES_WHICH:
			which_render(result, result->data.which.args);
			break;

		case HXQL_RES_GOTO:
			goto_render(result, sh);
			break;

		case HXQL_RES_FIND:
			search_render(result);
			break;

		default:
			fprintf(stderr, "nothing to see here... yet\n");
			break;
	}
	putchar('\n');
}

void search_render(hxql_result_t *result)
{
	search_result_t *res = &result->data.search;

	if (!res->found) {
		printf("pattern not found\n");
		return;
	}

	printf("found at 0x%08x\n", res->hit_addr);
}
