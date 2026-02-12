#include "hxptr_profile.h"
#include "hxptr.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static profile_region_t *
make_region(const char *name, uint32_t start, uint32_t end, uint8_t protected)
{
	profile_region_t *region;

	if (!name) {
		return NULL;
	}

	region = malloc(sizeof(profile_region_t));
	if (!region) {
		return NULL;
	}

	snprintf(region->name, strlen(name) - 1, "%s", name);
	// strncpy(region->name, name, strlen(name));
	region->start = start;
	region->end = end;
	region->protected = protected;

	return region;
}

static mcu_profile_t *atmega328p_profile(void)
{
	mcu_profile_t *pf;

	pf = calloc(1, sizeof(mcu_profile_t));
	if (!pf) {
		return NULL;
	}

	strncpy(pf->name, "ATmega328P", 11);
	strncpy(pf->architecture, "avr", 4);
	pf->flash_start = 0x0000;
	pf->flash_size = 32768;

	// pf->regions = malloc(sizeof(profile_region_t));
	// if (!pf->regions) {
	// 	free(pf);
	// 	return NULL;
	// }

	pf->regions = make_region("bootloader", 0x7000, 0x7fff, 1);
	pf->num_regions = 1;

	return pf;
}

mcu_profile_t *profile_load(void)
{
	return atmega328p_profile();
}

void mcu_profile_free(mcu_profile_t *prof)
{
	if (!prof) {
		return;
	}

	free(prof->regions);
	free(prof);
}
