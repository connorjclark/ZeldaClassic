#ifndef ZQ_DIALOG_TEST_REPLAYS_DLG_H_
#define ZQ_DIALOG_TEST_REPLAYS_DLG_H_

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

// Checks text that will be saved in a replay's metadata.
bool validate_replay_meta_text(const std::string& text, std::string& error);

// The folder the current quest's test mode replays are saved in.
std::filesystem::path get_test_replay_dir();

// Lists the replays made of the current quest in test mode, and launches them in zplayer.
void call_test_replays_dialog();

// Writes a zip of test replays together with the quest they play, to share (see
// write_replay_bundle). Returns what went wrong, if anything.
std::optional<std::string> export_test_replay_bundle(const std::vector<std::filesystem::path>& replays,
	const std::filesystem::path& qst_path, const std::filesystem::path& zip_path);

#endif
