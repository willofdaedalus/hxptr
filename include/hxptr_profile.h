#ifndef HXPTR_PROFILE_H
#define HXPTR_PROFILE_H

#include "hxptr.h"

// mcu_profile_t profile_load(const char *path);
mcu_profile_t *profile_load(const char *path, HxptrResult_t *res);
void mcu_profile_free(mcu_profile_t *prof);

#endif
