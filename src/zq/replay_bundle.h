#ifndef ZQ_REPLAY_BUNDLE_H_
#define ZQ_REPLAY_BUNDLE_H_

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

// A replay to put in a bundle, and what to say about it in the bundle's README.
struct ReplayBundleEntry
{
	std::filesystem::path replay;
	std::string description;
};

// Writes a zip of replays together with the quest they play, so that someone else can reproduce
// what happened in them (like an Assert that failed). It has a folder named for the zip, which
// holds the quest, each replay with the files zplayer saved next to it (its last result, the
// screenshots of a failure, the pictures of its frames), and a README saying how to play them.
//
// Each replay is changed to look for the quest next to it, rather than where it was on this
// computer. Returns what went wrong, if anything.
std::optional<std::string> write_replay_bundle(const std::filesystem::path& zip_path,
	const std::filesystem::path& qst_path, const std::vector<ReplayBundleEntry>& replays);

#endif
