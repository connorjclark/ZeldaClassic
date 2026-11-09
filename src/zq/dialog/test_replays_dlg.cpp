#include "test_replays_dlg.h"
#include "test_quest_dlg.h"
#include "dialog/alertfunc.h"
#include "dialog/externs.h"
#include "dialog/info.h"
#include "base/process_management.h"
#include "base/util.h"
#include "base/win_encoding.h"
#include "gui/builder.h"
#include "gui/list.h"
#include "zq/zq_class.h"
#include <fmt/format.h>
#include <algorithm>
#include <charconv>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <map>
#include <optional>

int32_t onSave();
extern char *filepath;
extern bool saved;

#ifndef __EMSCRIPTEN__
// The last run of replays launched from the Test Replays dialog. This lives outside the dialog,
// so closing and reopening it doesn't lose track of a run.
struct ReplayRun
{
	// Kept apart from the test session so that running a replay doesn't end it, which would
	// lose a recording in progress.
	process_killer killer;
	// The replays it was launched to play.
	std::vector<std::filesystem::path> paths;
	// What the result file of each held when the run started, to tell when zplayer has written
	// a new one.
	std::map<std::filesystem::path, std::string> old_results;
	// Whether it was stopped from the dialog, rather than ending by itself or crashing.
	bool stopped = false;
	// Whether it was an Assert or Update, rather than just playing them back. Those run without
	// opening the player's window.
	bool checks = false;
};
static ReplayRun replay_run;

static bool is_replay_run_going()
{
	return !replay_run.paths.empty() && replay_run.killer.is_alive();
}

// Whether the last run ended by crashing (or being killed), rather than exiting.
static bool did_replay_run_crash()
{
	return !replay_run.paths.empty() && !replay_run.stopped && !replay_run.killer.is_alive() && !replay_run.killer.exit_code();
}

static bool is_in_replay_run(const std::filesystem::path& path)
{
	return std::find(replay_run.paths.begin(), replay_run.paths.end(), path) != replay_run.paths.end();
}


struct TestReplay
{
	std::filesystem::path path;
	std::map<std::string, std::string> meta;
	// The result of the last Assert or Update of the replay, if it's had one.
	std::map<std::string, std::string> result;
};
static std::vector<TestReplay> test_replays;
static int32_t test_replay_val = 0;

static std::filesystem::path get_test_replay_root()
{
	return std::filesystem::path(zc_get_config("zquest", "replay_file_dir", "replays/")) / "test";
}

// zplayer writes a .result.txt file next to a replay every time it runs it. Playing a replay back
// would replace the result of its last Assert or Update that way, so those results go here.
static std::filesystem::path get_play_result_path(const std::filesystem::path& replay)
{
	std::error_code ec;
	return std::filesystem::absolute(get_test_replay_root(), ec) / ".played" / (replay.filename().string() + ".result.txt");
}

// Where zplayer writes the result of a replay in the last run.
static std::filesystem::path get_replay_run_result_path(const std::filesystem::path& replay)
{
	if (replay_run.checks)
		return replay.string() + ".result.txt";
	return get_play_result_path(replay);
}

// Whether zplayer has written a result for the replay since the last run started.
static bool has_replay_run_result(const std::filesystem::path& replay)
{
	auto it = replay_run.old_results.find(replay);
	return it != replay_run.old_results.end() && util::read_text_file(get_replay_run_result_path(replay)) != it->second;
}

// Each quest's test replays go in their own folder, named for the quest file. Quests with
// the same file name share one.
static std::filesystem::path get_test_replay_dir(const std::string& qst_path)
{
	std::string name = std::filesystem::path(qst_path).stem().string();
	for (char& c : name)
	{
		if (!(isalnum((unsigned char)c) || c == '-' || c == '_'))
			c = '_';
	}
	return get_test_replay_root() / name;
}

std::filesystem::path get_test_replay_dir()
{
	return get_test_replay_dir(filepath);
}

