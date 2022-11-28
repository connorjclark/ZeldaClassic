#ifndef ZC_GUI_QRCHECKBOX_H
#define ZC_GUI_QRCHECKBOX_H

#include "widget.h"
#include "checkbox.h"
#include "dialog_ref.h"

namespace GUI
{

class QRCheckbox: public Checkbox
{
public:
	QRCheckbox();
	
	void setQR(int32_t newqr);
	
private:
	int32_t qr;

	int32_t onEvent(int32_t event, MessageDispatcher& sendMessage) override;
};

}

#endif
