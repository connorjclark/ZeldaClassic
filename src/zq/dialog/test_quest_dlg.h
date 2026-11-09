#ifndef ZC_DIALOG_CHEATCODES_H_
#define ZC_DIALOG_CHEATCODES_H_

#include "gui/dialog.h"
#include "gui/checkbox.h"
#include "gui/text_field.h"
#include <functional>
#include <string>
#include <string_view>

void call_testqst_dialog();

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