// Whether a line of a .zplay file is part of the metadata at its top (`M key value`). The
// metadata ends at the first replay step. Metadata that changes during the replay has a
// frame number before its key, and isn't part of it.
static bool is_replay_header_line(const std::string& line, std::string* key = nullptr, std::string* value = nullptr)
{
	if (!line.starts_with("M "))
		return false;

	auto sep = line.find(' ', 2);
	std::string k = line.substr(2, sep == std::string::npos ? std::string::npos : sep - 2);
	if (k.empty() || isdigit((unsigned char)k[0]))
		return false;

	if (key)
		*key = k;
	if (value)
		*value = sep == std::string::npos ? "" : line.substr(sep + 1);
	return true;
}

static std::string get_or_empty(const std::map<std::string, std::string>& map, const char* key)
{
	auto it = map.find(key);
	return it == map.end() ? "" : it->second;
}

static std::map<std::string, std::string> read_replay_meta(const std::filesystem::path& path)
{
	std::map<std::string, std::string> meta;
	std::ifstream file(path, std::ios::binary);
	std::string line, key, value;
	while (std::getline(file, line))
	{
		if (!line.empty() && line.back() == '\r')
			line.pop_back();
		if (!is_replay_header_line(line, &key, &value))
			break;
		meta[key] = value;
	}
	return meta;
}

// Sets the name shown for a replay in the Test Replays dialog, which is kept in the
// replay's metadata. Not the "name" key - that's the hero's name, which the game uses.
static bool set_replay_label(const std::filesystem::path& path, const std::string& label)
{
	std::ifstream in(path, std::ios::binary);
	if (!in)
		return false;

	// The metadata can be in any order, so the new label just goes first.
	std::string out = label.empty() ? "" : fmt::format("M label {}\n", label);
	std::string line, key;
	bool in_header = true;
	while (std::getline(in, line))
	{
		in_header = in_header && is_replay_header_line(line, &key);
		if (!in_header || key != "label")
			out += line + '\n';
	}
	in.close();

	auto tmp_path = path;
	tmp_path += ".tmp";
	{
		std::ofstream file(tmp_path, std::ios::binary);
		file << out;
		if (!file)
			return false;
	}
	std::error_code ec;
	std::filesystem::rename(tmp_path, path, ec);
	return !ec;
}

// A replay and the files zplayer writes next to it (like its .result.txt).
static std::vector<std::filesystem::path> get_test_replay_files(const std::filesystem::path& replay)
{
	std::error_code ec;
	std::string name = replay.filename().string();
	std::vector<std::filesystem::path> files;
	for (const auto& entry : std::filesystem::directory_iterator(replay.parent_path(), ec))
	{
		std::string entry_name = entry.path().filename().string();
		if (entry.is_regular_file() && (entry_name == name || entry_name.starts_with(name + ".")))
			files.push_back(entry.path());
	}
	return files;
}

// Test replays used to all be saved directly in replays/test/. Move those into the folder
// for their quest, along with the files zplayer writes next to them (like the .result.txt).
static void migrate_old_test_replays()
{
	std::error_code ec;
	std::vector<std::filesystem::path> replays;
	for (const auto& entry : std::filesystem::directory_iterator(get_test_replay_root(), ec))
	{
		if (entry.is_regular_file() && entry.path().extension() == ".zplay")
			replays.push_back(entry.path());
	}

	for (const auto& replay : replays)
	{
		auto meta = read_replay_meta(replay);
		std::string qst = meta["qst"];
		if (meta["test_mode"] != "true" || qst.empty())
			continue;

		// Like zplayer does when it loads the replay.
		util::legacy_path_to_utf8(qst);
		auto dir = get_test_replay_dir(qst);
		std::filesystem::create_directories(dir, ec);
		if (ec)
			continue;

		// Replays are named for the second they were made, so one of the same name in the
		// quest's folder is almost certainly a copy of this one. Leave it be.
		if (std::filesystem::exists(dir / replay.filename()))
			continue;
		// Found before moving any, since moving them changes the folder being listed.
		for (const auto& file : get_test_replay_files(replay))
			std::filesystem::rename(file, dir / file.filename(), ec);
	}
}

