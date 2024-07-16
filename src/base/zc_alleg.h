#ifndef ZC_ALLEG_H_
#define ZC_ALLEG_H_

#include <allegro.h>
#include <allegro/internal/aintern.h>
#include <a5alleg.h>
#include "base/zfix.h"

#ifdef ALLEGRO_WINDOWS
#include <winalleg.h>
#endif

#if !defined(ALLEGRO_MACOSX)
#define KEY_ZC_LCONTROL KEY_LCONTROL
#define KEY_ZC_RCONTROL KEY_RCONTROL
#define CHECK_CTRL_CMD (key[KEY_LCONTROL] || key[KEY_RCONTROL])
#else
#define KEY_ZC_LCONTROL KEY_COMMAND
#define KEY_ZC_RCONTROL KEY_COMMAND
#define CHECK_CTRL_CMD key[KEY_COMMAND]
#endif
#define CHECK_SHIFT (key[KEY_LSHIFT] || key[KEY_RSHIFT])
#define CHECK_ALT (key[KEY_ALT] || key[KEY_ALTGR])

// https://www.allegro.cc/forums/thread/613716
#ifdef ALLEGRO_LEGACY_MSVC
   #include <limits.h>
   #ifdef PATH_MAX
      #undef PATH_MAX
   #endif
   #define PATH_MAX MAX_PATH
#endif

PACKFILE *pack_fopen_password(const char *filename, const char *mode, const char *password);
uint64_t file_size_ex_password(const char *filename, const char *password);

bool alleg4_save_bitmap(BITMAP* bitmap, int scale, const char* filename, AL_CONST RGB *pal = nullptr);

void clear_maskable_bitmap(BITMAP* bmp);

void zc_set_bitmap_depth(int depth);
int zc_get_bitmap_depth();

// Returns either an 8-bit or 32-bit bitmap, based on the value given set by `zc_set_bitmap_depth`.
BITMAP* zc_create_bitmap(int width, int height);
// Fills alpha channel of 32-bit bitmap with 255. Does nothing for other bitmaps.
// Resets a4 drawing mode to DRAW_MODE_SOLID.
void zc_fill_alpha_channel(BITMAP* bmp, int x1, int y1, int x2, int y2);
int zc_color(BITMAP* bmp, int color);
int zc_color(int color);
int col32(int color);

void zc_rectfill(BITMAP* bmp, int x1, int y1, int x2, int y2, int color);
void zc_rect(BITMAP* bmp, int x1, int y1, int x2, int y2, int color);
void zc_line(BITMAP* bmp, int x1, int y1, int x2, int y2, int color);
void zc_allegro_hline(BITMAP* bmp, int x1, int y, int x2, int color);

inline uint32_t getpalcolor(int color)
{
	extern PALETTE _current_palette;
	auto& col = _current_palette[color];
	uint8_t r = col.r * 4;
	uint8_t g = col.g * 4;
	uint8_t b = col.b * 4;
	return r + (g << 8) + (b << 16);
}

#endif
