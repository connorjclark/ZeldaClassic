#ifndef ZC_SCRIPTING_STACK_H_
#define ZC_SCRIPTING_STACK_H_

#include <cstdint>

int32_t stack_get_register(int32_t reg);
void stack_set_register(int32_t reg, int32_t value);

#endif
