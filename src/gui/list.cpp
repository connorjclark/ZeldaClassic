#include "list.h"
#include "common.h"
#include "dialog.h"
#include "dialog_runner.h"
#include "../jwin.h"
#include <cassert>
#include <cmath>

namespace GUI
{

List::List():
	listData(nullptr), selectedIndex(0), selectedValue(0), message(-1), isABC(false)
{
	setPreferredWidth(20_em);
	setPreferredHeight(12_em);
	fgColor = jwin_pal[jcTEXTFG];
	bgColor = jwin_pal[jcTEXTBG];
}

void List::setListData(const ::GUI::ListData& newListData)
{
	listData = &newListData;
	jwinListData = newListData.getJWin(&widgFont);
}

void List::setSelectedValue(int32_t value)
{
	selectedValue = value;
	selectedIndex = -1;
	if(alDialog)
	{
		setIndex();
		alDialog->d1 = alDialog->d2=selectedIndex;
	}
}

void List::setSelectedIndex(int32_t index)
{
	selectedIndex = index;
	if(alDialog)
	{
		alDialog->d1 = selectedIndex;
		alDialog->d2 = selectedIndex;
	}
}

int32_t List::getSelectedValue() const
{
	if(alDialog)
	{
		int32_t index = alDialog->d1;
		return listData->getValue(index);
	}
	else
		return selectedValue;
}

void List::setIndex()
{
	// Find a valid selection. We'll take the first thing with a matching
	// value. If nothing matches exactly, take the one that's closest to
	// the selected value.
	selectedIndex = 0;
	int32_t minDiff = std::abs(selectedValue-listData->getValue(0));
	for(size_t i = 1; i < listData->size(); ++i)
	{
		int32_t value = listData->getValue(i);
		if(value == selectedValue)
		{
			selectedIndex = i;
			return;
		}
		else
		{
			int32_t diff = std::abs(selectedValue-value);
			if(diff < minDiff)
			{
				selectedIndex = i;
				minDiff=diff;
			}
		}
	}
}

void List::setIsABC(bool abc)
{
	isABC = abc;
	if(alDialog)
	{
		alDialog->proc = (isABC ? newGUIProc<jwin_abclist_proc> : newGUIProc<jwin_list_proc>);
	}
}

void List::applyVisibility(bool visible)
{
	Widget::applyVisibility(visible);
	if(alDialog) alDialog.applyVisibility(visible);
}

void List::applyDisabled(bool dis)
{
	Widget::applyDisabled(dis);
	if(alDialog) alDialog.applyDisabled(dis);
}

void List::realize(DialogRunner& runner)
{
	// An empty list might logically be valid, but there's currently
	// no way to get a value from it.
	assert(listData);
	assert(listData->size() > 0);
	if(selectedIndex < 0)
		setIndex();

	alDialog = runner.push(shared_from_this(), DIALOG {
		isABC ? newGUIProc<jwin_abclist_proc> : newGUIProc<jwin_list_proc>,
		x, y, getWidth(), getHeight(),
		fgColor, bgColor,
		0, // key
		getFlags(), // flags
		selectedIndex, selectedIndex, // d1, d2,
		&jwinListData, nullptr, nullptr // dp, dp2, dp3
	});
}

int32_t List::onEvent(int32_t event, MessageDispatcher& sendMessage)
{
	assert(event == geCHANGE_SELECTION);
	if(onSelectFunc)
		onSelectFunc(listData->getValue(alDialog->d1));
	if(message >= 0)
		sendMessage(message, listData->getValue(alDialog->d1));
	return -1;
}

}
