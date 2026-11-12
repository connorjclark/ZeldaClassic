#include "test_replays_dlg.h"
#include "test_quest_dlg.h"
#include "dialog/alertfunc.h"
#include "dialog/externs.h"
#include "dialog/info.h"
#include "base/process_management.h"
#include "base/frame_stream.h"
#include "base/util.h"
#include "base/win_encoding.h"
#include "gui/builder.h"
#include "gui/common.h"
#include "gui/dialog_runner.h"
#include "gui/jwin.h"
#include "gui/list.h"
#include "zalleg/render.h"
#include "zq/zq_class.h"
#include "zq/zquest.h"
#include <allegro5/allegro.h>
#include <fmt/format.h>
#include <algorithm>
#include <charconv>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <map>
#include <optional>

#ifdef __EMSCRIPTEN__
#include "base/emscripten_utils.h"
#endif

int32_t onSave();
extern char *filepath;
extern bool saved;

// Checks text that will be saved in a replay's metadata.
bool validate_replay_meta_text(const std::string& text, std::string& error)
{
	// zplayer reads "«" as the start of an annotation, which isn't allowed in the metadata.
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
}

// The last run of replays launched from the Test Replays dialog. This lives outside the dialog,
// so closing and reopening it doesn't lose track of a run.
struct ReplayRun
{
	// Kept apart from the test session so that running a replay doesn't end it, which would
	// lose a recording in progress. On web the player runs in a browser tab or a hidden frame instead.
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
	// A replay played up to where it failed, which the player can record the rest of from
	// there, and what the replay held when the run started.
	std::filesystem::path take_over_path;
	std::string take_over_old_replay;
};
static ReplayRun replay_run;

static bool is_replay_run_going()
{
	if (replay_run.paths.empty())
		return false;
#ifdef __EMSCRIPTEN__
	return em_is_tracked_player_open();
#else
	return replay_run.killer.is_alive();
#endif
}

// Whether the last run ended by crashing (or being killed), rather than exiting.
static bool did_replay_run_crash()
{
#ifdef __EMSCRIPTEN__
	// There's no telling from another tab or frame.
	return false;
#else
	return !replay_run.paths.empty() && !replay_run.stopped && !replay_run.killer.is_alive() && !replay_run.killer.exit_code();
#endif
}

static void stop_replay_run_player()
{
#ifdef __EMSCRIPTEN__
	em_close_tracked_player();
#else
	replay_run.killer.kill();
#endif
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
#ifdef __EMSCRIPTEN__
	// Only /local is kept between sessions, and shared with the player's tab.
	return "local/replays/test";
#else
	return std::filesystem::path(zc_get_config("zquest", "replay_file_dir", "replays/")) / "test";
#endif
}

