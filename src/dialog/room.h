#ifndef ZC_DIALOG_ROOM_H
#define ZC_DIALOG_ROOM_H

#include <gui/dialog.h>
#include <gui/drop_down_list.h>
#include <gui/label.h>
#include <gui/switcher.h>
#include <gui/text_field.h>
#include <functional>

class RoomDialog: public GUI::Dialog<RoomDialog>
{
public:
	enum class message
	{
		SET_ROOM, SET_ARGUMENT, SET_GUY, SET_STRING, ROOM_INFO, OK, CANCEL
	};

	RoomDialog(int32_t room, int32_t argument, int32_t guy, int32_t string,
		std::function<void(int32_t, int32_t, int32_t, int32_t)> setRoomVars);

	std::shared_ptr<GUI::Widget> view() override;
	bool handleMessage(const GUI::DialogMessage<message>& msg);

private:
	GUI::ListData itemListData, shopListData, bshopListData, infoShopListData, stringListData;
	std::shared_ptr<GUI::DropDownList> shopDD, bshopDD, infoShopDD, itemDD;
	std::shared_ptr<GUI::TextField> argTF;
	std::shared_ptr<GUI::Switcher> argSwitcher;
	std::shared_ptr<GUI::Label> argLabel;
	struct
	{
		int32_t type, argument, guy, string;
	} room;
	std::function<void(int32_t, int32_t, int32_t, int32_t)> setRoomVars;

	/* Called when the room is changed to show the appropriate
	* argument selector and set its value.
	*/
	void setArgField();

	/* Called when the dialog is closed to get the argument
	 * limited to legal values.
	 */
	int32_t getArgument() const;

	/* Returns a string describing the currently selected room. */
	const char* getRoomInfo() const;
};

#endif
