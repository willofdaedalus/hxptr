#include "hxptr_profile.h"
#include "hxptr.h"
#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *skip_whitespace(char *p)
{
	while (*p && isspace((unsigned char) *p)) {
		p++;
	}

	return p;
}

// get next token
// returns pointer to start, advances line
static char *next_token(char **line)
{
	char *start;
	char *clean;

	clean = skip_whitespace(*line);
	if (*clean == '\0') {
		return NULL;
	}

	start = clean;

	// find end of token be it whitespace or newline
	while (*clean && !isspace((unsigned char) *clean)) {
		clean++;
	}

	if (*clean) {
		*clean = '\0';
		*line = clean + 1;
	} else {
		*line = clean;
	}

	return start;
}

static uint32_t parse_number(const char *str, uint8_t *ok)
{
	char *endptr;
	unsigned long val;

	if (!str || *str == '\0') {
		goto fail;
	}

	errno = 0;

	// base 0 auto detects hex/decimal
	val = strtoul(str, &endptr, 0);
	if (endptr == str || *endptr != '\0' || errno == ERANGE) {
		goto fail;
	}

	// overflow error
	if (val > UINT32_MAX) {
		goto fail;
	}

	*ok = 1;
	return (uint32_t) val;

fail:
	*ok = 0;
	return 0;
}

static HxptrResult_t parse_mcu_entry(mcu_profile_t *pf, char **line)
{
	char *name, *arch;

	// mcu <name> <arch>
	name = next_token(line);
	arch = next_token(line);

	if (!name || !arch) {
		return HX_ERR_PARSE_DEETS;
	}

	strncpy(pf->name, name, sizeof(pf->name) - 1);
	strncpy(pf->arch, arch, sizeof(pf->arch) - 1);

	return HX_OK;
}

static HxptrResult_t parse_memory_entry(mcu_profile_t *pf, char **line)
{
	uint8_t ok = 0;
	char *type, *start_str, *size_str;
	profile_memory_t mem = {0};

	// memory <type> <start> <size>
	type = next_token(line);
	start_str = next_token(line);
	size_str = next_token(line);

	if (!type || !start_str || !size_str) {
		return HX_ERR_PARSE_MEMORY;
	}

	// store the type
	strncpy(mem.type, type, sizeof(mem.type) - 1);

	// parse the addresses
	mem.start = parse_number(start_str, &ok);
	if (ok == 0) {
		return HX_ERR_PARSE_BAD_VALUE;
	}

	mem.size = parse_number(size_str, &ok);
	if (ok == 0) {
		return HX_ERR_PARSE_BAD_VALUE;
	}

	// add to the profile
	if (pf->mem_count >= MAX_MEMORY_BLOCKS) {
		return HX_ERR_OVERFLOW;
	}

	memcpy(&pf->mem_blocks[pf->mem_count++], &mem, sizeof(mem));

	return HX_OK;
}

static HxptrResult_t parse_vendor_entry(mcu_profile_t *pf, char **line)
{
	char *vendor;

	vendor = next_token(line);

	if (!vendor) {
		return HX_ERR_PARSE;
	}

	strncpy(pf->vendor, vendor, sizeof(pf->vendor) - 1);

	return HX_OK;
}

static HxptrResult_t parse_region_entry(mcu_profile_t *pf, char **line)
{
	char *name, *start_str, *end_str, *protected;
	profile_region_t reg = {0};
	uint8_t ok = 0;

	// region <name> <start> <end> [protected]
	name = next_token(line);
	start_str = next_token(line);
	end_str = next_token(line);
	protected = next_token(line);

	if (!name || !start_str || !end_str) {
		return HX_ERR_PARSE_REGION;
	}

	strncpy(reg.name, name, sizeof(reg.name) - 1);
	reg.start = parse_number(start_str, &ok);
	if (ok == 0) {
		return HX_ERR_PARSE_BAD_VALUE;
	}
	reg.end = parse_number(end_str, &ok);
	if (ok == 0) {
		return HX_ERR_PARSE_BAD_VALUE;
	}

	if (!protected) {
		reg.protected = 0;
	} else if (strcmp(protected, "protected") == 0) {
		reg.protected = 1;
	} else {
		return HX_ERR_PARSE;
	}

	// add to the profile
	if (pf->region_count >= MAX_REGIONS) {
		return HX_ERR_OVERFLOW;
	}

	memcpy(&pf->regions[pf->region_count++], &reg, sizeof(reg));

	return HX_OK;
}

mcu_profile_t *
mcu_profile_load(const char *path, HxptrResult_t *res, int *out_num)
{
	mcu_profile_t *prof;
	FILE *fp;
	char *directive, *unexpected;
	char line[256];
	int line_num = 0;

	fp = fopen(path, "r");
	if (!fp) {
		*res = HX_ERR_BAD_PROFILE;
		*out_num = 1;
		return NULL;
	}

	prof = calloc(1, sizeof(mcu_profile_t));
	if (!prof) {
		goto cleanup_fp;
	}

	prof->mem_blocks = malloc(sizeof(profile_memory_t) * MAX_MEMORY_BLOCKS);
	if (!prof->mem_blocks) {
		goto cleanup_prof;
	}

	prof->regions = malloc(sizeof(profile_region_t) * MAX_REGIONS);
	if (!prof->regions) {
		goto cleanup_mem_blocks;
	}

	while (fgets(line, sizeof(line), fp)) {
		line_num++;

		// skip comments and blank lines
		char *p = skip_whitespace(line);
		if (*p == '#' || *p == '\0' || *p == '\n') {
			continue;
		}

		// get the directive (should always be first token)
		directive = next_token(&p);
		if (!directive) {
			continue;
		}

		// dispatch to parser
		if (strcmp(directive, "mcu") == 0) {
			*res = parse_mcu_entry(prof, &p);
		} else if (strcmp(directive, "vendor") == 0) {
			*res = parse_vendor_entry(prof, &p);
		} else if (strcmp(directive, "memory") == 0) {
			*res = parse_memory_entry(prof, &p);
		} else if (strcmp(directive, "region") == 0) {
			*res = parse_region_entry(prof, &p);
		} else {
			*out_num = line_num;
			*res = HX_ERR_UNKN_DIRECTIVE;
			goto cleanup_reg_blocks;
		}

		if (*res != HX_OK) {
			*out_num = line_num;
			goto cleanup_reg_blocks;
		}

		// check for extra tokens that shouldn't be there
		unexpected = next_token(&p);
		if (unexpected && unexpected[0] != '#') {
			*res = HX_ERR_BAD_CONFIG;
			goto cleanup_reg_blocks;
		}
	}

	// validate required fields
	if (prof->name[0] == '\0' || prof->mem_count == 0) {
		fprintf(stderr, "profile missing required fields\n");
		goto cleanup_reg_blocks;
	}

	fclose(fp);

	return prof;

cleanup_reg_blocks:
	free(prof->regions);
cleanup_mem_blocks:
	free(prof->mem_blocks);
cleanup_prof:
	free(prof);
cleanup_fp:
	fclose(fp);

	return NULL;
}


// incomplete
void mcu_profile_free(mcu_profile_t *prof)
{
	if (!prof) {
		return;
	}

	free(prof->regions);
	free(prof->mem_blocks);
	free(prof);
}
