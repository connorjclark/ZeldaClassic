//--------------------------------------------------------
//  Zelda Classic
//  by Jeremy Craner, 1999-2000
//
//  colors.cc
//
//  Palette data for Zelda Classic.
//
//--------------------------------------------------------

#include "precompiled.h" //always first

#include "base/zc_alleg.h"
#include "base/colors.h"

byte nes_pal[] =
{
	31,31,31,                                                 //  0
	0, 0,63,                                                  //  1
	0, 0,47,                                                  //  2
	17,10,47,                                                 //  3
	37, 0,33,                                                 //  4
	42, 0, 8,                                                 //  5
	42, 4, 0,                                                 //  6
	34, 5, 0,                                                 //  7
	20,12, 0,                                                 //  8
	0,30, 0,                                                  //  9
	0,26, 0,                                                  // 10
	0,22, 0,                                                  // 11
	0,16,22,                                                  // 12
	0, 0, 0,                                                  // 13 0D
	0, 0, 0,                                                  // 14 0E
	0, 0, 0,                                                  // 15 0F
	47,47,47,                                                 // 16
	0,30,62,                                                  // 17
	0,22,62,                                                  // 18
	26,17,63,                                                 // 19
	54, 0,51,                                                 // 20
	57, 0,22,                                                 // 21
	62,14, 0,                                                 // 22
	57,23, 4,                                                 // 23
	43,31, 0,                                                 // 24
	0,46, 0,                                                  // 25
	0,42, 0,                                                  // 26
	0,42,17,                                                  // 27
	0,34,34,                                                  // 28
	0, 0, 0,                                                  // 29
	0, 0, 0,                                                  // 30
	0, 0, 0,                                                  // 31
	62,62,62,                                                 // 32
	15,47,63,                                                 // 33
	26,34,63,                                                 // 34
	38,30,62,                                                 // 35
	62,30,62,                                                 // 36
	62,22,38,                                                 // 37
	62,30,22,                                                 // 38
	63,40,17,                                                 // 39
	62,46, 0,                                                 // 40
	46,62, 6,                                                 // 41
	22,54,21,                                                 // 42
	22,62,38,                                                 // 43
	0,58,54,                                                  // 44
	30,30,30,                                                 // 45
	0, 0, 0,                                                  // 46
	0, 0, 0,                                                  // 47
	63,63,63,                                                 // 48 !
	41,57,63,                                                 // 49
	52,52,62,                                                 // 50
	54,46,62,                                                 // 51
	62,46,62,                                                 // 52
	62,41,48,                                                 // 53
	60,52,44,                                                 // 54
	63,56,42,                                                 // 55
	62,54,30,                                                 // 56
	54,62,30,                                                 // 57
	46,62,46,                                                 // 58
	46,62,54,                                                 // 59
	0,63,63,                                                  // 60
	62,54,62,                                                 // 61
	0,54,50,                                                  // 62
	31,63,63                                                  // 63
};

