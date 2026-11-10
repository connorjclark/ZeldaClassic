#include "core/dmap.h"
#include "core/initdata.h"
#include "dialog/info.h"
#include "test_quest_dlg.h"
#include "test_replays_dlg.h"
#include "dialog/alertfunc.h"
#include "base/process_management.h"
#include "gui/builder.h"
#include "zc_list_data.h"
#include "init.h"
#include "zq/zq_class.h"
#include "base/util.h"
#include "dialog/externs.h"
#include "dialog/init_data.h"
#include "gui/list.h"
#include <fmt/format.h>
#include <algorithm>
#include <ctime>
#include <filesystem>
#include <optional>

#ifdef __EMSCRIPTEN__
#include "base/emscripten_utils.h"
#endif

int32_t onSave();
int32_t onSaveAs();
extern char *filepath;
extern bool saved, first_save;

static int32_t test_start_dmap = 0, test_start_screen = 0, test_ret_sqr = 0, test_init_data_val = 0;
static std::vector<std::string> test_init_data;
static std::vector<std::string> test_init_data_names;
static process_killer test_killer;

static const GUI::ListData retsqrlist
{
	{ "A", 0 },
	{ "B", 1 },
	{ "C", 2 },
	{ "D", 3 }
};

static GUI::ListData init_data_list;

std::string find_test_init_data_name(const std::string& init_data)
{
	auto it = std::find(test_init_data.begin(), test_init_data.end(), init_data);
	return it != test_init_data.end() ? test_init_data_names[it - test_init_data.begin()] : "";
}

process_killer launch_player(const PlayerArgs& args, [[maybe_unused]] bool track)
{
#ifdef __EMSCRIPTEN__
	// The web player takes its switches as URL params, without the dash in front.
	std::vector<std::pair<std::string, std::string>> params;
	for (auto& arg : args)
	{
		if (arg[0] == "-test")
		{
			// The only one with more than one value. Each has its own param.
			const char* keys[] = {"test", "dmap", "screen", "retsquare"};
			for (size_t i = 1; i < arg.size(); i++)
				params.push_back({keys[i - 1], arg[i]});
		}
		else
			params.push_back({arg[0].substr(1), arg.size() > 1 ? arg[1] : ""});
	}
	em_open_player(params, track);
	return {};
#else
	std::vector<std::string> flat_args;
	for (auto& arg : args)
		flat_args.insert(flat_args.end(), arg.begin(), arg.end());
	return launch_process(locate_zapp_file(ZPLAYER_FILE), flat_args);
#endif
}

int calculate_test_dmap();
void call_testqst_dialog()
{
	if(!first_save) //Require quest file is saved (i.e. not 'Untitled' fresh from File->New)
	{
		onSaveAs();
		if(!first_save)
			return;
	}
	test_start_dmap = calculate_test_dmap();
	test_start_screen = zc_min(0x7F, Map.getCurrScr());
	
	TestQstDialog().show();
}

TestQstDialog::TestQstDialog() : dmap_list(GUI::ZCListData::dmaps(true))
{}

