#ifndef ZC_SCRIPTING_GENERICDATA_H_
#define ZC_SCRIPTING_GENERICDATA_H_

#include "zc/ffscript.h"
#include <cstdint>

user_genscript* checkGenericScr(int32_t ref);

int32_t genericdata_get_register(int32_t reg);
void genericdata_set_register(int32_t reg, int32_t value);

#endif