// Reads the `key: value` lines of a replay's .result.txt file, or a copy of one.
static std::map<std::string, std::string> read_replay_result(const std::filesystem::path& result_path)
{
	std::map<std::string, std::string> result;
	std::ifstream file(result_path, std::ios::binary);
	std::string line;
	while (std::getline(file, line))
	{
		if (!line.empty() && line.back() == '\r')
			line.pop_back();
		auto sep = line.find(": ");
		if (sep != std::string::npos)
			result[line.substr(0, sep)] = line.substr(sep + 2);
	}
	return result;
}

template <typename T>
static T parse_int(std::string_view str)
{
	T value = 0;
	std::from_chars(str.data(), str.data() + str.size(), value);
	return value;
}

// How far through a replay zplayer is, as a percentage, from the .result.txt file it rewrites
// about once a second as it goes.
static std::optional<int> get_replay_progress(const std::map<std::string, std::string>& result)
{
	int total = parse_int<int>(get_or_empty(result, "replay_log_frames"));
	if (total <= 0)
		return std::nullopt;
	return std::clamp(parse_int<int>(get_or_empty(result, "frame")) * 100LL / total, 0LL, 100LL);
}

// Describes the result of an Assert or Update, from a .result.txt file.
static std::optional<std::string> get_replay_outcome(const std::filesystem::path& replay_path,
	const std::map<std::string, std::string>& result)
{
	auto get = [&](const char* key) { return get_or_empty(result, key); };

	// Waiting its turn in the run going on, or never reached by the last run, in which case its
	// old result is out of date.
	if (is_in_replay_run(replay_path) && !has_replay_run_result(replay_path))
	{
		if (is_replay_run_going())
			return "queued";
		if (replay_run.checks)
			return "not checked";
	}

	// Record mode means this is the recording itself.
	std::string mode = get("mode");
	if (mode != "assert" && mode != "update")
		return std::nullopt;

	// zplayer writes -1 when the assert failed for a reason other than a mismatched frame.
	std::string failing_frame = get("failing_frame");
	if (!failing_frame.empty() && failing_frame != "-1")
		return fmt::format("{} failed (frame {})", mode, failing_frame);

	if (get("stopped") != "true")
	{
		// A run that just plays replays back doesn't write these results.
		bool in_run = replay_run.checks && is_in_replay_run(replay_path);
		if (in_run && did_replay_run_crash())
			return "crashed";
		if (!in_run || !is_replay_run_going())
			return fmt::format("{} stopped early", mode);
		auto progress = get_replay_progress(result);
		return progress ? fmt::format("{} running {}%", mode, *progress) : fmt::format("{} running", mode);
	}

	// Quitting partway through stops the replay without a verdict.
	if (get("aborted") == "true")
		return fmt::format("{} stopped early", mode);
	if (get("success") != "true")
		return fmt::format("{} failed", mode);

	if (mode == "update")
		return get("changed") == "true" ? "updated: changes saved" : "updated: no changes";
	return "passed";
}

static void load_test_replays()
{
	test_replays.clear();

	std::error_code ec;
	for (const auto& entry : std::filesystem::directory_iterator(get_test_replay_dir(), ec))
	{
		if (!entry.is_regular_file() || entry.path().extension() != ".zplay")
			continue;

		// Absolute, so the path means the same to zplayer wherever it runs from.
		std::error_code abs_ec;
		auto path = std::filesystem::absolute(entry.path(), abs_ec);
		if (abs_ec)
			path = entry.path();

		TestReplay& replay = test_replays.emplace_back();
		replay.path = path;
		replay.meta = read_replay_meta(path);
		replay.result = read_replay_result(path.string() + ".result.txt");
	}

	// Newest first. Replays are named for the time they were made, so this sorts by that.
	std::sort(test_replays.begin(), test_replays.end(), [](const TestReplay& a, const TestReplay& b) {
		return a.path.filename().string() > b.path.filename().string();
	});
}

