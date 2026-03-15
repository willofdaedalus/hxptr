#include "hxptr_profile.h"
#include "hxptr.h"
#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *skip_whitespace(char *p)
{
	while (*p && isspace((unsigned char) *p))
		p++;
	return p;
}

static char *next_token(char **line)
{
	char *start = skip_whitespace(*line);

	if (*start == '\0') {
		return NULL;
	}

	char *end = start;
	while (*end && !isspace((unsigned char) *end)) {
		end++;
	}

	if (*end) {
		*end = '\0';
		*line = end + 1;
	} else {
		*line = end;
	}

	return start;
}

static HxptrResult_t parse_number(const char *str, uint32_t *out)
{
	char *endptr;

	if (!str || *str == '\0') {
		return HX_ERR_PARSE_BAD_VALUE;
	}

	errno = 0;
	unsigned long val = strtoul(str, &endptr, 0);

	if (endptr == str || *endptr != '\0' || errno == ERANGE ||
	    val > UINT32_MAX) {
		return HX_ERR_PARSE_BAD_VALUE;
	}

	*out = (uint32_t) val;
	return HX_OK;
}

static HxptrResult_t parse_mcu_entry(mcu_profile_t *pf, char **line)
{
	char *name = next_token(line);
	char *arch = next_token(line);

	if (!name || !arch) {
		return HX_ERR_PARSE_DEETS;
	}

	strncpy(pf->name, name, sizeof(pf->name) - 1);
	strncpy(pf->arch, arch, sizeof(pf->arch) - 1);

	return HX_OK;
}

static HxptrResult_t parse_memory_entry(mcu_profile_t *pf, char **line)
{
	char *type = next_token(line);
	char *start_str = next_token(line);
	char *size_str = next_token(line);

	if (!type || !start_str || !size_str) {
		return HX_ERR_PARSE_MEMORY;
	}

	if (pf->mem_count >= MAX_MEMORY_BLOCKS) {
		return HX_ERR_OVERFLOW;
	}

	profile_memory_t mem = {0};
	HxptrResult_t res;

	strncpy(mem.type, type, sizeof(mem.type) - 1);

	res = parse_number(start_str, &mem.start);
	if (res != HX_OK) {
		return res;
	}

	res = parse_number(size_str, &mem.size);
	if (res != HX_OK) {
		return res;
	}

	memcpy(&pf->mem_blocks[pf->mem_count++], &mem, sizeof(mem));
	return HX_OK;
}

static HxptrResult_t parse_vendor_entry(mcu_profile_t *pf, char **line)
{
	char *vendor = next_token(line);

	if (!vendor) {
		return HX_ERR_PARSE;
	}

	strncpy(pf->vendor, vendor, sizeof(pf->vendor) - 1);
	return HX_OK;
}

static HxptrResult_t parse_region_entry(mcu_profile_t *pf, char **line)
{
	char *name = next_token(line);
	char *start_str = next_token(line);
	char *end_str = next_token(line);
	char *protected = next_token(line);

	if (!name || !start_str || !end_str) {
		return HX_ERR_PARSE_REGION;
	}

	if (pf->region_count >= MAX_REGIONS) {
		return HX_ERR_OVERFLOW;
	}

	profile_region_t reg = {0};
	HxptrResult_t res;

	strncpy(reg.name, name, sizeof(reg.name) - 1);

	res = parse_number(start_str, &reg.start);
	if (res != HX_OK) {
		return res;
	}

	res = parse_number(end_str, &reg.end);
	if (res != HX_OK) {
		return res;
	}

	if (!protected) {
		reg.protected = 0;
	} else if (strcmp(protected, "protected") == 0) {
		reg.protected = 1;
	} else {
		return HX_ERR_PARSE;
	}

	memcpy(&pf->regions[pf->region_count++], &reg, sizeof(reg));
	return HX_OK;
}

// dispatch table
typedef HxptrResult_t (*directive_fn)(mcu_profile_t *, char **);

typedef struct {
	const char *name;
	directive_fn fn;
} directive_entry_t;

static const directive_entry_t directives[] = {
	{"mcu", parse_mcu_entry},
	{"vendor", parse_vendor_entry},
	{"memory", parse_memory_entry},
	{"region", parse_region_entry},
};

static const size_t directive_count =
	sizeof(directives) / sizeof(directives[0]);

mcu_profile_t *
mcu_profile_load(const char *path, HxptrResult_t *res, int *out_num)
{
	FILE *fp;
	char line[256];
	int line_num = 0;

	*res = HX_OK;
	*out_num = 0;

	fp = fopen(path, "r");
	if (!fp) {
		*res = HX_ERR_BAD_PROFILE;
		return NULL;
	}

	mcu_profile_t *prof = calloc(1, sizeof(mcu_profile_t));
	if (!prof) {
		*res = HX_ERR_BAD_ALLOC;
		fclose(fp);
		return NULL;
	}

	prof->mem_blocks = malloc(sizeof(profile_memory_t) * MAX_MEMORY_BLOCKS);
	prof->regions = malloc(sizeof(profile_region_t) * MAX_REGIONS);

	if (!prof->mem_blocks || !prof->regions) {
		*res = HX_ERR_BAD_ALLOC;
		goto fail;
	}

	while (fgets(line, sizeof(line), fp)) {
		line_num++;

		char *p = skip_whitespace(line);
		if (*p == '#' || *p == '\0' || *p == '\n') {
			continue;
		}

		char *directive = next_token(&p);
		if (!directive) {
			continue;
		}

		directive_fn fn = NULL;
		for (size_t i = 0; i < directive_count; i++) {
			if (strcmp(directive, directives[i].name) == 0) {
				fn = directives[i].fn;
				break;
			}
		}

		if (!fn) {
			*res = HX_ERR_UNKN_DIRECTIVE;
			*out_num = line_num;
			goto fail;
		}

		*res = fn(prof, &p);
		if (*res != HX_OK) {
			*out_num = line_num;
			goto fail;
		}

		char *unexpected = next_token(&p);
		if (unexpected && unexpected[0] != '#') {
			*res = HX_ERR_BAD_CONFIG;
			*out_num = line_num;
			goto fail;
		}
	}

	if (prof->name[0] == '\0' || prof->mem_count == 0) {
		fprintf(stderr, "profile missing required fields\n");
		*res = HX_ERR_BAD_PROFILE;
		goto fail;
	}

	fclose(fp);
	return prof;

fail:
	free(prof->mem_blocks);
	free(prof->regions);
	free(prof);
	fclose(fp);
	return NULL;
}

void mcu_profile_free(mcu_profile_t *prof)
{
	if (!prof) {
		return;
	}

	free(prof->regions);
	free(prof->mem_blocks);
	free(prof);
}
