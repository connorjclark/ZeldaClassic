#ifndef ZQ_DIALOG_TEST_QUEST_DLG_H_
#define ZQ_DIALOG_TEST_QUEST_DLG_H_

#include "gui/dialog.h"
#include "gui/checkbox.h"
#include "gui/text_field.h"
#include "base/process_management.h"
#include <functional>
#include <string>
#include <string_view>
#include <vector>

void call_testqst_dialog();

// A zplayer command line: each entry is a switch, followed by its values.
using PlayerArgs = std::vector<std::vector<std::string>>;

// Starts zplayer. On web there's no process: it opens in a browser tab, or a hidden frame if
// it's headless, and can be tracked (see em_open_player).
process_killer launch_player(const PlayerArgs& args, bool track = false);

// The name of the quest's test init data with these contents, or an empty string if none has them.
std::string find_test_init_data_name(const std::string& init_data);

/* Note: The string_views passed to setCheatCodes will include
 * null terminators.
 */
class TestQstDialog: public GUI::Dialog<TestQstDialog>
{
public:
	enum class message { REFR_INFO, MANAGE_INIT_DATA, OK, CANCEL, REPLAYS };

	TestQstDialog();

	std::shared_ptr<GUI::Widget> view() override;
	bool handleMessage(const GUI::DialogMessage<message>& msg);

private:
	GUI::ListData dmap_list;
};

#endif
