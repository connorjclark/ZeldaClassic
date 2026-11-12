#include "zq/replay_bundle.h"
#include "base/version.h"
#include <fmt/format.h>
#include <algorithm>
#include <fstream>
#include <iterator>
#include <sstream>
#include <miniz.h>

namespace fs = std::filesystem;

// Read as bytes, since some are pictures, and text mode would change the line endings of the
// rest on Windows.
static std::optional<std::string> read_file(const fs::path& path)
{
	std::ifstream file(path, std::ios::binary);
	if (!file)
		return std::nullopt;
	return std::string(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
}

// A replay and the files zplayer saves next to it, like its .result.txt.
static std::vector<fs::path> get_replay_files(const fs::path& replay)
{
	std::error_code ec;
	std::string name = replay.filename().string();
	std::vector<fs::path> files;
	for (const auto& entry : fs::directory_iterator(replay.parent_path(), ec))
	{
		std::string entry_name = entry.path().filename().string();
		// Not ones being written while a replay is recorded or updated.
		if (!entry.is_regular_file() || entry.path().extension() == ".tmp")
			continue;
		if (entry_name == name || entry_name.starts_with(name + "."))
			files.push_back(entry.path());
	}
	std::sort(files.begin(), files.end());
	return files;
}

// Changes the metadata at the top of a replay (`M <key> <value>`) to look for the quest next to
// it. The metadata ends at the first step; metadata that changes during the replay has a frame
// number before its key.
static std::string relocate_replay(const std::string& text, const std::string& qst_name)
{
	std::istringstream in(text);
	std::string out, line;
	bool in_header = true;
	while (std::getline(in, line))
	{
		in_header = in_header && line.starts_with("M ") && line.size() > 2 && !isdigit((unsigned char)line[2]);
		if (in_header && (line == "M qst" || line.starts_with("M qst ")))
			line = "M qst " + qst_name;
		out += line + '\n';
	}
	return out;
}

// The result file of a replay says where it was on this computer, which means nothing elsewhere
// (and has the user's name in it).
static std::string relocate_result(const std::string& text, const std::string& replay_name)
{
	std::istringstream in(text);
	std::string out, line;
	while (std::getline(in, line))
	{
		if (line.starts_with("replay: "))
			line = "replay: " + replay_name;
		out += line + '\n';
	}
	return out;
}

static std::string make_readme(const std::string& qst_name, const std::vector<ReplayBundleEntry>& replays)
{
	std::string text = fmt::format(
		"Test replays of {0}\n"
		"\n"
		"Made with ZQuest Classic {1}. Play them with the same version to see the same results.\n"
		"\n"
		"To play one, unzip this folder, open ZQuest Classic's player, choose Replay > Load replay"
		" (or Replay > Load replay (assert) to check that the quest still plays the same as when it"
		" was recorded), and pick its .zplay file. Each replay finds {0} next to it. Or from a"
		" command line:\n"
		"\n"
		"    zplayer -assert \"{2}\"\n"
		"\n"
		"Next to each replay:\n"
		"  .result.txt   the result of its last Assert or Update\n"
		"  .png          screenshots of the frames where it went wrong\n"
		"  .frames       pictures of each frame as it looked when it was recorded or last updated\n"
		"  .roundtrip    what happened instead when it went wrong, to compare with the .zplay\n"
		"\n"
		"Replays:\n",
		qst_name, getVersionString(), replays.empty() ? "" : replays[0].replay.filename().string());
	for (const auto& entry : replays)
	{
		text += fmt::format("\n{}\n", entry.replay.filename().string());
		std::istringstream in(entry.description);
		std::string line;
		while (std::getline(in, line))
			text += line.empty() ? "\n" : "    " + line + "\n";
	}
	return text;
}

std::optional<std::string> write_replay_bundle(const fs::path& zip_path, const fs::path& qst_path,
	const std::vector<ReplayBundleEntry>& replays)
{
	mz_zip_archive zip{};
	if (!mz_zip_writer_init_heap(&zip, 0, 0))
		return "Failed to start the zip";

	std::string folder = zip_path.stem().string() + "/";
	std::string qst_name = qst_path.filename().string();
	std::optional<std::string> error;
	auto add = [&](const std::string& name, const std::string& data, mz_uint level) {
		if (error)
			return;
		if (!mz_zip_writer_add_mem(&zip, (folder + name).c_str(), data.data(), data.size(), level))
			error = fmt::format("Failed to add {} to the zip: {}", name, mz_zip_get_error_string(zip.m_last_error));
	};

	if (auto qst = read_file(qst_path))
		add(qst_name, *qst, MZ_DEFAULT_LEVEL);
	else
		error = fmt::format("Failed to read {}", qst_path.string());

	for (const auto& entry : replays)
	{
		std::string replay_name = entry.replay.filename().string();
		auto files = get_replay_files(entry.replay);
		if (files.empty())
			error = fmt::format("Failed to read {}", entry.replay.string());
		for (const auto& file : files)
		{
			auto data = read_file(file);
			if (!data)
			{
				error = fmt::format("Failed to read {}", file.string());
				break;
			}

			std::string name = file.filename().string();
			std::string ext = file.extension().string();
			if (ext == ".zplay" || ext == ".roundtrip")
				*data = relocate_replay(*data, qst_name);
			else if (name == replay_name + ".result.txt")
				*data = relocate_result(*data, replay_name);
			// These are compressed already.
			bool compressed = ext == ".png" || ext == ".frames";
			add(name, *data, compressed ? MZ_NO_COMPRESSION : MZ_DEFAULT_LEVEL);
		}
	}
	add("README.txt", make_readme(qst_name, replays), MZ_DEFAULT_LEVEL);

	void* buf = nullptr;
	size_t size = 0;
	if (!error && !mz_zip_writer_finalize_heap_archive(&zip, &buf, &size))
		error = fmt::format("Failed to finish the zip: {}", mz_zip_get_error_string(zip.m_last_error));
	if (!error)
	{
		std::ofstream out(zip_path, std::ios::binary);
		out.write((const char*)buf, size);
		out.close();
		if (!out)
			error = fmt::format("Failed to write {}", zip_path.string());
	}
	// The finished zip's memory is ours to free, rather than the writer's.
	if (buf)
		mz_free(buf);
	mz_zip_writer_end(&zip);
	return error;
}
