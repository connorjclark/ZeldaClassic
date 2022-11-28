#include "frame.h"
#include "dialog_runner.h"
#include "../jwin.h"

// int32_t d_dummy_proc(int32_t, DIALOG*, int32_t)
// {
	// return D_O_K;
// }

namespace GUI
{

Frame::Frame(): frameStyle(style::ETCHED)
{}

void Frame::setTitle(const std::string& newTitle)
{
	title = " "+newTitle+" ";
}

void Frame::applyVisibility(bool visible)
{
	Widget::applyVisibility(visible);
	if(alDialog)
	{
		alDialog.applyVisibility(visible);
		alDialog.applyVisibility(visible,1);
	}
	if(content)
		content->applyVisibility(visible);
}

void Frame::applyDisabled(bool dis)
{
	Widget::applyDisabled(dis);
	if(alDialog)
	{
		alDialog.applyDisabled(dis);
		alDialog.applyDisabled(dis,1);
	}
	if(content)
		content->applyDisabled(dis);
}

void Frame::calculateSize()
{
	if(content)
	{
		content->calculateSize();
		int32_t extraHeight;
		if(!title.empty())
			extraHeight = sized(8, 10);
		else
			extraHeight = sized(4, 8);
		setPreferredWidth(Size::pixels(content->getTotalWidth()+sized(4, 8)));
		setPreferredHeight(Size::pixels(content->getTotalHeight()+extraHeight));
	}
	else
	{
		setPreferredWidth(10_em);
		setPreferredHeight(5_em);
	}
	Widget::calculateSize();
}

void Frame::arrange(int32_t contX, int32_t contY, int32_t contW, int32_t contH)
{
	Widget::arrange(contX, contY, contW, contH);
	if(content)
	{
		if(is_large)
		{
			if(!title.empty())
				content->arrange(x+4, y+8, getWidth()-8, getHeight()-10);
			else
				content->arrange(x+4, y+4, getWidth()-8, getHeight()-8);
		}
		else
		{
			if(!title.empty())
				content->arrange(x+2, y+6, getWidth()-4, getHeight()-8);
			else
				content->arrange(x+2, y+2, getWidth()-4, getHeight()-4);
		}
	}
}

void Frame::realize(DialogRunner& runner)
{
	alDialog = runner.push(shared_from_this(), DIALOG {
		newGUIProc<jwin_frame_proc>,
		x, y, getWidth(), getHeight(),
		fgColor, bgColor,
		0, // key
		getFlags(), // flags
		static_cast<int32_t>(frameStyle), 0, // d1, d2
		nullptr, nullptr, nullptr // dp, dp2, dp3
	});

	if(!title.empty())
	{
		runner.push(shared_from_this(), DIALOG {
			jwin_text_proc,
			x+sized(3, 5), y-sized(2, 3), getWidth(), getHeight(),
			fgColor, bgColor,
			0,
			getFlags(),
			0, 0,
			title.data(), widgFont, nullptr // dp, dp2, dp3
		});
	}
	else // No title
	{
		// runner.push(shared_from_this(), DIALOG {
			// d_dummy_proc,
			// 0, 0, 0, 0,
			// 0, 0,
			// 0,
			// getFlags(),
			// 0, 0,
			// nullptr, nullptr, nullptr
		// });
	}

	if(content)
		content->realize(runner);
}

}