byte nes_colors[] =
{

	// ---=== Main Palette ===---
	
	// background
	
	0x0F,0x30,0x00,0x12,0x0F,                                 // cset 0    system colors (always the same)
	0x0F,0x16,0x27,0x36,0x0F,                                 // cset 1      `'
	
	0x0F,0x1A,0x37,0x12,0x0F,                                 // cset 2    level colors
	0x0F,0x17,0x37,0x12,0x0F,                                 // cset 3      `'
	0x0F,0x0F,0x0F,0x0F,0x0F,                                 // cset 4      `'
	
	0x0F,0x0F,0x0F,0x0F,0x0F,                                 // cset 5    extra
	
	// foreground
	
	0x0F,0x29,0x27,0x17,0x0F,                                 // cset 6    system  (6 = Hero's colors)
	0x0F,0x02,0x22,0x30,0x0F,                                 // cset 7      `'
	0x0F,0x16,0x27,0x30,0x0F,                                 // cset 8      `'
	0x0F,0x0F,0x1C,0x16,0x0F,                                 // cset 9    level sprites
	
	0x0F,0x0F,0x0F,0x0F,0x0F,                                 // cset 10   extras
	0x0F,0x0F,0x0F,0x0F,0x0F,                                 // cset 11     `'
	0x0F,0x0F,0x0F,0x0F,0x0F,                                 // cset 12     `'
	0x0F,0x0F,0x0F,0x0F,0x0F,                                 // cset 13     `'
	0x0F,0x0F,0x0F,0x0F,0x0F,                                 // cset 14     `'
	
	// ---=== Level Palettes ===---
	
	// overworld (level 0)
	0x0F,0x1A,0x37,0x12,0x0F,                                 // cset 2    normal
	0x0F,0x17,0x37,0x12,0x0F,                                 // cset 3
	0x0F,0x0F,0x0F,0x0F,0x0F,                                 // cset 4
	0x0F,0x0F,0x1C,0x16,0x0F,                                 // cset 9
	
	0x0F,0x0F,0x0F,0x0F,0x0F,                                 // cset 2    fading #1
	0x0F,0x0F,0x0F,0x0F,0x0F,                                 // cset 3
	0x0F,0x0F,0x0F,0x0F,0x0F,                                 // cset 4
	
	0x0F,0x0F,0x0F,0x0F,0x0F,                                 // cset 2    fading #2
	0x0F,0x0F,0x0F,0x0F,0x0F,                                 // cset 3
	0x0F,0x0F,0x0F,0x0F,0x0F,                                 // cset 4
	
	0x0F,0x0F,0x0F,0x0F,0x0F,                                 // cset 2    fading #3
	0x0F,0x0F,0x0F,0x0F,0x0F,                                 // cset 3
	0x0F,0x0F,0x0F,0x0F,0x0F,                                 // cset 4
	
	// level 1
	0x0F,0x0C,0x1C,0x2C,0x0F,
	0x0F,0x12,0x1C,0x2C,0x0F,
	0x0F,0x0F,0x0F,0x0F,0x0F,
	0x0F,0x0C,0x1C,0x2C,0x0F,
	
	0x0F,0x0C,0x0C,0x1C,0x0F,
	0x0F,0x11,0x0C,0x1C,0x0F,
	0x0F,0x0F,0x0F,0x0F,0x0F,
	
	0x0F,0x0F,0x0C,0x0C,0x0F,
	0x0F,0x02,0x0C,0x0C,0x0F,
	0x0F,0x0F,0x0F,0x0F,0x0F,
	
	0x0F,0x0F,0x0F,0x0C,0x0F,
	0x0F,0x0F,0x0F,0x0F,0x0F,
	0x0F,0x0F,0x0F,0x0F,0x0F,
	
	// level 2
	0x0F,0x02,0x12,0x22,0x0F,
	0x0F,0x16,0x12,0x22,0x0F,
	0x0F,0x0F,0x0F,0x0F,0x0F,
	0x0F,0x02,0x12,0x22,0x0F,
	
	0x0F,0x02,0x02,0x12,0x0F,
	0x0F,0x17,0x02,0x12,0x0F,
	0x0F,0x0F,0x0F,0x0F,0x0F,
	
	0x0F,0x0F,0x02,0x02,0x0F,
	0x0F,0x07,0x02,0x02,0x0F,
	0x0F,0x0F,0x0F,0x0F,0x0F,
	
	0x0F,0x0F,0x0F,0x02,0x0F,
	0x0F,0x0F,0x0F,0x0F,0x0F,
	0x0F,0x0F,0x0F,0x0F,0x0F,
	
	// level 3
	0x0F,0x0B,0x1B,0x2B,0x0F,
	0x0F,0x16,0x1B,0x2B,0x0F,
	0x0F,0x0F,0x0F,0x0F,0x0F,
	0x0F,0x0B,0x1B,0x2B,0x0F,
	
	0x0F,0x0B,0x0B,0x1B,0x0F,
	0x0F,0x17,0x0B,0x1B,0x0F,
	0x0F,0x0F,0x0F,0x0F,0x0F,
	
	0x0F,0x0F,0x0B,0x0B,0x0F,
	0x0F,0x07,0x0B,0x0B,0x0F,
	0x0F,0x0F,0x0F,0x0F,0x0F,
	
	0x0F,0x0F,0x0F,0x0B,0x0F,
	0x0F,0x0F,0x0F,0x0F,0x0F,
	0x0F,0x0F,0x0F,0x0F,0x0F,
	
	// level 4
	0x0F,0x08,0x18,0x28,0x0F,
	0x0F,0x12,0x18,0x28,0x0F,
	0x0F,0x0F,0x0F,0x0F,0x0F,
	0x0F,0x0F,0x18,0x28,0x0F,
	
	0x0F,0x08,0x08,0x18,0x0F,
	0x0F,0x11,0x08,0x18,0x0F,
	0x0F,0x0F,0x0F,0x0F,0x0F,
	
	0x0F,0x0F,0x08,0x08,0x0F,
	0x0F,0x02,0x08,0x08,0x0F,
	0x0F,0x0F,0x0F,0x0F,0x0F,
	
	0x0F,0x0F,0x0F,0x08,0x0F,
	0x0F,0x0F,0x0F,0x0F,0x0F,
	0x0F,0x0F,0x0F,0x0F,0x0F,
	
	// level 5
	0x0F,0x0A,0x1A,0x2A,0x0F,
	0x0F,0x16,0x1A,0x2A,0x0F,
	0x0F,0x0F,0x0F,0x0F,0x0F,
	0x0F,0x0A,0x1A,0x2A,0x0F,
	
	0x0F,0x0A,0x0A,0x1A,0x0F,
	0x0F,0x17,0x0A,0x1A,0x0F,
	0x0F,0x0F,0x0F,0x0F,0x0F,
	
	0x0F,0x0F,0x0A,0x0A,0x0F,
	0x0F,0x07,0x0A,0x0A,0x0F,
	0x0F,0x0F,0x0F,0x0F,0x0F,
	
	0x0F,0x0F,0x0F,0x0A,0x0F,
	0x0F,0x0F,0x0F,0x0F,0x0F,
	0x0F,0x0F,0x0F,0x0F,0x0F,
	
	// level 6
	0x0F,0x08,0x18,0x28,0x0F,
	0x0F,0x16,0x18,0x28,0x0F,
	0x0F,0x0F,0x0F,0x0F,0x0F,
	0x0F,0x08,0x18,0x28,0x0F,
	
	0x0F,0x08,0x08,0x18,0x0F,
	0x0F,0x17,0x08,0x18,0x0F,
	0x0F,0x0F,0x0F,0x0F,0x0F,
	
	0x0F,0x0F,0x08,0x08,0x0F,
	0x0F,0x07,0x08,0x08,0x0F,
	0x0F,0x0F,0x0F,0x0F,0x0F,
	
	0x0F,0x0F,0x0F,0x08,0x0F,
	0x0F,0x0F,0x0F,0x0F,0x0F,
	0x0F,0x0F,0x0F,0x0F,0x0F,
	
	// level 7
	0x0F,0x0A,0x1A,0x2A,0x0F,
	0x0F,0x12,0x1A,0x2A,0x0F,
	0x0F,0x0F,0x0F,0x0F,0x0F,
	0x0F,0x0A,0x1A,0x2A,0x0F,
	
	0x0F,0x0A,0x0A,0x1A,0x0F,
	0x0F,0x11,0x0A,0x1A,0x0F,
	0x0F,0x0F,0x0F,0x0F,0x0F,
	
	0x0F,0x0F,0x0A,0x0A,0x0F,
	0x0F,0x02,0x0A,0x0A,0x0F,
	0x0F,0x0F,0x0F,0x0F,0x0F,
	
	0x0F,0x0F,0x0F,0x0A,0x0F,
	0x0F,0x0F,0x0F,0x0F,0x0F,
	0x0F,0x0F,0x0F,0x0F,0x0F,
	
	// level 8
	0x0F,0x00,0x10,0x20,0x0F,
	0x0F,0x22,0x10,0x20,0x0F,
	0x0F,0x0F,0x0F,0x0F,0x0F,
	0x0F,0x00,0x10,0x30,0x0F,
	
	0x0F,0x00,0x00,0x10,0x0F,
	0x0F,0x11,0x00,0x10,0x0F,
	0x0F,0x0F,0x0F,0x0F,0x0F,
	
	0x0F,0x0F,0x00,0x00,0x0F,
	0x0F,0x02,0x00,0x00,0x0F,
	0x0F,0x0F,0x0F,0x0F,0x0F,
	
	0x0F,0x0F,0x0F,0x00,0x0F,
	0x0F,0x0F,0x0F,0x0F,0x0F,
	0x0F,0x0F,0x0F,0x0F,0x0F,
	
	// level 9
	0x0F,0x00,0x10,0x20,0x0F,
	0x0F,0x16,0x10,0x20,0x0F,
	0x0F,0x0F,0x0F,0x0F,0x0F,
	0x0F,0x0F,0x10,0x30,0x0F,
	
	0x0F,0x00,0x00,0x10,0x0F,
	0x0F,0x17,0x00,0x10,0x0F,
	0x0F,0x0F,0x0F,0x0F,0x0F,
	
	0x0F,0x0F,0x00,0x00,0x0F,
	0x0F,0x07,0x00,0x00,0x0F,
	0x0F,0x0F,0x0F,0x0F,0x0F,
	
	0x0F,0x0F,0x0F,0x00,0x0F,
	0x0F,0x0F,0x0F,0x0F,0x0F,
	0x0F,0x0F,0x0F,0x0F,0x0F,
	
	// cave (10)
	0x0F,0x1A,0x37,0x12,0x0F,
	0x0F,0x07,0x0F,0x17,0x0F,
	0x0F,0x0F,0x0F,0x0F,0x0F,
	0x0F,0x0F,0x1C,0x16,0x0F,
	
	0x0F,0x0F,0x0F,0x0F,0x0F,
	0x0F,0x0F,0x0F,0x0F,0x0F,
	0x0F,0x0F,0x0F,0x0F,0x0F,
	
	0x0F,0x0F,0x0F,0x0F,0x0F,
	0x0F,0x0F,0x0F,0x0F,0x0F,
	0x0F,0x0F,0x0F,0x0F,0x0F,
	
	0x0F,0x0F,0x0F,0x0F,0x0F,
	0x0F,0x0F,0x0F,0x0F,0x0F,
	0x0F,0x0F,0x0F,0x0F,0x0F,
	
	// passage way (11)
	0x0F,0x00,0x10,0x20,0x0F,
	0x0F,0x00,0x10,0x20,0x0F,
	0x0F,0x0F,0x0F,0x0F,0x0F,
	0x0F,0x00,0x10,0x30,0x0F,
	
	0x0F,0x00,0x00,0x10,0x0F,
	0x0F,0x00,0x00,0x10,0x0F,
	0x0F,0x0F,0x0F,0x0F,0x0F,
	
	0x0F,0x0F,0x00,0x00,0x0F,
	0x0F,0x0F,0x00,0x00,0x0F,
	0x0F,0x0F,0x0F,0x0F,0x0F,
	
	0x0F,0x0F,0x0F,0x00,0x0F,
	0x0F,0x0F,0x0F,0x0F,0x0F,
	0x0F,0x0F,0x0F,0x0F,0x0F,
	
	// extra 1  (12)
	0x0F,0x0F,0x0F,0x0F,0x0F,
	0x0F,0x0F,0x0F,0x0F,0x0F,
	0x0F,0x0F,0x0F,0x0F,0x0F,
	0x0F,0x0F,0x0F,0x0F,0x0F,
	
	0x0F,0x0F,0x0F,0x0F,0x0F,
	0x0F,0x0F,0x0F,0x0F,0x0F,
	0x0F,0x0F,0x0F,0x0F,0x0F,
	
	0x0F,0x0F,0x0F,0x0F,0x0F,
	0x0F,0x0F,0x0F,0x0F,0x0F,
	0x0F,0x0F,0x0F,0x0F,0x0F,
	
	0x0F,0x0F,0x0F,0x0F,0x0F,
	0x0F,0x0F,0x0F,0x0F,0x0F,
	0x0F,0x0F,0x0F,0x0F,0x0F,
	
	// extra 2  (13)
	0x0F,0x0F,0x0F,0x0F,0x0F,
	0x0F,0x0F,0x0F,0x0F,0x0F,
	0x0F,0x0F,0x0F,0x0F,0x0F,
	0x0F,0x0F,0x0F,0x0F,0x0F,
	
	0x0F,0x0F,0x0F,0x0F,0x0F,
	0x0F,0x0F,0x0F,0x0F,0x0F,
	0x0F,0x0F,0x0F,0x0F,0x0F,
	
	0x0F,0x0F,0x0F,0x0F,0x0F,
	0x0F,0x0F,0x0F,0x0F,0x0F,
	0x0F,0x0F,0x0F,0x0F,0x0F,
	
	0x0F,0x0F,0x0F,0x0F,0x0F,
	0x0F,0x0F,0x0F,0x0F,0x0F,
	0x0F,0x0F,0x0F,0x0F,0x0F,
	
	// extra 3  (14)
	0x0F,0x0F,0x0F,0x0F,0x0F,
	0x0F,0x0F,0x0F,0x0F,0x0F,
	0x0F,0x0F,0x0F,0x0F,0x0F,
	0x0F,0x0F,0x0F,0x0F,0x0F,
	
	0x0F,0x0F,0x0F,0x0F,0x0F,
	0x0F,0x0F,0x0F,0x0F,0x0F,
	0x0F,0x0F,0x0F,0x0F,0x0F,
	
	0x0F,0x0F,0x0F,0x0F,0x0F,
	0x0F,0x0F,0x0F,0x0F,0x0F,
	0x0F,0x0F,0x0F,0x0F,0x0F,
	
	0x0F,0x0F,0x0F,0x0F,0x0F,
	0x0F,0x0F,0x0F,0x0F,0x0F,
	0x0F,0x0F,0x0F,0x0F,0x0F,
	
	// ---=== 30 Extra Sprite Palettes ===---
	
	0x0F,0x0A,0x29,0x30,0x0F,                                 // aquamentus
	0x0F,0x2A,0x1A,0x0C,0x0F,                                 // gleeok
	0x0F,0x17,0x27,0x30,0x0F,                                 // digdogger
	0x0F,0x16,0x3E,0x3F,0x0F,                                 // teal Ganon
	0x0F,0x07,0x17,0x30,0x0F,                                 // brown Ganon
	
	0x0F,0x27,0x06,0x16,0x0F,                                 // dead Ganon
	0x0F,0x32,0x27,0x17,0x0F,                                 // Hero w/ blue ring
	0x0F,0x16,0x27,0x17,0x0F,                                 // Hero w/ red ring
	0x0F,0x0F,0x0F,0x0F,0x0F,
	0x0F,0x0F,0x0F,0x0F,0x0F,
	
	0x0F,0x0F,0x0F,0x0F,0x0F,
	0x0F,0x0F,0x0F,0x0F,0x0F,
	0x0F,0x0F,0x0F,0x0F,0x0F,
	0x0F,0x0F,0x0F,0x0F,0x0F,
	0x0F,0x0F,0x0F,0x0F,0x0F,
	
	0x0F,0x0F,0x0F,0x0F,0x0F,
	0x0F,0x0F,0x0F,0x0F,0x0F,
	0x0F,0x0F,0x0F,0x0F,0x0F,
	0x0F,0x0F,0x0F,0x0F,0x0F,
	0x0F,0x0F,0x0F,0x0F,0x0F,
	
	0x0F,0x0F,0x0F,0x0F,0x0F,
	0x0F,0x0F,0x0F,0x0F,0x0F,
	0x0F,0x0F,0x0F,0x0F,0x0F,
	0x0F,0x0F,0x0F,0x0F,0x0F,
	0x0F,0x0F,0x0F,0x0F,0x0F,
	
	0x0F,0x0F,0x0F,0x0F,0x0F,
	0x0F,0x0F,0x0F,0x0F,0x0F,
	0x0F,0x0F,0x0F,0x0F,0x0F,
	0x0F,0x0F,0x0F,0x0F,0x0F,
	0x0F,0x0F,0x0F,0x0F,0x0F
};

