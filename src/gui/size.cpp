#include "gui/size.h"
#include "gui/common.h"
#include "zalleg/zalleg.h"
#include "core/fonts.h"

extern int dlgfontheight;
namespace GUI
{

int32_t Size::emSize()
{
	return dlgfontheight;
}

}
