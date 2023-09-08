// Allow for programs to provide callbacks into specific points of the base library.

#ifndef _HOOKS_H_
#define _HOOKS_H_

namespace GUI {
	class DialogRunner;
}

void hooks_dialog_runner_start_register(void (*cb) (GUI::DialogRunner*));
void hooks_on_dialog_runner_start_execute(GUI::DialogRunner* runner);

#endif
