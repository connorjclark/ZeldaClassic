#ifndef ZC_DIALOG_ALERTFUNC_H
#define ZC_DIALOG_ALERTFUNC_H

#include "info.h"
#include <gui/button.h>
#include <gui/grid.h>

// A basic dialog that just shows some lines of text and a set of function buttons
class AlertFuncDialog: public InfoDialog
{
public:
	enum class message { OK, BTN };
	
	AlertFuncDialog(std::string title, std::string text, uint32_t numButtons = 0, uint32_t focused_button = 0, ...);
	AlertFuncDialog(std::string title, std::vector<std::string_view> lines, uint32_t numButtons = 0, uint32_t focused_button = 0, ...);
	
	std::shared_ptr<GUI::Widget> view() override;
	bool handleMessage(const GUI::DialogMessage<int32_t>& msg) override;

private:
	bool didend;
	std::shared_ptr<GUI::Grid> buttonRow;
	std::vector<std::shared_ptr<GUI::Button>> buttons;
	
	void initButtons(va_list args, uint32_t numButtons, uint32_t focused_button);
};

#endif
