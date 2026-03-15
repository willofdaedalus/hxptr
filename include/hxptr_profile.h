#ifndef HXPTR_PROFILE_H
#define HXPTR_PROFILE_H

#include "hxptr.h"

// mcu_profile_t profile_load(const char *path);
mcu_profile_t *mcu_profile_load(const char *path, hxptr_error_t *err);
void mcu_profile_free(mcu_profile_t *prof);

#endif
