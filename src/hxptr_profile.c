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

static int32_t parse_number(const char *str)
{
	char *endptr;
	unsigned long val;

	if (!str || *str == '\0') {
		return -1;
	}

	errno = 0;

	val = strtoul(str, &endptr, 0);
	if (endptr == str) {
		return -1;
	}

	if (*endptr != '\0') {
		return -1;
	}

	// overflow or other conversion error
	if (errno == ERANGE || val > INT32_MAX) {
		return -1;
	}

	return (int32_t) val;
}

static HxptrResult_t parse_mcu_entry(mcu_profile_t *pf, char **line)
{
	char *name, *arch;

	// mcu <name> <arch>
	name = next_token(line);
	arch = next_token(line);

	if (!name || !arch) {
		return HX_ERR_PARSE;
	}

	strncpy(pf->name, name, sizeof(pf->name) - 1);
	strncpy(pf->arch, arch, sizeof(pf->arch) - 1);

	return HX_OK;
}

static MemoryType_t str_mem_type(const char *str)
{
	if (strncmp("flash", str, 5) == 0) {
		return MEM_FLASH;
	} else if (strncmp("ram", str, 3) == 0) {
		return MEM_RAM;
	} else if (strncmp("rom", str, 3) == 0) {
		return MEM_ROM;
	} else if (strncmp("eeprom", str, 6) == 0) {
		return MEM_EEPROM;
	} else {
		return MEM_UNKNOWN;
	}
}

static HxptrResult_t parse_memory_entry(mcu_profile_t *pf, char **line)
{
	int32_t val;
	char *type, *start_str, *size_str;
	profile_memory_t mem = {0};

	// memory <type> <start> <size>
	type = next_token(line);
	start_str = next_token(line);
	size_str = next_token(line);

	if (!type || !start_str || !size_str) {
		return HX_ERR_PARSE;
	}

	// store the type
	mem.type = str_mem_type(type);
	if (mem.type == MEM_UNKNOWN) {
		return HX_ERR_PARSE;
	}

	// parse the addresses
	val = parse_number(start_str);
	if (val < 0) {
		return HX_ERR_PARSE;
	}
	mem.start = (uint32_t) val;

	val = parse_number(size_str);
	if (val < 0) {
		return HX_ERR_PARSE;
	}
	mem.size = (uint32_t) val;

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

	// region <name> <start> <end> [protected]
	name = next_token(line);
	start_str = next_token(line);
	end_str = next_token(line);
	protected = next_token(line);

	if (!name || !start_str || !end_str) {
		return HX_ERR_PARSE;
	}

	strncpy(reg.name, name, sizeof(reg.name) - 1);
	reg.start = parse_number(start_str);
	reg.size = parse_number(end_str);
	if (reg.start < 0 || reg.size < 0) {
		return HX_ERR_PARSE;
	}

	if (!protected) {
		reg.protected = 0;
	} else if (strncmp(protected, "protected", 9) == 0) {
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

mcu_profile_t *profile_load(const char *path, HxptrResult_t *res)
{
	mcu_profile_t *prof;
	FILE *fp;
	char *directive, *unexpected;
	char line[64];
	int line_num = 0;

	fp = fopen(path, "r");
	if (!fp) {
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
		if (strncmp(directive, "mcu", 3) == 0) {
			*res = parse_mcu_entry(prof, &p);
		} else if (strncmp(directive, "vendor", 6) == 0) {
			*res = parse_vendor_entry(prof, &p);
		} else if (strncmp(directive, "memory", 6) == 0) {
			*res = parse_memory_entry(prof, &p);
		} else if (strncmp(directive, "region", 6) == 0) {
			*res = parse_region_entry(prof, &p);
		} else {
			fprintf(stderr,
				"Line %d: Unknown directive '%s'\n",
				line_num,
				directive);
			continue;
		}

		if (*res != HX_OK) {
			fprintf(stderr,
				"Line %d: Parse error in '%s' directive\n",
				line_num,
				directive);
			goto cleanup_mem_blocks;
		}

		// check for extra tokens that shouldn't be there
		unexpected = next_token(&p);
		if (unexpected) {
			fprintf(stderr,
				"Line %d: Unexpected tokens after directives\n",
				line_num);
			*res = HX_ERR_BAD_CONFIG;
			goto cleanup_mem_blocks;
		}
	}

	// validate required fields
	if (prof->name[0] == '\0' || prof->mem_count == 0) {
		fprintf(stderr, "Profile missing required fields\n");
		goto cleanup_mem_blocks;
	}

	fclose(fp);

	return prof;

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
