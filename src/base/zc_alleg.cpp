#include "base/zc_alleg.h"
#include "a5alleg.h"
#include "allegro/inline/gfx.inl"
#include "allegro5/bitmap.h"
#include "tiles.h"

PACKFILE *pack_fopen_password(const char *filename, const char *mode, const char *password)
{
	packfile_password(password);
	PACKFILE *new_pf = pack_fopen(filename, mode);
	packfile_password("");
	return new_pf;
}

uint64_t file_size_ex_password(const char *filename, const char *password)
{
	packfile_password(password);
	uint64_t new_pf = file_size_ex(filename);
	packfile_password("");
	return new_pf;
}

bool alleg4_save_bitmap(BITMAP* source, int scale, const char* filename, AL_CONST RGB *pal)
{
	BITMAP* scaled = nullptr;
	if (scale != 1)
	{
		int w = source->w;
		int h = source->h;
		scaled = create_bitmap_ex(bitmap_color_depth(source), w*scale, h*scale);
		stretch_blit(source, scaled, 0, 0, w, h, 0, 0, w*scale, h*scale);
	}

	bool result;
	if (bitmap_color_depth(source) == 32)
	{
		BITMAP* a4bmp = scaled ? scaled : source;
		ALLEGRO_BITMAP* a5bmp = al_create_bitmap(a4bmp->w, a4bmp->h);
		all_render_a5_bitmap(a4bmp, a5bmp);
		result = al_save_bitmap(filename, a5bmp);
		al_destroy_bitmap(a5bmp);
	}
	else
	{
		PALETTE default_pal;
		if (!pal)
			get_palette(default_pal);
		result = save_bitmap(filename, scaled ? scaled : source, pal ? pal : default_pal) == 0;
	}

	destroy_bitmap(scaled);
	return result;
}

static int bitmap_depth = 8;

void zc_set_bitmap_depth(int depth)
{
	assert(depth == 8 || depth == 32);
	bitmap_depth = depth;
}

int zc_get_bitmap_depth()
{
	return bitmap_depth;
}

BITMAP* zc_create_bitmap(int width, int height)
{
	assert(bitmap_depth == 8 || bitmap_depth == 32);
	return create_bitmap_ex(bitmap_depth, width, height);
}

int zc_color(BITMAP* bmp, int color)
{
	if (bitmap_color_depth(bmp) == 32)
		color = getpalcolor(color);
	return color;
}

void clear_maskable_bitmap(BITMAP* bmp)
{
	clear_to_color(bmp, bitmap_mask_color(bmp));
}

void zc_rectfill(BITMAP* bmp, int x1, int y1, int x2, int y2, int color)
{
	rectfill(bmp, x1, y1, x2, y2, zc_color(bmp, color));
}

void zc_rect(BITMAP* bmp, int x1, int y1, int x2, int y2, int color)
{
	rect(bmp, x1, y1, x2, y2, zc_color(bmp, color));
}