// Global pointer to current color data used by "pal.cc" routines.
// Must be allocated to pdTOTAL*16 bytes before calling init_colordata().
extern byte *colordata;

/* create_zc_trans_table:
  *  Constructs a translucency color table for the specified palette. The
  *  r, g, and b parameters specifiy the solidity of each color component,
  *  ranging from 0 (totally transparent) to 255 (totally solid). If the
  *  callback function is not NULL, it will be called 256 times during the
  *  calculation, allowing you to display a progress indicator.
  */
void create_zc_trans_table(COLOR_MAP *table, AL_CONST PALETTE pal, int32_t r, int32_t g, int32_t b)
{
	int32_t tmp[768], *q;
	int32_t x, y, i, j, k;
	uint8_t *p;
	RGB c;
	
	for(x=0; x<256; x++)
	{
		tmp[x*3]   = pal[x].r * (255-r) / 255;
		tmp[x*3+1] = pal[x].g * (255-g) / 255;
		tmp[x*3+2] = pal[x].b * (255-b) / 255;
	}
	
	for(x=0; x<PAL_SIZE; x++)
	{
		i = pal[x].r * r / 255;
		j = pal[x].g * g / 255;
		k = pal[x].b * b / 255;
		
		p = table->data[x];
		q = tmp;
		
		if(rgb_map)
		{
			for(y=0; y<PAL_SIZE; y++)
			{
				c.r = i + *(q++);
				c.g = j + *(q++);
				c.b = k + *(q++);
				p[y] = rgb_map->data[c.r>>1][c.g>>1][c.b>>1];
			}
		}
		else
		{
			for(y=0; y<PAL_SIZE; y++)
			{
				c.r = i + *(q++);
				c.g = j + *(q++);
				c.b = k + *(q++);
				p[y] = bestfit_color(pal, c.r, c.g, c.b);
			}
		}
	}
}

/*** end of colors.cc ***/