// The result of a replay in the last run, if zplayer has finished with it. These are read from
// the run's own paths rather than the listed replays, which may be another quest's by now.
static std::optional<std::map<std::string, std::string>> read_replay_run_result(const std::filesystem::path& path)
{
	if (!has_replay_run_result(path))
		return std::nullopt;
	auto result = read_replay_result(get_replay_run_result_path(path));
	if (get_or_empty(result, "stopped") != "true")
		return std::nullopt;
	return result;
}

// A finished run normally leaves a new result for each replay it played. If zplayer crashed,
// or couldn't load a replay or its quest, some won't have one. (Ones just being played back
// also won't if the player was closed early, which isn't a problem.) Returns what went wrong
// with the last run, if anything did.
static std::string get_replay_run_error()
{
	// Stopped from the dialog, so there's nothing wrong with it not finishing.
	if (replay_run.paths.empty() || replay_run.stopped || is_replay_run_going())
		return "";

	std::string error;
	size_t missing = std::count_if(replay_run.paths.begin(), replay_run.paths.end(),
		[](const auto& path) { return !has_replay_run_result(path); });
	bool crashed = did_replay_run_crash();
	size_t count = replay_run.paths.size();
	if (crashed || (missing && replay_run.checks))
	{
		if (crashed)
			error = count == 1 ? "The player crashed while running the replay. " :
				"The player crashed while running the replays. ";
		if (missing)
			error += fmt::format("{} of {} weren't checked (marked [not checked]). ", missing, count);
		error += "Details are in allegro.log in the ZQuest Classic folder.";
	}

	// The player refuses to update a replay when the quest plays differently, rather than just
	// looks different, since that can't be saved into the old replay.
	size_t failed_updates = std::count_if(replay_run.paths.begin(), replay_run.paths.end(), [](const auto& path) {
		auto result = read_replay_run_result(path);
		return result && (*result)["mode"] == "update" && (*result)["success"] != "true" &&
			(*result)["aborted"] != "true";
	});
	if (failed_updates)
	{
		if (!error.empty())
			error += " ";
		error += fmt::format("Update failed for {} of {}: the quest now plays differently, not just"
			" looks different. Record a new session to replace it.", failed_updates, count);
	}
	return error;
}

static std::string get_replay_label(const TestReplay& replay)
{
	auto get = [&](const char* key) { return get_or_empty(replay.meta, key); };

	// The result goes first, so a long row doesn't cut it off.
	std::string label;
	if (auto outcome = get_replay_outcome(replay.path, replay.result))
		label = fmt::format("[{}]  ", *outcome);
	if (!get("label").empty())
		label += get("label") + "  ";
	// Replays are named test_<unix time>.
	std::string stem = replay.path.stem().string();
	int64_t time = stem.starts_with("test_") ? parse_int<int64_t>(stem.substr(5)) : 0;
	if (time > 0)
	{
		std::time_t t = time;
		char buf[32];
		std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M", std::localtime(&t));
		label += buf;
	}
	else
		label += stem;

	int seconds = parse_int<int>(get("frames")) / 60;
	label += fmt::format("  {}:{:02}", seconds / 60, seconds % 60);

	label += fmt::format("  DMap {} Scr {:02X}",
		parse_int<int>(get("starting_dmap")), parse_int<int>(get("starting_scr")));

	std::string init_data = get("init_data");
	if (!init_data.empty())
	{
		// Replays made before the name was saved can only be matched by their contents.
		std::string name = get("init_data_name");
		if (name.empty())
			name = find_test_init_data_name(init_data);
		if (name.empty())
			name = "custom init data";
		label += fmt::format("  ({})", name);
	}

	return label;
}


namespace {

// Lists the replays made of the current quest in test mode, and launches them in zplayer.
class TestReplaysDialog: public GUI::Dialog<TestReplaysDialog>
{
public:
	enum class message { REFR_INFO, PLAY, ASSERT, UPDATE, RENAME, REMOVE, COPY_PATH, OPEN_FOLDER, CANCEL_RUN, CLOSE };