std::shared_ptr<GUI::Widget> TestQstDialog::view()
{
	using namespace GUI::Builder;
	using namespace GUI::Props;

	init_data_list = GUI::ListData();
	init_data_list.add("<default>", 0);

	test_init_data.clear();
	test_init_data_names.clear();
	int i = 1;
	std::string qst_cfg_header = qst_cfg_header_from_path(filepath);
	while (strlen(zc_get_config(qst_cfg_header.c_str(), fmt::format("test_init_data_{}", i).c_str(), "")))
	{
		std::string init_data = zc_get_config(qst_cfg_header.c_str(), fmt::format("test_init_data_{}", i).c_str(), "");
		std::string name = zc_get_config(qst_cfg_header.c_str(), fmt::format("test_init_data_{}_name", i).c_str(), "");
		if (name.empty())
			name = fmt::format("Init Data {}", i);
		test_init_data.push_back(std::move(init_data));
		test_init_data_names.push_back(name);
		init_data_list.add(name, i);
		i += 1;
	}

	return Window(
		title = "Test Quest",
		onClose = message::CANCEL,
		Column(
			Rows<2>(
				Label(text = "Start DMap:"),
				DropDownList(data = dmap_list,
					fitParent = true,
					selectedValue = test_start_dmap,
					onSelectFunc = [&](int32_t val)
					{
						test_start_dmap = val;
					}
				),
				Label(text = "Start Screen:"),
				TextField(
					fitParent = true, minwidth = 4_em,
					type = GUI::TextField::type::SWAP_BYTE,
					bounds = {0x00, 0x7F}, val = test_start_screen,
					swap_type = 1,
					onValChangedFunc = [&](GUI::TextField::type,std::string_view,int32_t val)
					{
						test_start_screen = val;
					}),
				Label(text = "Return Square:"),
				DropDownList(
					maxwidth = 10_em,
					data = retsqrlist,
					selectedValue = test_ret_sqr,
					onSelectFunc = [&](int32_t val)
					{
						test_ret_sqr = val;
					}
				),
				Label(text = "Init Data:"),
				Row(
					hAlign = 0.0,
					DropDownList(
						maxwidth = 10_em,
						data = init_data_list,
						selectedValue = test_init_data_val,
						onSelectFunc = [&](int32_t val)
						{
							test_init_data_val = val;
						}
					),
					Button(
						text = "Edit...",
						padding = 0_px,
						forceFitH = true,
						onClick = message::MANAGE_INIT_DATA)
				)
			),
			Row(
				vAlign = 1.0,
				spacing = 2_em,
				Button(
					text = "Test",
					minwidth = 90_px,
					onClick = message::OK,
					focused = true),
				Button(
					text = "Replays",
					minwidth = 90_px,
					onClick = message::REPLAYS),
				Button(
					text = "Cancel",
					minwidth = 90_px,
					onClick = message::CANCEL)
			),
			Rows<2>(
				INFOBTN("Save before opening test mode."),
				Checkbox(text = "Auto-Save", fitParent = true,
					checked = zc_get_config("zquest", "test_mode_auto_save", true),
					onToggleFunc = [&](bool state)
					{
						zc_set_config("zquest", "test_mode_auto_save", state);
					}
				),

				INFOBTN("Restart player when qst is saved."),
				Checkbox(text = "Auto-Restart", fitParent = true,
					checked = zc_get_config("zquest", "test_mode_auto_restart", false),
					onToggleFunc = [&](bool state)
					{
						zc_set_config("zquest", "test_mode_auto_restart", state);
					}
				),

				INFOBTN("Record your test session so you can play it back later. After changing"
					" the quest, use Replays > Assert to check it still plays the same, so you'll"
					" know if a change broke something."),
				Checkbox(text = "Record", fitParent = true,
					checked = zc_get_config("zquest", "test_mode_record", false),
					onToggleFunc = [&](bool state)
					{
						zc_set_config("zquest", "test_mode_record", state);
					}
				)
			)
		)
	);
}

static void save_test_init_data(int i)
{
	std::string qst_cfg_header = qst_cfg_header_from_path(filepath);
	zc_set_config(qst_cfg_header.c_str(), fmt::format("test_init_data_{}", i + 1).c_str(), test_init_data[i].c_str());
	zc_set_config(qst_cfg_header.c_str(), fmt::format("test_init_data_{}_name", i + 1).c_str(), test_init_data_names[i].c_str());
}

// Test init data names go in the metadata of replays recorded with them, and on zplayer's
// command line, where a leading "-" would be read as a switch.
static bool validate_test_init_data_name(const std::string& text, std::string& error)
{
	auto first = text.find_first_not_of("\t\n\v\f\r ");
	if (first != std::string::npos && text[first] == '-')
	{
		error = "Names can't start with \"-\".";
		return false;
	}
	return validate_replay_meta_text(text, error);
}

