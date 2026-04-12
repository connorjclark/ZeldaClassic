#ifndef ZC_SCRIPTING_BITMAP_H_
#define ZC_SCRIPTING_BITMAP_H_

#include <user_object.h>
#include "allegro5/display.h"
#include "allegro/gfx.h"
#include "base/ints.h"

#include <cstdint>

//User-generated / Script-Generated bitmap object
#define UBMPFLAG_FREEING               0x01
#define UBMPFLAG_CAN_DELETE            0x02

struct user_bitmap : public user_abstract_obj
{
	BITMAP* u_bmp;
	int32_t width;
	int32_t height;
	byte flags;

	user_bitmap() = default;
	user_bitmap(const user_bitmap&) = delete;

	~user_bitmap()
	{
		destroy_bitmap(u_bmp);
	}

	void destroy()
	{
		destroy_bitmap(u_bmp);
		width = 0;
		height = 0;
		flags = 0;
		u_bmp = NULL;
	}

	void free_obj()
	{
		flags |= UBMPFLAG_FREEING;
	}

	void mark_can_del()
	{
		flags |= UBMPFLAG_CAN_DELETE;
	}

	bool is_freeing()
	{
		return flags & UBMPFLAG_FREEING;
	}

	bool can_del()
	{
		return flags & UBMPFLAG_CAN_DELETE;
	}
};

user_bitmap* checkBitmap(int32_t ref);
user_bitmap* checkBitmap(int32_t ref, bool req_valid, bool skipError = false);

int32_t bitmap_get_register(int32_t reg);
void bitmap_set_register(int32_t reg, int32_t value);

#endif