	std::shared_ptr<GUI::Widget> view() override
	{
		using namespace GUI::Builder;
		using namespace GUI::Props;

		load_test_replays();
		update_replay_list();
		if (test_replay_val >= test_replays.size())
			test_replay_val = 0;

		std::shared_ptr<GUI::Widget> content;
		if (test_replays.empty())
		{
			content = Label(text = "No replays of this quest yet."
				"\nCheck \"Record\" in the Test Quest dialog to make one.");
		}
		else
		{
			content = list_widget = List(
				data = replay_list,
				width = 42_em, height = 16_em, fitParent = true,
				multiSelect = true,
				focused = true,
				selectedValue = test_replay_val,
				onSelectFunc = [&](int32_t val)
				{
					test_replay_val = val;
				},
				onDClick = message::PLAY
			);
		}

		bool no_replays = test_replays.empty();
		auto window = Window(
			title = "Test Replays",
			info = "Replays recorded of this quest in test mode, newest first. Check \"Record\""
				" in the Test Quest dialog to make one. They're kept by the quest's file name, so"
				" renaming the quest file leaves its replays behind."
				"\n\nShift+click or Ctrl+click (Cmd+click on macOS) to select more than one"
				" replay, or Ctrl+A (Cmd+A) to select them all. Play, Assert and Update play all the selected replays, one"
				" after another."
				"\n\nPlay shows a replay in the player. Assert also checks that the quest still plays"
				" the same as when it was recorded. Assert and Update run in the background, and"
				" the result of the last one is shown at the start of each row."
				"\n\nUpdate accepts changes to how the quest looks, such as new tiles or palettes,"
				" by saving them into the replay. If a change alters where the hero goes or"
				" when screens change, Update fails, and you'll need to record a new session."
				"\n\nRename names the highlighted replay. Delete deletes the selected replays,"
				" and Copy Path copies their paths. Open Folder shows where this quest's"
				" replays are kept.",
			onClose = message::CLOSE,
			use_vsync = true,
			onTick = [&]()
			{
				// Show progress while a run goes on, and the results once it's done.
				if (shown_running && (++ticks % 30 == 0 || !is_replay_run_going()))
					refresh_in_place();
				return ONTICK_CONTINUE;
			},
			Column(
				content,
				status_label = Label(width = 42_em, maxLines = 4),
				Row(
					spacing = 1_em,
					replay_button = Button(
						text = "Play",
						tooltip = "Play the selected replays back",
						onClick = message::PLAY),
					assert_button = Button(
						text = "Assert",
						tooltip = "Play the selected replays, and check the quest still plays the same",
						onClick = message::ASSERT),
					update_button = Button(
						text = "Update",
						tooltip = "Save how the quest looks now into the selected replays",
						onClick = message::UPDATE),
					stop_button = Button(
						text = "Stop",
						tooltip = "Stop the replays that are running",
						onClick = message::CANCEL_RUN)
				),
				Row(
					spacing = 1_em,
					rename_button = Button(
						text = "Rename",
						onClick = message::RENAME),
					delete_button = Button(
						text = "Delete",
						onClick = message::REMOVE),
					Button(
						text = "Copy Path",
						disabled = no_replays,
						onClick = message::COPY_PATH),
					Button(
						text = "Open Folder",
						onClick = message::OPEN_FOLDER)
				),
				Button(
					text = "Close",
					minwidth = 90_px,
					onClick = message::CLOSE,
					focused = no_replays)
			)
		);
		run_buttons = {replay_button, assert_button, update_button, rename_button, delete_button};
		update_run_status();
		return window;
	}

