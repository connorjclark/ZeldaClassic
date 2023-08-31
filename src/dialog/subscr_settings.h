#ifndef ZC_DIALOG_SUBSCR_SETTINGS_H
#define ZC_DIALOG_SUBSCR_SETTINGS_H

#include <gui/dialog.h>
#include <gui/window.h>
#include <gui/grid.h>
#include <initializer_list>
#include <string>
#include <string_view>
#include "subscr.h"

void call_subscrsettings_dialog();

class SubscrSettingsDialog: public GUI::Dialog<SubscrSettingsDialog>
{
public:
	enum class message
	{
		REFR_INFO, OK, CANCEL, REFR_SELECTOR
	};

	SubscrSettingsDialog();
	
	std::shared_ptr<GUI::Widget> view() override;
	bool handleMessage(const GUI::DialogMessage<message>& msg);

protected:
	std::shared_ptr<GUI::Window> window;
	std::shared_ptr<GUI::Grid> selector_grid;
	byte ty;
	ZCSubscreen local_subref;
	
	GUI::ListData list_sfx;
	
	void refr_selector();
	void refr_info();
};

#endif
