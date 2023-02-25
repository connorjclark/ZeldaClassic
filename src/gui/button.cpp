#include "button.h"
#include "common.h"
#include "dialog.h"
#include "dialog_runner.h"
#include "../jwin.h"
#include <algorithm>
#include <utility>

int32_t next_press_key()
{
	return readkey()>>8;
}
void kb_getkey(DIALOG *d)
{
	d->flags|=D_SELECTED;
	
	scare_mouse();
	jwin_button_proc(MSG_DRAW,d,0);
	jwin_draw_win(screen, (screen->w-160)/2, (screen->h-48)/2, 160, 48, FR_WIN);
	textout_centre_ex(screen, font, "Press a key", screen->w/2, screen->h/2 - 8, jwin_pal[jcBOXFG],jwin_pal[jcBOX]);
	textout_centre_ex(screen, font, "ESC to cancel", screen->w/2, screen->h/2, jwin_pal[jcBOXFG],jwin_pal[jcBOX]);
	unscare_mouse();
	
	update_hw_screen(true);
	
	clear_keybuf();
	int32_t k = next_press_key();
	clear_keybuf();
	
	if(d->dp3)
	{
		bool valid_key = k>0 && k<123;
		if(k > 46 && k < 60 && k != KEY_F11) //f keys, esc? Allow F11!
			valid_key = false;
		if(valid_key)
			*((int32_t*)d->dp3) = k;
	}
	
	d->flags&=~D_SELECTED;
}

void kb_clearkey(DIALOG *d)
{
	d->flags|=D_SELECTED;
	
	scare_mouse();
	jwin_button_proc(MSG_DRAW,d,0);
	jwin_draw_win(screen, (screen->w-160)/2, (screen->h-48)/2, 160, 48, FR_WIN);
	textout_centre_ex(screen, font, "Press any key to clear", screen->w/2, screen->h/2 - 8, jwin_pal[jcBOXFG],jwin_pal[jcBOX]);
	textout_centre_ex(screen, font, "ESC to cancel", screen->w/2, screen->h/2, jwin_pal[jcBOXFG],jwin_pal[jcBOX]);
	unscare_mouse();
	
	update_hw_screen(true);
	
	clear_keybuf();
	int32_t k = next_press_key();
	clear_keybuf();
	
	if (d->dp3)
	{
		if(k != KEY_ESC)
			*((int32_t*)d->dp3) = 0;
	}
	
	d->flags&=~D_SELECTED;
}

int32_t d_kbutton_proc(int32_t msg,DIALOG *d,int32_t c)
{
	switch(msg)
	{
	case MSG_KEY:
	case MSG_CLICK:

		kb_getkey(d);
		
		while(gui_mouse_b()) {
			clear_keybuf();
			rest(1);
		}
		GUI_EVENT(d, geCLICK);
		return D_REDRAW;
	}

	return jwin_button_proc(msg,d,c);
}
int32_t d_k_clearbutton_proc(int32_t msg,DIALOG *d,int32_t c)
{
	switch(msg)
	{
	case MSG_KEY:
	case MSG_CLICK:

		kb_clearkey(d);
		
		while(gui_mouse_b()) {
			clear_keybuf();
			rest(1);
		}
		GUI_EVENT(d, geCLICK);
		return D_REDRAW;
	}

	return jwin_button_proc(msg,d,c);
}

namespace GUI
{

Button::Button(): text(), message(-1), btnType(type::BASIC), bound_kb(nullptr)
{
	setPreferredHeight(3_em);
}

void Button::setType(type newType)
{
	btnType = newType;
}
void Button::setBoundKB(int32_t* kb_ptr)
{
	bound_kb = kb_ptr;
}
void Button::setText(std::string newText)
{
	text = std::move(newText);
}

void Button::setOnPress(std::function<void()> newOnPress)
{
	onPress = std::move(newOnPress);
}

void Button::applyVisibility(bool visible)
{
	Widget::applyVisibility(visible);
	if(alDialog) alDialog.applyVisibility(visible);
}

void Button::applyDisabled(bool dis)
{
	Widget::applyDisabled(dis);
	if(alDialog) alDialog.applyDisabled(dis);
}

void Button::calculateSize()
{
	setPreferredWidth(16_px+Size::pixels(gui_text_width(widgFont, text.c_str())));
	Widget::calculateSize();
}

void Button::applyFont(FONT* newFont)
{
	if(alDialog)
	{
		alDialog->dp2 = newFont;
	}
	Widget::applyFont(newFont);
}

void Button::realize(DialogRunner& runner)
{
	Widget::realize(runner);
	switch(btnType)
	{
		case type::BASIC:
			alDialog = runner.push(shared_from_this(), DIALOG {
				newGUIProc<jwin_button_proc>,
				x, y, getWidth(), getHeight(),
				fgColor, bgColor,
				getAccelKey(text),
				getFlags(),
				0, 0, // d1, d2
				text.data(), widgFont, nullptr // dp, dp2, dp3
			});
			break;
		case type::BIND_KB:
			alDialog = runner.push(shared_from_this(), DIALOG {
				newGUIProc<d_kbutton_proc>,
				x, y, getWidth(), getHeight(),
				fgColor, bgColor,
				getAccelKey(text),
				getFlags(),
				0, 0, // d1, d2
				text.data(), widgFont, bound_kb // dp, dp2, dp3
			});
			break;
		case type::BIND_KB_CLEAR:
			alDialog = runner.push(shared_from_this(), DIALOG {
				newGUIProc<d_k_clearbutton_proc>,
				x, y, getWidth(), getHeight(),
				fgColor, bgColor,
				getAccelKey(text),
				getFlags(),
				0, 0, // d1, d2
				text.data(), widgFont, bound_kb // dp, dp2, dp3
			});
			break;
	}
}

int32_t Button::onEvent(int32_t event, MessageDispatcher& sendMessage)
{
	assert(event == geCLICK);
	// jwin_button_proc doesn't seem to allow for a non-toggle button...
	alDialog->flags &= ~D_SELECTED;
	
	if(onPress)
		onPress();
	if(message >= 0)
		sendMessage(message, MessageArg::none);
	return -1;
}

}