	bool handleMessage(const GUI::DialogMessage<message>& msg)
	{
		switch (msg.message)
		{
			case message::PLAY:
				return launch("-replay");

			case message::ASSERT:
				return launch("-assert");

			case message::UPDATE:
			{
				std::string text = "This saves how the quest looks now into the selected replays,"
					"\nwhich replaces what a later Assert checks against."
					"\n\nIt only works for changes to how the quest looks. If it now plays"
					"\ndifferently, the Update fails and the replay is left as it was.";
				text += "\n\nDo you wish to continue?";
				if (!alert_confirm("Update Replays", text))
					return false;
				return launch("-update");
			}

			case message::RENAME:
			{
				if (test_replay_val >= test_replays.size())
					return false;
				if (get_selected().size() > 1)
				{
					InfoDialog("Rename Replay", "Select just one replay to rename.").show();
					return false;
				}

				auto& replay = test_replays[test_replay_val];
				auto validate = [](const std::string& text, std::string& error)
				{
					// zplayer reads "«" as the start of an annotation, which isn't allowed in
					// the metadata.
					if (text.find("\xC2\xAB") != std::string::npos)
					{
						error = "Names can't contain \"\xC2\xAB\".";
						return false;
					}
					for (char c : text)
					{
						if ((unsigned char)c < 0x20 || c == 0x7F)
						{
							error = "Names can't contain control characters.";
							return false;
						}
					}
					return true;
				};
				if (auto label = call_get_text("Rename Replay", "Name:", replay.meta["label"], validate))
				{
					util::trimstr(*label);
					if (!set_replay_label(replay.path, *label))
						InfoDialog("Rename Replay", fmt::format("Failed to write {}", replay.path.string())).show();
					refresh_dlg();
				}
				return false;
			}

			case message::REMOVE:
			{
				auto selected = get_selected();
				if (selected.empty())
					return false;

				std::string what = selected.size() == 1 ? "this replay" : fmt::format("these {} replays", selected.size());
				if (!alert_confirm("Delete Replays", fmt::format("Delete {}? This can't be undone.", what)))
					return false;

				for (int32_t i : selected)
				{
					auto files = get_test_replay_files(test_replays[i].path);
					files.push_back(get_play_result_path(test_replays[i].path));
					for (const auto& file : files)
					{
						std::error_code ec;
						std::filesystem::remove(file, ec);
					}
				}
				refresh_dlg();
				return false;
			}

			case message::COPY_PATH:
			{
				std::string paths;
				for (int32_t i : get_selected())
				{
					if (!paths.empty())
						paths += '\n';
					paths += test_replays[i].path.string();
				}
				set_al_clipboard(paths);
				return false;
			}

			case message::OPEN_FOLDER:
			{
				std::error_code ec;
				auto dir = get_test_replay_dir();
				std::filesystem::create_directories(dir, ec);
				std::string path = std::filesystem::absolute(dir, ec).make_preferred().string();
#if defined(_WIN32)
				launch_process("explorer", {path});
#elif defined(__APPLE__)
				launch_process("open", {path});
#else
				launch_process("xdg-open", {path});
#endif
				return false;
			}

			case message::CANCEL_RUN:
				replay_run.killer.kill();
				replay_run.stopped = true;
				refresh_in_place();
				return false;

			case message::CLOSE:
				return true;
		}
		return false;
	}

private:
	GUI::ListData replay_list;
	std::shared_ptr<GUI::List> list_widget;
	std::shared_ptr<GUI::Button> replay_button, assert_button, update_button, rename_button, delete_button;
	// Disabled during a run: starting another would end the one going on, and renaming or
	// deleting would change a replay it may be reading.
	std::vector<std::shared_ptr<GUI::Button>> run_buttons;
	std::shared_ptr<GUI::Label> status_label;
	std::shared_ptr<GUI::Button> stop_button;
	// Whether the dialog shows a run going on, and the ticks since it started showing it.
	bool shown_running = false;
	int ticks = 0;
	// Whether it shows what went wrong with the last run.
	bool shown_error = false;

	void update_replay_list()
	{
		replay_list = GUI::ListData();
		for (int i = 0; i < test_replays.size(); i++)
			replay_list.add(get_replay_label(test_replays[i]), i);
	}

	// Updates the results and the run's status without rebuilding the dialog, which would lose
	// the selection.
	void refresh_in_place()
	{
		for (auto& replay : test_replays)
			replay.result = read_replay_result(replay.path.string() + ".result.txt");
		update_replay_list();
		if (list_widget)
			list_widget->setListData(replay_list);
		bool had_error = shown_error;
		update_run_status();
		// An error can take a few lines, which the dialog has to be laid out again to fit.
		if (!had_error && shown_error)
			refresh_dlg();
		pendDraw();
	}