namespace {

// Lists the quest's test init data, which Test Quest can start the game with, and creates,
// edits and renames them. Picking a different one here also picks it for Test Quest.
class TestInitDataDialog: public GUI::Dialog<TestInitDataDialog>
{
public:
	enum class message { REFR_INFO, CREATE, EDIT, RENAME, REMOVE, CLOSE };

	std::shared_ptr<GUI::Widget> view() override
	{
		using namespace GUI::Builder;
		using namespace GUI::Props;

		list_data = GUI::ListData();
		for (int i = 0; i < test_init_data_names.size(); i++)
			list_data.add(test_init_data_names[i], i + 1);

		std::shared_ptr<GUI::Widget> content;
		if (test_init_data.empty())
			content = Label(text = "No test init data yet.");
		else
		{
			if (selected < 1 || selected > test_init_data.size())
				selected = test_init_data_val >= 1 && test_init_data_val <= test_init_data.size() ? test_init_data_val : 1;
			content = List(
				data = list_data,
				width = 20_em, height = 8_em, fitParent = true,
				focused = true,
				selectedValue = selected,
				onSelectFunc = [&](int32_t val)
				{
					selected = test_init_data_val = val;
				},
				onDClick = message::EDIT
			);
		}

		bool none = test_init_data.empty();
		return Window(
			title = "Test Init Data",
			info = "Test init data changes what the game starts with in test mode, like the"
				" items, counters and level items, without changing the quest's own init data."
				" Picking a different one here also picks it in the Test Quest dialog's Init Data"
				" list.",
			onClose = message::CLOSE,
			Column(
				content,
				Row(
					spacing = 1_em,
					Button(
						text = "Create",
						onClick = message::CREATE),
					Button(
						text = "Edit",
						disabled = none,
						onClick = message::EDIT),
					Button(
						text = "Rename",
						disabled = none,
						onClick = message::RENAME),
					Button(
						text = "Delete",
						disabled = none,
						onClick = message::REMOVE)
				),
				Button(
					text = "Close",
					minwidth = 90_px,
					onClick = message::CLOSE,
					focused = none)
			)
		);
	}

	bool handleMessage(const GUI::DialogMessage<message>& msg)
	{
		int i = selected - 1;
		bool has_selection = i >= 0 && i < test_init_data.size();
		switch (msg.message)
		{
			case message::CREATE:
			{
				zinitdata zinit_test = zinit;
				bool old_saved = saved;
				// Only allow configuration for the same stuff visible in the cheat menu in the player,
				// plus the viewport follow settings, which (unlike the cheat) Test Quest can apply.
				bool kept = call_init_dlg(zinit_test, true, true);
				saved = old_saved;
				if (!kept)
					return false;

				std::string delta = serialize_init_data_delta(&zinit, &zinit_test);

				int index = test_init_data.size();
				// Cancelling the name keeps the default, rather than losing the init data just set up.
				std::string name = fmt::format("Init Data {}", index + 1);
				if (auto new_name = call_get_text("Name Test Init Data", "Name:", name, validate_test_init_data_name))
				{
					util::trimstr(*new_name);
					if (!new_name->empty())
						name = *new_name;
				}

				test_init_data.push_back(delta);
				test_init_data_names.push_back(name);
				save_test_init_data(index);
				selected = test_init_data_val = index + 1;
				refresh_dlg();
				return false;
			}

			case message::EDIT:
			{
				if (!has_selection)
					return false;

				std::string error;
				zinitdata* zinit_test = apply_init_data_delta(&zinit, test_init_data[i], error);
				if (!zinit_test)
				{
					InfoDialog("Error applying init data delta", error).show();
					return false;
				}

				bool old_saved = saved;
				bool kept = call_init_dlg(*zinit_test, true, true);
				saved = old_saved;

				if (kept)
				{
					std::string delta = serialize_init_data_delta(&zinit, zinit_test);
					test_init_data[i] = delta;
					save_test_init_data(i);
				}

				delete zinit_test;
				return false;
			}

			case message::RENAME:
			{
				if (!has_selection)
					return false;

				auto name = call_get_text("Rename Test Init Data", "Name:", test_init_data_names[i], validate_test_init_data_name);
				if (!name)
					return false;

				util::trimstr(*name);
				test_init_data_names[i] = name->empty() ? fmt::format("Init Data {}", i + 1) : *name;
				save_test_init_data(i);
				refresh_dlg();
				return false;
			}

			case message::REMOVE:
			{
				if (!has_selection)
					return false;

				if (!alert_confirm("Delete Test Init Data",
					fmt::format("Delete \"{}\"? This can't be undone.", test_init_data_names[i])))
					return false;

				test_init_data.erase(test_init_data.begin() + i);
				test_init_data_names.erase(test_init_data_names.begin() + i);

				// They're saved numbered from 1, and loading stops at the first number that's
				// missing, so move the ones after it down and clear the last.
				for (int j = i; j < test_init_data.size(); j++)
					save_test_init_data(j);
				std::string qst_cfg_header = qst_cfg_header_from_path(filepath);
				int last = test_init_data.size() + 1;
				zc_set_config(qst_cfg_header.c_str(), fmt::format("test_init_data_{}", last).c_str(), "");
				zc_set_config(qst_cfg_header.c_str(), fmt::format("test_init_data_{}_name", last).c_str(), "");

				// If Test Quest was using it, go back to the default rather than picking another.
				if (test_init_data_val == i + 1)
					test_init_data_val = 0;
				else if (test_init_data_val > i + 1)
					test_init_data_val--;
				selected = std::min<int>(i + 1, test_init_data.size());
				refresh_dlg();
				return false;
			}

			case message::CLOSE:
				return true;
		}
		return false;
	}

private:
	GUI::ListData list_data;
	// The highlighted test init data, counting from 1.
	int selected = 0;
};

}

