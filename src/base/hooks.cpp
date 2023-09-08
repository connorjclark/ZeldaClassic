#include "base/hooks.h"
#include <vector>

static std::vector<void (*) (GUI::DialogRunner*)> dialog_runner_start_cbs;

void hooks_dialog_runner_start_register(void (*cb) (GUI::DialogRunner*))
{
	dialog_runner_start_cbs.push_back(cb);
}

void hooks_on_dialog_runner_start_execute(GUI::DialogRunner* runner)
{
	for (auto cb : dialog_runner_start_cbs)
		cb(runner);
}