	void update_run_status()
	{
		bool running = is_replay_run_going();
		std::string status = get_replay_run_error();
		shown_error = !status.empty();
		if (status.empty() && !test_replays.empty())
			status = test_replays.size() == 1 ? "1 replay" : fmt::format("{} replays", test_replays.size());
		if (running)
		{
			// The one being played is the one with a result that isn't final yet.
			size_t done = 0;
			std::optional<int> progress;
			for (const auto& path : replay_run.paths)
			{
				if (read_replay_run_result(path))
					done++;
				else if (has_replay_run_result(path))
					progress = get_replay_progress(read_replay_result(get_replay_run_result_path(path)));
			}
			size_t count = replay_run.paths.size();
			status = count == 1 ? "Running 1 replay" :
				fmt::format("Running {} of {} replays", std::min(done + 1, count), count);
			if (progress)
				status += fmt::format(" ({}%)", *progress);
			status += replay_run.checks ? "..." : " in the player's window.";
		}
		status_label->setText(status);
		stop_button->setDisabled(!running);
		for (auto& button : run_buttons)
			button->setDisabled(test_replays.empty() || running);
		shown_running = running;
		ticks = 0;
	}

	std::vector<int32_t> get_selected() const
	{
		std::vector<int32_t> selected;
		if (list_widget)
		{
			for (int32_t val : list_widget->getSelectedValues())
			{
				if (val >= 0 && val < test_replays.size())
					selected.push_back(val);
			}
		}
		return selected;
	}

	bool launch(const char* mode_switch)
	{
		// Assert and Update run in the background, and show their results in the list.
		bool checks = strcmp(mode_switch, "-replay");

		auto selected = get_selected();
		if (selected.empty() || is_replay_run_going())
			return false;

		if (!saved && zc_get_config("zquest", "test_mode_auto_save", true))
			onSave();

		std::vector<std::string> args;
		if (checks)
			args.push_back("-headless");
		else
		{
			auto dir = get_play_result_path("").parent_path();
			std::error_code ec;
			std::filesystem::create_directories(dir, ec);
			args.push_back("-replay-output-dir");
			args.push_back(dir.string());
		}
		if (selected.size() == 1)
		{
			args.push_back(mode_switch);
			args.push_back(test_replays[selected[0]].path.string());
			// Otherwise the player starts the replay over once it ends.
			args.push_back("-replay-exit-when-done");
		}
		else
		{
			// Play them one after another, in the order they're listed. zplayer exits once
			// they're all done.
			// zplayer reads the list when it starts, so this one file is reused.
			std::error_code ec;
			auto list_path = std::filesystem::absolute(get_test_replay_root(), ec) / "zc_test_replays.txt";
			std::filesystem::create_directories(list_path.parent_path(), ec);
			std::ofstream list_file(list_path, std::ios::binary);
			for (int32_t i : selected)
				list_file << test_replays[i].path.string() << '\n';
			list_file.close();
			if (!list_file)
			{
				InfoDialog("Test Replays", fmt::format("Failed to write {}", list_path.string())).show();
				return false;
			}

			args.push_back("-replay-batch");
			args.push_back(list_path.string());
			// Assert is the default for a batch.
			if (strcmp(mode_switch, "-assert"))
				args.push_back(mode_switch);
		}

		replay_run.paths.clear();
		replay_run.old_results.clear();
		replay_run.stopped = false;
		replay_run.checks = checks;
		for (int32_t i : selected)
		{
			auto& path = test_replays[i].path;
			replay_run.paths.push_back(path);
			replay_run.old_results[path] = util::read_text_file(get_replay_run_result_path(path));
		}
		replay_run.killer.kill();
		replay_run.killer = launch_process(locate_zapp_file(ZPLAYER_FILE), args);

		refresh_in_place();
		return false;
	}
};
}

void call_test_replays_dialog()
{
	migrate_old_test_replays();
	TestReplaysDialog().show();
}
#endif
