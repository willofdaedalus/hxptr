#include "hxptr.h"
#include "hxptr_diagnostics.h"
#include "hxptr_memory.h"
#include "hxptr_record.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

hxptr_t *hxptr_init(void)
{
	hxptr_t *hx;

	hx = calloc(1, sizeof(hxptr_t));
	if (!hx) {
		return NULL;
	}

	hx->memory = sm_init();
	if (!hx->memory) {
		free(hx);
		return NULL;
	}

	hx->reports = reports_init();
	if (!hx->reports) {
		free(hx->memory);
		free(hx);
		return NULL;
	}

	return hx;
}

HxptrResult_t hxptr_load_file(hxptr_t *ctx, const char *path)
{
	FILE *fp;
	const char *filename;
	char buf[512];
	uint32_t linenum = 1;
	HxptrResult_t res;
	hxptr_record_t record = {0};

	if (!ctx || !path) {
		return HX_ERR_NULL;
	}

	fp = fopen(path, "r");
	if (!fp) {
		perror("fopen");
		return HX_ERR_BAD_FILE;
	}

	// get the actual filename name
	filename = strrchr(path, '/');
	filename = filename ? filename + 1 : path;

	ctx->filename = strdup(filename);
	if (!ctx->filename) {
		printf("bad alloc\n");
		return HX_ERR_BAD_ALLOC;
	}

	while (fgets(buf, sizeof(buf), fp) != NULL) {
		res = hxptr_hex_to_record(buf, &record);
		if (res != HX_OK) {
			printf("%d\n", res);
			return res;
		}

		record.linenum = linenum;
		res = hxptr_append_record(ctx, &record);
		if (res != HX_OK) {
			printf("%d\n", res);
			goto cleanup;
		}

		linenum++;
	}

	fclose(fp);

	return HX_OK;

cleanup:
	fclose(fp);
	return res;
}

void hxptr_free(hxptr_t *hx)
{
	if (!hx) {
		return;
	}

	sm_free(hx->memory);
	reports_free(hx->reports);
	free(hx);
}

static int sm_comp(const void *a, const void *b)
{
	memory_entry_t *entry_a = (memory_entry_t *) a;
	memory_entry_t *entry_b = (memory_entry_t *) b;

	if (entry_a->address < entry_b->address) {
		return -1;
	}
	if (entry_a->address > entry_b->address) {
		return 1;
	}

	// tie-breaker; compare by the line_num as most recent should always win
	if (entry_a->line_num < entry_b->address) {
		return -1;
	}
	if (entry_a->line_num > entry_b->address) {
		return 1;
	}

	return -1;
}

void radix_sort_memory(memory_entry_t *arr, size_t n)
{
	memory_entry_t *tmp = malloc(n * sizeof(memory_entry_t));
	if (!tmp)
		return; // handle allocation failure

	const size_t RADIX = 256; // 1 byte per pass
	size_t count[RADIX];
	size_t i;

	// We will do 4 passes: least-significant byte to most-significant byte
	// of address
	for (int shift = 0; shift < 32; shift += 8) {
		memset(count, 0, sizeof(count));

		// Count occurrences of each byte value
		for (i = 0; i < n; i++) {
			uint8_t byte = (arr[i].address >> shift) & 0xFF;
			count[byte]++;
		}

		// Compute prefix sums
		size_t sum = 0;
		for (i = 0; i < RADIX; i++) {
			size_t tmp_count = count[i];
			count[i] = sum;
			sum += tmp_count;
		}

		// Scatter into tmp array
		for (i = 0; i < n; i++) {
			uint8_t byte = (arr[i].address >> shift) & 0xFF;
			tmp[count[byte]++] = arr[i];
		}

		// Copy back
		memcpy(arr, tmp, n * sizeof(memory_entry_t));
	}

	free(tmp);

	// Optional: tie-breaker pass by line_num for records with same address
	size_t start = 0;
	while (start < n) {
		size_t end = start + 1;
		while (end < n && arr[end].address == arr[start].address) {
			end++;
		}
		if (end - start > 1) {
			// Sort by line_num within this range (stable sort)
			for (size_t i = start; i < end - 1; i++) {
				for (size_t j = i + 1; j < end; j++) {
					if (arr[i].line_num > arr[j].line_num) {
						memory_entry_t tmp_entry =
							arr[i];
						arr[i] = arr[j];
						arr[j] = tmp_entry;
					}
				}
			}
		}
		start = end;
	}
}


