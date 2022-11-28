#ifndef ZC_GUI_TEXTFIELD_H
#define ZC_GUI_TEXTFIELD_H

#include "widget.h"
#include "dialog_ref.h"
#include <memory>
#include <string_view>

namespace GUI
{

class TextField: public Widget
{
public:
	enum class type
	{
		TEXT, INT_DECIMAL, INT_HEX, SWAP_BYTE, SWAP_SSHORT, SWAP_ZSINT, FIXED_DECIMAL
	};
	
	TextField();

	/* Set the text field's input type. This determines how the text
	 * will be interpreted when a message is sent.
	 */
	inline void setType(type newType)
	{
		tfType = newType;
	}
	
	bool isSwapType()
	{
		switch(tfType)
		{
			case type::SWAP_BYTE:
			case type::SWAP_SSHORT:
			case type::SWAP_ZSINT:
				return true;
			default:
				return false;
		}
	}

	/* Set the current text. If it's longer than the current maximum length,
	 * only that many characters will be kept. However, if the maximum length
	 * is 0, the maximum length will be set to the length of the text.
	 */
	void setText(std::string_view newText);

	/* Returns the current text. This does not interpret the text according to
	 * the type. The string_view will include a null terminator. It's owned by
	 * the TextField, so don't hold on to it after the dialog is closed.
	 */
	std::string_view getText();
	
	/* Set the int32_t value, unused for type::TEXT
	 */
	void setVal(int32_t val);
	
	/* Gets the value as an integer.
	 * Attempts to read 'type::TEXT' as a decimal int32_t value.
	 */
	int32_t getVal();
	
	void setLowBound(int32_t low);
	void setHighBound(int32_t high);
	
	/* Set the maximum length of the text, not including the null terminator.
	 */
	void setMaxLength(size_t newMax);

	/* Sets the message to send when the enter key is pressed. Note that
	 * the type of the argument varies depending on the text field's type.
	 * If set to Text, the argument will be a std::string_view. If set to
	 * IntDecimal or IntHex, it will be an int32_t.
	 */
	template<typename T>
	RequireMessage<T> onEnter(T m)
	{
		onEnterMsg = static_cast<int32_t>(m);
	}

	/* Sets the message to send whenever the text changes. Like onEnter,
	 * the type of the argument varies depending on the text field's type.
	 */
	template<typename T>
	RequireMessage<T> onValueChanged(T m)
	{
		onValueChangedMsg = static_cast<int32_t>(m);
	}

	/* Sets a function to be called on value change. */
	void setOnValChanged(std::function<void(type,std::string_view,int32_t)> newOnValChanged);
	
	void setFixedPlaces(size_t places);
	void setSwapType(int32_t newtype);
	
	size_t get_str_pos();
private:
	std::unique_ptr<char[]> buffer;
	int32_t startVal;
	int32_t fixedPlaces;
	int32_t lbound, ubound;
	type tfType;
	int32_t swap_type_start;
	size_t maxLength;
	bool forced_length;
	DialogRef alDialog;
	DialogRef swapBtnDialog;
	int32_t onEnterMsg, onValueChangedMsg;
	bool valSet;
	std::function<void(type,std::string_view,int32_t)> onValChanged;
	
	void applyReadableFont();
	void applyVisibility(bool visible) override;
	void applyDisabled(bool dis) override;
	void realize(DialogRunner& runner) override;
	int32_t onEvent(int32_t event, MessageDispatcher& sendMessage) override;
	void applyFont(FONT* newFont) override;
	void updateReadOnly(bool ro) override;
	
	void _updateBuf(size_t sz);
	void check_len(size_t min);
};

}

#endif
