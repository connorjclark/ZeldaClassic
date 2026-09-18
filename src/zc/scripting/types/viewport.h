#ifndef ZC_SCRIPTING_VIEWPORT_H_
#define ZC_SCRIPTING_VIEWPORT_H_

#include <cstdint>
#include <optional>

#include "base/ints.h"

int32_t viewport_get_register(int32_t reg);
void viewport_set_register(int32_t reg, int32_t value);
std::optional<int32_t> viewport_run_command(word command);

#endif