// On web the player runs in another tab or frame, with its own copy of the files. This loads the
// replays and results that it has saved since.
static void pull_test_replays()
{
#ifdef __EMSCRIPTEN__
	em_pull_persisted_dir(get_test_replay_root().string());
#endif
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

// Removes the result of a replay's last run, along with the screenshots zplayer saved of where
// it failed. zplayer doesn't remove those itself, and they'd be mistaken for ones of a new failure.
static void remove_replay_results(const std::filesystem::path& replay, bool include_result)
{
	std::vector<std::string> removed;
	for (const auto& file : get_test_replay_files(replay))
	{
		bool is_result = file.filename().string() == replay.filename().string() + ".result.txt";
		if (file.extension() != ".png" && file.extension() != ".roundtrip" && !(include_result && is_result))
			continue;
		std::error_code ec;
		std::filesystem::remove(file, ec);
		removed.push_back(file.string());
	}
#ifdef __EMSCRIPTEN__
	em_remove_persisted_files(removed);
#endif
}

// Once a replay played up to where it failed is done, its old results no longer apply if the
// player recorded the rest of it again.
static void forget_results_if_rerecorded()
{
	if (replay_run.take_over_path.empty() || is_replay_run_going())
		return;
	if (util::read_text_file(replay_run.take_over_path) != replay_run.take_over_old_replay)
		remove_replay_results(replay_run.take_over_path, true);
	replay_run.take_over_path.clear();
	replay_run.take_over_old_replay.clear();
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

// Describes a frame of a replay as a time, like "0:02 (frame 121)".
static std::string describe_replay_frame(int frame)
{
	int seconds = frame / 60;
	return fmt::format("{}:{:02} (frame {})", seconds / 60, seconds % 60, frame);
}

// A step of a replay, as zplayer writes it in a .zplay file: its type, the frame it happened
// on, and the rest.
struct ReplayStepText
{
	char type = 0;
	int frame = -1;
	std::string text;
};

static std::optional<ReplayStepText> parse_replay_step(const std::string& line)
{
	if (line.size() < 3 || line[1] != ' ')
		return std::nullopt;
	auto sep = line.find(' ', 2);
	ReplayStepText step;
	step.type = line[0];
	step.frame = parse_int<int>(line.substr(2, sep == std::string::npos ? std::string::npos : sep - 2));
	step.text = sep == std::string::npos ? "" : line.substr(sep + 1);
	return step;
}

// Whether a step is the hash of a frame, which the debug data of a test replay records
// whenever the screen changes (`C <frame> g <hash>`).
static bool is_gfx_step(const std::optional<ReplayStepText>& step)
{
	return step && step->type == 'C' && step->text.starts_with("g ");
}

// Describes a step of a replay in words, for the ones that test replays record most often.
static std::string describe_replay_step(const std::optional<ReplayStepText>& step)
{
	if (!step)
		return "the replay ended";
	if (step->type != 'C')
		return fmt::format("input or random numbers ({} {})", step->type, step->text);

	const std::string& text = step->text;
	if (text.starts_with("g "))
		return "the screen changed";
	if (text.starts_with("h "))
	{
		// The hero's position, in hex.
		int x = 0, y = 0;
		if (sscanf(text.c_str() + 2, "%x %x", &x, &y) == 2)
			return fmt::format("the hero moved to ({}, {})", x, y);
	}
	if (text.starts_with("scr="))
		return fmt::format("screen {:02X} loaded", parse_int<int>(text.substr(4)));
	if (text.starts_with("dmap="))
		return fmt::format("warped to DMap {}", text.substr(5));
	if (text.starts_with("sfx "))
		return fmt::format("sound \"{}\" played", text.substr(4));
	if (text.starts_with("getitem "))
		return fmt::format("got the item \"{}\"", text.substr(8));
	return text;
}

// Explains why a replay failed its last Assert, from the details zplayer writes in the
// .result.txt file. Returns nothing if it didn't fail one.
static std::optional<std::string> describe_replay_failure(const std::map<std::string, std::string>& result)
{
	auto get = [&](const char* key) { return get_or_empty(result, key); };
	if (get("mode") != "assert" || get("stopped") != "true" || get("aborted") == "true" || get("success") == "true")
		return std::nullopt;

	std::string text = "Assert failed";
	std::string frame = get("failing_frame");
	if (!frame.empty() && frame != "-1")
		text += " at " + describe_replay_frame(parse_int<int>(frame));
	if (!get("failing_dmap").empty())
		text += fmt::format(", on DMap {} Scr {:02X}", parse_int<int>(get("failing_dmap")), parse_int<int>(get("failing_screen")));
	text += ".\n\n";

	bool looks_different = false;
	std::string error = get("failing_error");
	if (error == "rng desync")
		text += "Random numbers came out differently than when it was recorded.";
	else if (!error.empty())
		text += fmt::format("The replay was stopped: {}.", error);
	else if (result.contains("failing_expected"))
	{
		auto expected = parse_replay_step(get("failing_expected"));
		auto got = parse_replay_step(get("failing_got"));
		looks_different = is_gfx_step(expected) && is_gfx_step(got);
		if (looks_different)
			text += "The screen looks different than when it was recorded, though the game played the same up to here.";
		else
		{
			// Only say when each happened if that's part of the difference.
			bool same_frame = expected && got && expected->frame == got->frame;
			auto describe = [&](const std::optional<ReplayStepText>& step) {
				std::string desc = describe_replay_step(step);
				if (step && !same_frame)
					desc += " at " + describe_replay_frame(step->frame);
				return desc;
			};
			text += fmt::format("The first difference:\n  When recorded, {}.\n  Now, {}.", describe(expected), describe(got));
		}
	}
	else
		text += "The quest played differently than when it was recorded.";

	if (looks_different)
		text += "\n\nIf that's an intended change, such as new tiles or palettes, Update accepts it."
			" (Update fails if the quest also plays differently later on.)";
	else
		text += "\n\nThe quest now plays differently, which Update can't accept. If that's an intended"
			" change, record a new session to replace this replay.";
	return text;
}

// The screenshot zplayer saved of the frame a replay failed its last Assert on, if there is one.
static std::optional<std::filesystem::path> get_replay_failure_image(const std::filesystem::path& replay_path,
	const std::map<std::string, std::string>& result)
{
	std::string frame = get_or_empty(result, "failing_frame");
	if (frame.empty() || frame == "-1")
		return std::nullopt;

	std::error_code ec;
	for (const char* suffix : {"-unexpected.png", ".png"})
	{
		std::filesystem::path path = fmt::format("{}.{}{}", replay_path.string(), frame, suffix);
		if (std::filesystem::exists(path, ec))
			return path;
	}
	return std::nullopt;
}

// Draws a picture of a frame into a bitmap, making a new one if it isn't there yet or is a
// different size. Returns the bitmap, or null if it couldn't be made.
static ALLEGRO_BITMAP* draw_picture(const IndexedFrame& picture, ALLEGRO_BITMAP* bmp = nullptr)
{
	if (bmp && (al_get_bitmap_width(bmp) != picture.width || al_get_bitmap_height(bmp) != picture.height))
	{
		al_destroy_bitmap(bmp);
		bmp = nullptr;
	}
	if (!bmp)
		bmp = al_create_bitmap(picture.width, picture.height);
	if (!bmp)
		return nullptr;
	ALLEGRO_LOCKED_REGION* lr = al_lock_bitmap(bmp, ALLEGRO_PIXEL_FORMAT_ABGR_8888_LE, ALLEGRO_LOCK_WRITEONLY);
	if (!lr)
		return bmp;
	for (int y = 0; y < picture.height; y++)
	{
		uint8_t* row = (uint8_t*)lr->data + y * lr->pitch;
		for (int x = 0; x < picture.width; x++)
		{
			const uint8_t* color = &picture.palette[picture.pixels[y * picture.width + x] * 3];
			row[x * 4] = color[0];
			row[x * 4 + 1] = color[1];
			row[x * 4 + 2] = color[2];
			row[x * 4 + 3] = 255;
		}
	}
	al_unlock_bitmap(bmp);
	return bmp;
}

static std::filesystem::path get_replay_frames_path(const std::filesystem::path& replay_path)
{
	return replay_path.string() + ".frames";
}

// The picture of how a frame of a replay looked when it was recorded, if one was saved with it.
static ALLEGRO_BITMAP* load_recorded_frame(const std::filesystem::path& replay_path, int frame)
{
	auto picture = read_frame_stream(get_replay_frames_path(replay_path), frame);
	return picture ? draw_picture(*picture) : nullptr;
}

// Copies a picture with the pixels that differ from another of the same size marked, and the rest
// darkened. Returns nothing if they aren't the same size.
static ALLEGRO_BITMAP* mark_differences(ALLEGRO_BITMAP* bmp, ALLEGRO_BITMAP* other, int* count)
{
	int w = al_get_bitmap_width(bmp), h = al_get_bitmap_height(bmp);
	if (w != al_get_bitmap_width(other) || h != al_get_bitmap_height(other))
		return nullptr;

	ALLEGRO_BITMAP* marked = al_clone_bitmap(bmp);
	if (!marked)
		return nullptr;
	ALLEGRO_LOCKED_REGION* lr = al_lock_bitmap(marked, ALLEGRO_PIXEL_FORMAT_ABGR_8888_LE, ALLEGRO_LOCK_READWRITE);
	ALLEGRO_LOCKED_REGION* other_lr = al_lock_bitmap(other, ALLEGRO_PIXEL_FORMAT_ABGR_8888_LE, ALLEGRO_LOCK_READONLY);
	if (!lr || !other_lr)
	{
		if (lr)
			al_unlock_bitmap(marked);
		if (other_lr)
			al_unlock_bitmap(other);
		al_destroy_bitmap(marked);
		return nullptr;
	}

	*count = 0;
	for (int y = 0; y < h; y++)
	{
		uint8_t* row = (uint8_t*)lr->data + y * lr->pitch;
		const uint8_t* other_row = (const uint8_t*)other_lr->data + y * other_lr->pitch;
		for (int x = 0; x < w; x++)
		{
			uint8_t* color = &row[x * 4];
			if (memcmp(color, &other_row[x * 4], 3))
			{
				color[0] = 255;
				color[1] = 0;
				color[2] = 255;
				++*count;
			}
			else
			{
				for (int i = 0; i < 3; i++)
					color[i] /= 3;
			}
		}
	}
	al_unlock_bitmap(other);
	al_unlock_bitmap(marked);
	return marked;
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
#ifndef __EMSCRIPTEN__
		error += "Details are in allegro.log in the ZQuest Classic folder.";
#endif
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

// Shows a picture of a frame of a replay. The editor's dialogs are drawn with its own palette, so
// this draws on a true-color layer above the dialog instead.
class ReplayFrameView: public GUI::Widget
{
public:
	// The bitmap is the caller's, and has to outlive the dialog. It's drawn `scale` times as large.
	explicit ReplayFrameView(ALLEGRO_BITMAP* bmp, int scale = 1)
		: ReplayFrameView(al_get_bitmap_width(bmp) * scale, al_get_bitmap_height(bmp) * scale)
	{
		this->bmp = bmp;
	}

	// A box of the given size, which pictures set later are fit into.
	ReplayFrameView(int width, int height)
	{
		setPreferredWidth(GUI::Size::pixels(width + 4));
		setPreferredHeight(GUI::Size::pixels(height + 4));
	}

	void setBitmap(ALLEGRO_BITMAP* new_bmp)
	{
		bmp = new_bmp;
		if (rti)
			rti->dirty = true;
		pendDraw();
	}

private:
	ALLEGRO_BITMAP* bmp = nullptr;
	RenderTreeItem* rti = nullptr;
	GUI::DialogRef alDialog;

	static int32_t proc(int32_t msg, DIALOG* d, int32_t)
	{
		auto* view = (ReplayFrameView*)d->dp3;
		if (!view)
			return D_O_K;

		switch (msg)
		{
			case MSG_START:
				if (!view->rti)
				{
					view->rti = add_dlg_layer();
					view->rti->render_cb = [view, d](RenderTreeItem*, bool) {
						if (!view->bmp || (d->flags & D_HIDDEN))
							return;
						// As large as fits, keeping its shape, in the middle.
						float w = al_get_bitmap_width(view->bmp), h = al_get_bitmap_height(view->bmp);
						float scale = std::min((d->w - 4) / w, (d->h - 4) / h);
						float x = d->x + 2 + (d->w - 4 - w * scale) / 2, y = d->y + 2 + (d->h - 4 - h * scale) / 2;
						al_draw_scaled_bitmap(view->bmp, 0, 0, w, h, x, y, w * scale, h * scale, 0);
					};
				}
				break;

			case MSG_END:
				if (view->rti)
				{
					remove_dlg_layer(view->rti);
					view->rti = nullptr;
				}
				break;

			case MSG_DRAW:
				rectfill(screen, d->x, d->y, d->x + d->w - 1, d->y + d->h - 1, jwin_pal[jcBOX]);
				jwin_draw_frame(screen, d->x, d->y, d->w, d->h, FR_DEEP);
				if (view->rti)
					view->rti->dirty = true;
				break;
		}
		return D_O_K;
	}

	void applyVisibility(bool visible) override
	{
		Widget::applyVisibility(visible);
		if (alDialog)
			alDialog.applyVisibility(visible);
	}

	void realize(GUI::DialogRunner& runner) override
	{
		Widget::realize(runner);
		alDialog = runner.push(shared_from_this(), DIALOG {
			GUI::newGUIProc<proc>,
			x, y, getWidth(), getHeight(),
			0, 0,
			0, // key
			getFlags(), // flags
			0, 0, // d1, d2
			nullptr, nullptr, this // dp, dp2, dp3
		});
	}
};

// The two pictures of a frame a replay failed an Assert on: how it looked when it was recorded,
// and how it looks now. Either can be missing. Each has a copy with the pixels that differ marked.
struct ReplayFramePictures
{
	static constexpr const char* names[2] = {"When recorded", "Now"};
	std::array<ALLEGRO_BITMAP*, 2> plain{};
	std::array<ALLEGRO_BITMAP*, 2> marked{};
	int difference_count = 0;

	ALLEGRO_BITMAP* get(int which, bool highlight) const
	{
		return highlight && marked[which] ? marked[which] : plain[which];
	}
};

// Shows one picture of a failed frame at a time, as large as fits in the editor's window, and
// switches between the two to compare them.
class ReplayFrameCompareDialog: public GUI::Dialog<ReplayFrameCompareDialog>
{
public:
	enum class message { SWITCH, CLOSE };

	ReplayFrameCompareDialog(const ReplayFramePictures& pictures, int shown, bool highlight)
		: pictures(pictures), shown(shown), highlight(highlight)
	{
	}

	// Which picture is shown, and whether the differences are highlighted, to carry back.
	int shown;
	bool highlight;

	std::shared_ptr<GUI::Widget> view() override
	{
		using namespace GUI::Builder;
		using namespace GUI::Props;

		// Scaled by a whole number, so the pixels stay square, leaving room for the title bar
		// and buttons.
		int w = 0, h = 0;
		for (ALLEGRO_BITMAP* bmp : pictures.plain)
		{
			if (!bmp)
				continue;
			w = std::max(w, al_get_bitmap_width(bmp));
			h = std::max(h, al_get_bitmap_height(bmp));
		}
		int scale = std::max(1, std::min((zq_screen_w - 40) / w, (zq_screen_h - 110) / h));

		bool both = pictures.plain[0] && pictures.plain[1];
		std::shared_ptr<GUI::Widget> highlight_checkbox = DummyWidget();
		if (pictures.marked[0] && pictures.difference_count)
		{
			highlight_checkbox = Checkbox(
				text = "&Highlight differences",
				checked = highlight,
				onToggleFunc = [&](bool state)
				{
					highlight = state;
					frame_view->setBitmap(pictures.get(shown, highlight));
				});
		}

		return Window(
			title = "Compare Frames",
			info = "Switch (or S) shows the other picture, to see what changed between them."
				" Highlight (or H) marks the pixels that differ.",
			onClose = message::CLOSE,
			Column(
				frame = Frame(
					title = pictures.names[shown],
					frame_view = std::make_shared<ReplayFrameView>(pictures.get(shown, highlight), scale)),
				Row(
					spacing = 1_em,
					Button(
						text = "&Switch",
						tooltip = "Show the other picture",
						disabled = !both,
						focused = both,
						onClick = message::SWITCH),
					highlight_checkbox,
					Button(
						text = "Close",
						minwidth = 90_px,
						focused = !both,
						onClick = message::CLOSE)
				)
			)
		);
	}

	bool handleMessage(const GUI::DialogMessage<message>& msg)
	{
		if (msg.message == message::CLOSE)
			return true;

		shown = 1 - shown;
		frame->setTitle(pictures.names[shown]);
		frame_view->setBitmap(pictures.get(shown, highlight));
		return false;
	}

private:
	const ReplayFramePictures& pictures;
	std::shared_ptr<GUI::Frame> frame;
	std::shared_ptr<ReplayFrameView> frame_view;
};

// Explains why a replay failed its last Assert, with pictures of the frame it failed on as it
// looks now and as it looked when the replay was recorded.
class ReplayFailureDialog: public GUI::Dialog<ReplayFailureDialog>
{
public:
	enum class message { REPLAY_TO_FAILURE, FULL_SCREEN, CLOSE };

	// Takes the bitmap of how the frame looked when recorded, if there is one.
	ReplayFailureDialog(std::string details, std::optional<std::filesystem::path> image,
		ALLEGRO_BITMAP* recorded, bool can_replay)
		: details(std::move(details)), recorded(recorded), can_replay(can_replay)
	{
		if (image)
			now = al_load_bitmap(image->string().c_str());
		if (now && recorded)
		{
			marked_now = mark_differences(now, recorded, &difference_count);
			if (marked_now)
				marked_recorded = mark_differences(recorded, now, &difference_count);
		}
		if (now && !recorded)
			this->details += "\n\nThere's no picture of how this looked when it was recorded. Replays"
				" save one of each frame when they're recorded or updated.";
	}

	~ReplayFailureDialog()
	{
		for (ALLEGRO_BITMAP* bmp : {recorded, now, marked_recorded, marked_now})
		{
			if (bmp)
				al_destroy_bitmap(bmp);
		}
	}

	// Whether Replay to Failure was clicked.
	bool replay_to_failure = false;

	std::shared_ptr<GUI::Widget> view() override
	{
		using namespace GUI::Builder;
		using namespace GUI::Props;

		// The pictures go side by side, with the details below them.
		auto pictures = Row(vAlign = 0.0);
		int pictures_width = 0;
		for (auto [bmp, view, name] : {std::tuple{recorded, &recorded_view, "When recorded"}, {now, &now_view, "Now"}})
		{
			if (!bmp)
				continue;
			*view = std::make_shared<ReplayFrameView>(bmp);
			pictures->add(Frame(title = name, *view));
			pictures_width += al_get_bitmap_width(bmp) + 16;
		}

		std::shared_ptr<GUI::Widget> highlight;
		if (marked_now && difference_count == 0)
			highlight = Label(text = "The pictures look the same.");
		else if (marked_now)
		{
			highlight = highlight_checkbox = Checkbox(
				text = difference_count == 1 ? "Highlight the 1 pixel that differs" :
					fmt::format("Highlight the {} pixels that differ", difference_count),
				onToggleFunc = [&](bool state)
				{
					show_highlight(state);
				});
		}
		else
			highlight = DummyWidget();

		return Window(
			title = "Assert Failed",
			info = "Replay to Failure plays the replay up to just before it went wrong, and pauses"
				" there. From there you can look around and play on, or record the rest of the"
				" replay again, which replaces it when you quit.",
			onClose = message::CLOSE,
			Column(
				pictures,
				highlight,
				Label(text = details, width = GUI::Size::pixels(std::max(pictures_width, 300)), maxLines = 16),
				Row(
					spacing = 1_em,
					Button(
						text = "Full Screen",
						tooltip = "Show the pictures as large as fits, one at a time, to switch between them",
						disabled = !recorded && !now,
						onClick = message::FULL_SCREEN),
					Button(
						text = "Replay to Failure",
						tooltip = "Play the replay up to just before it went wrong, and pause there,"
							" to play on or record the rest of it again",
						disabled = !can_replay,
						onClick = message::REPLAY_TO_FAILURE),
					Button(
						text = "Close",
						minwidth = 90_px,
						focused = true,
						onClick = message::CLOSE)
				)
			)
		);
	}

	bool handleMessage(const GUI::DialogMessage<message>& msg)
	{
		if (msg.message == message::FULL_SCREEN)
		{
			ReplayFramePictures pictures;
			pictures.plain = {recorded, now};
			pictures.marked = {marked_recorded, marked_now};
			pictures.difference_count = difference_count;
			bool highlight = highlight_checkbox && highlight_checkbox->getChecked();
			ReplayFrameCompareDialog dlg(pictures, now ? 1 : 0, highlight);
			dlg.show();
			if (highlight_checkbox && dlg.highlight != highlight)
			{
				highlight_checkbox->setChecked(dlg.highlight);
				show_highlight(dlg.highlight);
			}
			return false;
		}

		replay_to_failure = msg.message == message::REPLAY_TO_FAILURE;
		return true;
	}

private:
	std::string details;
	ALLEGRO_BITMAP* recorded;
	ALLEGRO_BITMAP* now = nullptr;
	// Copies of those with the pixels that differ marked.
	ALLEGRO_BITMAP* marked_recorded = nullptr;
	ALLEGRO_BITMAP* marked_now = nullptr;
	int difference_count = 0;
	std::shared_ptr<ReplayFrameView> recorded_view, now_view;
	std::shared_ptr<GUI::Checkbox> highlight_checkbox;
	bool can_replay;

	void show_highlight(bool highlight)
	{
		recorded_view->setBitmap(highlight ? marked_recorded : recorded);
		now_view->setBitmap(highlight ? marked_now : now);
	}
};

// Lists the replays made of the current quest in test mode, and launches them in zplayer.
class TestReplaysDialog: public GUI::Dialog<TestReplaysDialog>
{
public:
	enum class message { REFR_INFO, PLAY, ASSERT, UPDATE, DETAILS, RENAME, REMOVE, COPY_PATH, OPEN_FOLDER, CANCEL_RUN, PAUSE_PREVIEW, CLOSE };

	~TestReplaysDialog()
	{
		if (preview_bmp)
			al_destroy_bitmap(preview_bmp);
	}

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
				width = 30_em, height = 16_em, fitParent = true,
				multiSelect = true,
				focused = true,
				selectedValue = test_replay_val,
				onSelectFunc = [&](int32_t val)
				{
					test_replay_val = val;
					update_details_button();
					load_preview();
				},
				onDClick = message::PLAY
			);
		}

		// Plays the pictures of the highlighted replay's frames, if it has them.
		preview_view = std::make_shared<ReplayFrameView>(256, 232);
		preview_view->setBitmap(preview_bmp);
		if (!test_replays.empty())
		{
			content = Row(
				vAlign = 0.0,
				content,
				Column(
					vAlign = 0.0,
					preview_view,
					Row(
						preview_label = Label(width = GUI::Size::pixels(180), maxLines = 3),
						preview_button = Button(
							text = preview_paused ? "Play" : "Pause",
							disabled = !preview_reader.is_open(),
							onClick = message::PAUSE_PREVIEW)
					)
				)
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
				"\n\nNext to the list, the highlighted replay plays back as pictures of its frames,"
				" if it has them. Replays save them when they're recorded or updated."
				"\n\nIf an Assert fails, Details explains what went wrong, and shows the frame it"
				" went wrong on. From there, Replay to Failure plays the replay up to just before"
				" that, and pauses so you can look around and play on, or record the rest of the"
				" replay again."
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
				advance_preview();
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
						onClick = message::CANCEL_RUN),
					details_button = Button(
						text = "Details",
						tooltip = "Explain why the highlighted replay failed its last Assert",
						onClick = message::DETAILS)
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
						onClick = message::COPY_PATH)
#ifndef __EMSCRIPTEN__
					,
					Button(
						text = "Open Folder",
						onClick = message::OPEN_FOLDER)
#endif
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
		// The last replay previewed may be gone, or its pictures changed.
		preview_path.clear();
		load_preview();
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

			case message::DETAILS:
			{
				if (test_replay_val >= test_replays.size())
					return false;
				auto& replay = test_replays[test_replay_val];
				auto details = describe_replay_failure(replay.result);
				if (!details)
					return false;

				int frame = parse_int<int>(get_or_empty(replay.result, "failing_frame"));
				ReplayFailureDialog dlg(*details, get_replay_failure_image(replay.path, replay.result),
					frame >= 0 ? load_recorded_frame(replay.path, frame) : nullptr, frame > 0 && !is_replay_run_going());
				dlg.show();
				if (dlg.replay_to_failure)
					return launch("-update", {test_replay_val}, frame);
				return false;
			}

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
				if (auto label = call_get_text("Rename Replay", "Name:", replay.meta["label"], validate_replay_meta_text))
				{
					util::trimstr(*label);
					if (!set_replay_label(replay.path, *label))
						InfoDialog("Rename Replay", fmt::format("Failed to write {}", replay.path.string())).show();
#ifdef __EMSCRIPTEN__
					em_persist_files({replay.path.string()});
#endif
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

				std::vector<std::string> removed;
				for (int32_t i : selected)
				{
					auto files = get_test_replay_files(test_replays[i].path);
					files.push_back(get_play_result_path(test_replays[i].path));
					for (const auto& file : files)
					{
						std::error_code ec;
						std::filesystem::remove(file, ec);
						removed.push_back(file.string());
					}
				}
#ifdef __EMSCRIPTEN__
				em_remove_persisted_files(removed);
#endif
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
#ifndef __EMSCRIPTEN__
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
#endif
				return false;
			}

			case message::PAUSE_PREVIEW:
				preview_paused = !preview_paused;
				preview_button->setText(preview_paused ? "Play" : "Pause");
				return false;

			case message::CANCEL_RUN:
				stop_replay_run_player();
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
	std::shared_ptr<GUI::Button> replay_button, assert_button, update_button, rename_button, delete_button, details_button;
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

	// The pictures of the highlighted replay's frames, played back on a loop like an animation:
	// the frame it's up to, and that of the picture shown.
	FrameStreamReader preview_reader;
	std::filesystem::path preview_path;
	std::filesystem::file_time_type preview_time;
	ALLEGRO_BITMAP* preview_bmp = nullptr;
	std::shared_ptr<ReplayFrameView> preview_view;
	std::shared_ptr<GUI::Label> preview_label;
	std::shared_ptr<GUI::Button> preview_button;
	int preview_frame = 0;
	int preview_shown = -1;
	// How long the replay is, which the pictures may stop short of if it ends on a still screen.
	int preview_length = 0;
	bool preview_paused = false;

	// Starts playing the pictures of the highlighted replay, unless they're already playing.
	void load_preview()
	{
		if (!preview_view)
			return;

		std::filesystem::path path;
		std::filesystem::file_time_type time;
		if (test_replay_val < test_replays.size())
		{
			path = get_replay_frames_path(test_replays[test_replay_val].path);
			std::error_code ec;
			time = std::filesystem::last_write_time(path, ec);
		}
		if (path == preview_path && time == preview_time)
			return;

		preview_path = path;
		preview_time = time;
		preview_frame = 0;
		preview_shown = -1;
		preview_length = 0;
		bool has_pictures = !path.empty() && preview_reader.open(path);
		if (!has_pictures)
			preview_reader = FrameStreamReader();
		else
			preview_length = std::max(preview_reader.last_frame(), parse_int<int>(get_or_empty(test_replays[test_replay_val].meta, "frames")));
		preview_button->setDisabled(!has_pictures);
		preview_view->setBitmap(nullptr);
		if (has_pictures)
			show_preview_frame();
		else
			preview_label->setText("No pictures of this replay yet. Update saves them.");
	}

	void advance_preview()
	{
		if (!preview_reader.is_open() || preview_paused)
			return;
		if (++preview_frame > preview_length)
			preview_frame = 0;
		show_preview_frame();
	}

	void show_preview_frame()
	{
		// Only redrawn when the picture changes, and the time each second.
		const IndexedFrame* picture = preview_reader.at(preview_frame);
		if (picture && picture->frame != preview_shown)
		{
			preview_shown = picture->frame;
			preview_bmp = draw_picture(*picture, preview_bmp);
			preview_view->setBitmap(preview_bmp);
		}
		if (preview_frame % 60 == 0)
		{
			int seconds = preview_frame / 60, total = preview_length / 60;
			preview_label->setText(fmt::format("{}:{:02} / {}:{:02}", seconds / 60, seconds % 60, total / 60, total % 60));
		}
	}

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
		pull_test_replays();
		forget_results_if_rerecorded();
		for (auto& replay : test_replays)
			replay.result = read_replay_result(replay.path.string() + ".result.txt");
		update_replay_list();
		if (list_widget)
			list_widget->setListData(replay_list);
		// An Update may have saved new pictures.
		if (!is_replay_run_going())
			load_preview();
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
		update_details_button();
	}

	// Details is only for a replay that failed its last Assert.
	void update_details_button()
	{
		bool failed = test_replay_val < test_replays.size() &&
			!(is_in_replay_run(test_replays[test_replay_val].path) && is_replay_run_going()) &&
			describe_replay_failure(test_replays[test_replay_val].result);
		details_button->setDisabled(!failed);
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
		return launch(mode_switch, get_selected());
	}

	// With a frame, plays just the one replay in the player's window up to that frame, and then
	// gives control to the player. With Update, the player can record the rest of it from there.
	bool launch(const char* mode_switch, std::vector<int32_t> selected, std::optional<int> frame = std::nullopt)
	{
		// Assert and Update run in the background, and show their results in the list.
		bool checks = !frame && strcmp(mode_switch, "-replay");

		if (selected.empty() || is_replay_run_going())
			return false;

		if (!saved && zc_get_config("zquest", "test_mode_auto_save", true))
			onSave();

		PlayerArgs args;
		if (checks)
			args.push_back({"-headless"});
		else
		{
			auto dir = get_play_result_path("").parent_path();
			std::error_code ec;
			std::filesystem::create_directories(dir, ec);
#ifdef __EMSCRIPTEN__
			// The player's tab has its own copy of the files, which it loads when it starts.
			em_persist_files({dir.string()});
#endif
			args.push_back({"-replay-output-dir", dir.string()});
		}
		if (checks)
		{
			for (int32_t i : selected)
				remove_replay_results(test_replays[i].path, false);
		}
		if (frame)
		{
			args.push_back({"-frame", std::to_string(*frame)});
			if (!strcmp(mode_switch, "-update"))
				args.push_back({"-replay-take-over"});
		}
		// Updating a replay also saves pictures of its frames, to compare with if an Assert fails.
		if (!strcmp(mode_switch, "-update"))
			args.push_back({"-replay-save-frames"});
		if (selected.size() == 1)
		{
			args.push_back({mode_switch, test_replays[selected[0]].path.string()});
			// Otherwise the player starts the replay over once it ends.
			args.push_back({"-replay-exit-when-done"});
		}
		else
		{
			// Play them one after another, in the order they're listed. zplayer exits once
			// they're all done.
			// zplayer reads the list when it starts, so this one file is reused. On web it has
			// to be somewhere the player's tab can read.
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
#ifdef __EMSCRIPTEN__
			em_persist_files({list_path.string()});
#endif

			args.push_back({"-replay-batch", list_path.string()});
			// Assert is the default for a batch.
			if (strcmp(mode_switch, "-assert"))
				args.push_back({mode_switch});
		}

		replay_run.paths.clear();
		replay_run.old_results.clear();
		replay_run.stopped = false;
		replay_run.checks = checks;
		replay_run.take_over_path.clear();
		replay_run.take_over_old_replay.clear();
		if (frame && !strcmp(mode_switch, "-update"))
		{
			replay_run.take_over_path = test_replays[selected[0]].path;
			replay_run.take_over_old_replay = util::read_text_file(replay_run.take_over_path);
		}
		// The results as they are now, including what a player on web has saved since.
		pull_test_replays();
		for (int32_t i : selected)
		{
			auto& path = test_replays[i].path;
			replay_run.paths.push_back(path);
			replay_run.old_results[path] = util::read_text_file(get_replay_run_result_path(path));
		}
		replay_run.killer.kill();
		replay_run.killer = launch_player(args, true);

		refresh_in_place();
		return false;
	}
};
}

void call_test_replays_dialog()
{
#ifdef __EMSCRIPTEN__
	pull_test_replays();
#else
	migrate_old_test_replays();
#endif
	forget_results_if_rerecorded();
	TestReplaysDialog().show();
}
