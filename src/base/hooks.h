// Allow for programs to provide callbacks into specific points of the base library.

#ifndef _HOOKS_H_
#define _HOOKS_H_

namespace GUI {
	class DialogRunner;
}

#define HOOK(name, type) \
void hooks_register_##name(void (*cb) (type));\
void hooks_execute_##name(type input);

HOOK(dialog_runner_start, GUI::DialogRunner*)
HOOK(dialog_runner_stop, GUI::DialogRunner*)

#endif
