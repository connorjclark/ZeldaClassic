#include "base/hooks.h"
#include <vector>

#define HOOK_IMPL(name, type) \
static std::vector<void (*) (type)> cbs_##name;\
void hooks_register_##name(void (*cb) (type)) {\
	cbs_##name.push_back(cb);\
}\
void hooks_execute_##name(type input) {\
	for (auto cb : cbs_##name)\
		cb(input);\
}

HOOK_IMPL(dialog_runner_start, GUI::DialogRunner*)
HOOK_IMPL(dialog_runner_stop, GUI::DialogRunner*)