HxptrResult_t hxptr_finalize_addr(hxptr_t *hx)
{
	if (!hx) {
		return HX_ERR_NULL;
	}

	radix_sort_memory(hx->memory->entries, hx->memory->count);

	// qsort(hx->memory->entries,
	//       hx->memory->count,
	//       sizeof(memory_entry_t),
	//       sm_comp);

	hx->memory->finalized = 1;

	// more should be done here
	return sm_detect_overlaps(hx);
}

void hxptr_display_overlaps(hxptr_t *hx)
{
	if (!hx) {
		return;
	}

	if (hx->reports->count == 0) {
		printf("no overlaps detected!\n\n");
	}

	// NOTE
	// store the addresses and the count and the index it starts at for tui
	// integration in the future
	diag_print_with_highlight(hx);
}

// NOTE; have all hxptr_* functions pass a ctx; this one doesn't
HxptrResult_t hxptr_hex_to_record(char *line, hxptr_record_t *record)
{
	if (!line || !record) {
		return HX_ERR_NULL;
	}

	return hex_line_to_record(line, record);
}

HxptrResult_t hxptr_append_record(hxptr_t *ctx, hxptr_record_t *record)
{
	uint32_t addr;
	HxptrResult_t res;

	if (!ctx || !record) {
		return HX_ERR_NULL;
	}

	// no records after eof
	if (ctx->eof) {
		return HX_ERR_BAD_FILE;
	}

	switch (record->type) {
	case IHEX_DATA:
		if (ctx->cur_type == IHEX_ESA) {
			addr = ((uint32_t) ctx->segment << 4) + record->address;
		} else if (ctx->cur_type == IHEX_ELA) {
			addr = ((uint32_t) ctx->linear << 16) | record->address;
		} else {
			addr = record->address;
		}

		res = sm_append_record(ctx->memory, record, addr);
		if (res != HX_OK) {
			return res;
		}

		ctx->total_bytes += record->byte_count;
		ctx->total_records += 1;
		break;

	case IHEX_EOF:
		// we already hit eof! no more!
		if (ctx->eof) {
			return HX_ERR_BAD_FILE;
		}

		// eof MUST fit the spec
		if (record->byte_count != 0 || record->address != 0x0000) {
			return HX_ERR_INV_RECORD;
		}

		ctx->eof = 1;
		break;

	case IHEX_ESA:
		if (record->byte_count != ADDR_COUNT) {
			return HX_ERR_INV_RECORD;
		}
		ctx->segment =
			(uint16_t) ((record->bytes[0] << 8) | record->bytes[1]);
		// ctx->segment = ((uint16_t) record->bytes[0] << 8) |
		// 	       (uint16_t) record->bytes[1];

		ctx->linear = 0;
		ctx->cur_type = IHEX_ESA;
		break;

	case IHEX_ELA:
		if (record->byte_count != ADDR_COUNT) {
			return HX_ERR_INV_RECORD;
		}

		ctx->linear = ((uint16_t) record->bytes[0] << 8) |
			      (uint16_t) record->bytes[1];

		ctx->segment = 0;
		ctx->cur_type = IHEX_ELA;
		break;

	case IHEX_SSA:
		if (record->byte_count != START_COUNT || record->address != 0) {
			return HX_ERR_INV_RECORD;
		}
		ctx->cs =
			(uint16_t) ((record->bytes[0] << 8) | record->bytes[1]);
		ctx->ip =
			(uint16_t) ((record->bytes[2] << 8) | record->bytes[3]);
		ctx->cur_type = IHEX_SSA;
		break;

	case IHEX_SLA:
		if (record->byte_count != START_COUNT || record->address != 0) {
			return HX_ERR_INV_RECORD;
		}

		ctx->cs =
			(uint16_t) ((record->bytes[0] << 8) | record->bytes[1]);
		ctx->ip =
			(uint16_t) ((record->bytes[2] << 8) | record->bytes[3]);
		ctx->cur_type = IHEX_SLA;
		break;

	default:
		return HX_ERR_INV_RECORD;
	}

	return HX_OK;
}
