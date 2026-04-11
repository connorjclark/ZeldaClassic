#include "zc/scripting/types/bitmap.h"

#include "base/check.h"
#include "base/general.h"
#include "components/zasm/defines.h"
#include "zc/ffscript.h"
#include "zc/scripting/script_object.h"

extern refInfo *ri;
extern int32_t sarg1;
extern int32_t sarg2;
extern int32_t sarg3;

// TODO ! static?
UserDataContainer<user_bitmap, MAX_USER_BITMAPS> user_bitmaps = {script_object_type::bitmap, "bitmap"};

int32_t bitmap_get_register(int32_t reg)
{
	int32_t ret = 0;

	switch (reg)
	{
		case BITMAPHEIGHT:
		{
			if (auto bmp = user_bitmaps.check(GET_REF(bitmapref)); bmp && bmp->u_bmp)
			{
				ret = bmp->height * 10000;
			}
			else
			{
				ret = -10000;
			}
			break;
		}
		case BITMAPWIDTH:
		{
			if (auto bmp = user_bitmaps.check(GET_REF(bitmapref)); bmp && bmp->u_bmp)
			{
				ret = bmp->width * 10000;
			}
			else
			{
				ret = -10000;
			}
			break;
		}
		case CREATEBITMAP:
		{
			ret=FFCore.do_create_bitmap();
			break;
		}

		default:
			NOTREACHED();
	}

	return ret;
}

void bitmap_set_register([[maybe_unused]] int32_t reg, [[maybe_unused]] int32_t value)
{
	NOTREACHED();
}