bool TestQstDialog::handleMessage(const GUI::DialogMessage<message>& msg)
{
	switch(msg.message)
	{
		case message::MANAGE_INIT_DATA:
			TestInitDataDialog().show();
			// Show changes to the test init data in the dropdown.
			rerun_dlg = true;
			return true;

		case message::OK:
		{
			if (!saved && zc_get_config("zquest", "test_mode_auto_save", true))
				onSave();

			std::string init_data, init_data_name;
			if (test_init_data_val)
			{
				init_data = test_init_data[test_init_data_val - 1];
				init_data_name = test_init_data_names[test_init_data_val - 1];
			}

			std::string record_path;
			if (zc_get_config("zquest", "test_mode_record", false))
			{
				std::error_code ec;
				std::filesystem::path replay_file_dir = std::filesystem::absolute(get_test_replay_dir(), ec);
				if (!ec)
					std::filesystem::create_directories(replay_file_dir, ec);
				if (ec)
				{
					InfoDialog("Test Quest", fmt::format("Couldn't create {} ({}), so this test won't be recorded.",
						replay_file_dir.string(), ec.message())).show();
				}
				else
					record_path = (replay_file_dir / fmt::format("test_{}.zplay", std::time(nullptr))).string();
			}

			PlayerArgs args = {{
				"-test",
				filepath,
				fmt::format("{}", test_start_dmap),
				fmt::format("{}", test_start_screen),
				fmt::format("{}", test_ret_sqr),
			}};
			if (!init_data.empty())
			{
				args.push_back({"-test-init-data", init_data});
				args.push_back({"-test-init-data-name", init_data_name});
			}
			if (zc_get_config("zquest", "test_mode_auto_restart", false))
				args.push_back({"-test-auto-restart"});
			if (!record_path.empty())
			{
				args.push_back({"-record", record_path});
				// Record the hero's position and a hash of each frame, so that asserting the
				// replay later catches changes to what's drawn, not just to RNG and events.
				args.push_back({"-replay-debug"});
			}
			test_killer.kill();
			test_killer = launch_player(args);
		}
		return true;
		
		case message::REPLAYS:
		{
			call_test_replays_dialog();
			return false;
		}

		case message::CANCEL:
			return true;
	}
	return false;
}
