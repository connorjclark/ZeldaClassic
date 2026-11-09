#ifndef ZQ_DIALOG_TEST_REPLAYS_DLG_H_
#define ZQ_DIALOG_TEST_REPLAYS_DLG_H_

#include <filesystem>
#include <string>

#ifndef __EMSCRIPTEN__
// The folder the current quest's test mode replays are saved in.
std::filesystem::path get_test_replay_dir();

// Lists the replays made of the current quest in test mode, and launches them in zplayer.
void call_test_replays_dialog();
#endif

#endif
