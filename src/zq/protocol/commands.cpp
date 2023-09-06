#include "zq/protocol/impl.h"
#include "zq/zq_class.h"
#include "zq/zquest.h"
#include "dialog/info.h"
#include "dialog/alert.h"

protocol::commands::show_message::result protocol::commands::show_message::handle(protocol::commands::show_message::params params)
{
	if (params.type == "info")
		InfoDialog(params.title, params.content).show();
	else if (params.type == "alert")
		// TODO: improve.
		AlertDialog(params.title, params.content, [](bool, bool){}).show();
	return {};
}

protocol::commands::quit::result protocol::commands::quit::handle(protocol::commands::quit::params params)
{
	exit(0);
	return {};
}

protocol::commands::load_quest::result protocol::commands::load_quest::handle(protocol::commands::load_quest::params params)
{
	protocol::commands::load_quest::result result;
	if (currently_loading_quest)
	{
		// TODO instead of failing, somehow enqueue the command and attempt later.
		result.error_code = -1;
		result.success = false;
	}
	else
	{
		result.error_code = ::load_quest("modules/classic/classic_1st.qst", params.show_progress);
		result.success = result.error_code == 0;
	}
	return result;
}

protocol::commands::get_items::result protocol::commands::get_items::handle(protocol::commands::get_items::params params)
{
	protocol::commands::get_items::result result;
	result.items.reserve(MAXITEMS);
	for (int i = 0; i < MAXITEMS; i++)
    {
		result.items.push_back({
			.id = i,
			.name = item_string[i],
			.tile = itemsbuf[i].tile,
		});
    }
	return result;
}

protocol::commands::get_item_names::result protocol::commands::get_item_names::handle(protocol::commands::get_item_names::params params)
{
	protocol::commands::get_item_names::result result;
	result.names.reserve(MAXITEMS);
	for (int i = 0; i < MAXITEMS; i++)
    {
		result.names.push_back(item_string[i]);
    }
	return result;
}

protocol::commands::click_screen::result protocol::commands::click_screen::handle(protocol::commands::click_screen::params params)
{
	// TODO implement
	protocol::commands::click_screen::result result;
	abort();
	return result;
}
