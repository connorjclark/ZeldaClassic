#include "zq/gui/regiongrid.h"
#include "base/qrs.h"
#include "gui/common.h"
#include "gui/dialog_runner.h"
#include "gui/size.h"
#include "gui/jwin.h"
#include "zq/zq_class.h"
#include <cassert>

using namespace GUI;

void custom_vsync();

// This is a snapshot of all the compat QRs as of Jan 26, 2024.
static std::vector<int> qrs_that_prevent_regions = {
	qr_0AFRAME_ITEMS_IGNORE_AFRAME_CHANGES,
	qr_192b163_WARP,
	qr_210_WARPRETURN,
	qr_8WAY_SHOT_SFX_DEP,
	qr_ALLOW_EDITING_COMBO_0,
	qr_ALLTRIG_PERMSEC_NO_TEMP,
	qr_ANGULAR_REFLECT_BROKEN,
	qr_ANONE_NOANIM,
	qr_ARROWCLIP,
	qr_BITMAPOFFSETFIX,
	qr_BROKEN_ASKIP_Y_FRAMES,
	qr_BROKEN_ATTRIBUTE_31_32,
	qr_BROKEN_BIG_ENEMY_ANIMATION,
	qr_BROKEN_BOMB_AMMO_COSTS,
	qr_BROKEN_DRAWSCREEN_FUNCTIONS,
	qr_BROKEN_FLAME_ARROW_REFLECTING,
	qr_BROKEN_GENERIC_PUSHBLOCK_LOCKING,
	qr_BROKEN_HORIZONTAL_WEAPON_ANIM,
	qr_BROKEN_INPUT_DOWN_STATE,
	qr_BROKEN_ITEM_CARRYING,
	qr_BROKEN_KEEPOLD_FLAG,
	qr_BROKEN_LIFTSWIM,
	qr_BROKEN_LIGHTBEAM_HITBOX,
	qr_BROKEN_MOVING_BOMBS,
	qr_BROKEN_OVERWORLD_MINIMAP,
	qr_BROKEN_RAFT_SCROLL,
	qr_BROKEN_RING_POWER,
	qr_BROKEN_SWORD_SPIN_TRIGGERS,
	qr_BROKEN_Z3_ANIMATION,
	qr_BROKENBOOKCOST,
	qr_BROKENHITBY,
	qr_BUGGED_LAYERED_FLAGS,
	qr_BUGGY_BUGGY_SLASH_TRIGGERS,
	qr_CANDLES_SHARED_LIMIT,
	qr_CHECKSCRIPTWEAPONOFFSCREENCLIP,
	qr_CONT_SWORD_TRIGGERS,
	qr_COPIED_SWIM_SPRITES,
	qr_CUSTOMWEAPON_IGNORE_COST,
	qr_DECO_2_YOFFSET,
	qr_DMAP_0_CONTINUE_BUG,
	qr_DUNGEONS_USE_CLASSIC_CHARTING,
	qr_ENEMIES_DONT_SCRIPT_FIRST_FRAME,
	qr_ENEMIES_SECRET_ONLY_16_31,
	qr_ENEMY_BROKEN_TOP_HALF_SOLIDITY,
	qr_FAIRY_FLAG_COMPAT,
	qr_FFCPRELOAD_BUGGED_LOAD,
	qr_FLUCTUATING_ENEMY_JUMP,
	qr_GANON_CANT_SPAWN_ON_CONTINUE,
	qr_GANONINTRO,
	qr_GOHMA_UNDAMAGED_BUG,
	qr_GOTOLESSNOTEQUAL,
	qr_HARDCODED_BS_PATRA,
	qr_HARDCODED_ENEMY_ANIMS,
	qr_HARDCODED_FFC_BUSH_DROPS,
	qr_HARDCODED_LITEM_LTMS,
	qr_HOOKSHOTDOWNBUG,
	qr_ITEMPICKUPSETSBELOW,
	qr_LEEVERS_DONT_OBEY_STUN,
	qr_MANHANDLA_BLOCK_SFX,
	qr_MOVINGBLOCK_FAKE_SOLID,
	qr_NO_LANMOLA_RINGLEADER,
	qr_NO_LIFT_SPRITE,
	qr_NO_OVERWORLD_MAP_CHARTING,
	qr_NOFAIRYGUYFIRES,
	qr_NOSOLIDDAMAGECOMBOS,
	qr_OFFSCREENWEAPONS,
	qr_OLD_210_WATER,
	qr_OLD_BOMB_HITBOXES,
	qr_OLD_BRIDGE_COMBOS,
	qr_OLD_BUG_NET,
	qr_OLD_CHEST_COLLISION,
	qr_OLD_DMAP_INTRO_STRINGS,
	qr_OLD_DOORREPAIR,
	qr_OLD_DRAWOFFSET,
	qr_OLD_ENEMY_KNOCKBACK_COLLISION,
	qr_OLD_F6,
	qr_OLD_FAIRY_LIMIT,
	qr_OLD_FFC_FUNCTIONALITY,
	qr_OLD_FFC_SPEED_CAP,
	qr_OLD_FLAMETRAIL_DURATION,
	qr_OLD_GUY_HANDLING,
	qr_OLD_HALF_MAGIC,
	qr_OLD_ITEMDATA_SCRIPT_TIMING,
	qr_OLD_KEESE_Z_AXIS,
	qr_OLD_LADDER_ITEM_SIDEVIEW,
	qr_OLD_LENS_LAYEREFFECT,
	qr_OLD_LOCKBLOCK_COLLISION,
	qr_OLD_POTION_OR_HC,
	qr_OLD_SCRIPT_VOLUME,
	qr_OLD_SCRIPTED_KNOCKBACK,
	qr_OLD_SECRETMONEY,
	qr_OLD_SHALLOW_SFX,
	qr_OLD_SIDEVIEW_CEILING_COLLISON,
	qr_OLD_SIDEVIEW_LANDING_CODE,
	qr_OLD_SLASHNEXT_SECRETS,
	// TODO QRHINT doesnt know about needing to do what onStrFix does
	// qr_OLD_STRING_EDITOR_MARGINS,
	qr_OLD_TILE_INITIALIZATION,
	qr_OLD_WIZZROBE_SUBMERGING,
	qr_OLDCS2,
	qr_OLDHOOKSHOTGRAB,
	qr_OLDINFMAGIC,
	qr_OLDLENSORDER,
	qr_OLDSIDEVIEWSPIKES,
	qr_OLDSPRITEDRAWS,
	qr_PATRAS_USE_HARDCODED_OFFSETS,
	qr_PEAHATCLOCKVULN,
	qr_POLVIRE_NO_SHADOW,
	qr_REPLACEOPENDOORS,
	qr_SCREENSTATE_80s_BUG,
	qr_SCRIPT_FRIENDLY_ENEMY_TYPES,
	qr_SCROLLING_KILLS_CHARGE,
	qr_SCROLLWARP_NO_RESET_FRAME,
	qr_SHORTDGNWALK,
	qr_SPARKLES_INHERIT_PROPERTIES,
	qr_SPOTLIGHT_IGNR_SOLIDOBJ,
	qr_SPRITE_JUMP_IS_TRUNCATED,
	qr_STEPTEMP_SECRET_ONLY_16_31,
	// TODO QRHINT doesnt know about needing to do what onStrFix does
	// qr_STRING_FRAME_OLD_WIDTH_HEIGHT,
	qr_SUBSCR_BACKWARDS_ID_ORDER,
	qr_SUBSCR_OLD_SELECTOR,
	qr_TRIGGERSREPEAT,
	qr_WALKTHROUGHWALL_NO_DOORSTATE,
	qr_WARPS_RESTART_DMAPSCRIPT,
	qr_WIZZROBES_DONT_OBEY_STUN,
	qr_WRONG_BRANG_TRAIL_DIR,
};

// To reduce the amount of old features that need to be upgraded for region support, we draw a line in the sand and require
// that all these compat rules must be disabled for regions to be enabled.
static bool should_allow_regions()
{
	for (auto qr : qrs_that_prevent_regions)
	{
		if (get_bit(quest_rules, qr))
		{
			return false;
		}
	}

	return true;
}

static int rg_current_region_id = 0;
static int rg_frame_thickness = 5;
static int rg_button_thickness = 2;
static int rg_header_width = 6;
static int rg_header_height = 9;
static int rg_cols = 16;
static int rg_col_width = 27;
static int rg_l = 25;

static void draw_region_square(BITMAP* bmp, int frame, int region_id, int x, int y, FONT* f, int text_height)
{
	int color = vc(region_id);
	jwin_draw_frame(bmp, x, y, rg_col_width, rg_l, frame);

	int x0 = x + rg_button_thickness;
	int y0 = y + rg_button_thickness;
	rectfill(bmp, x0, y0,
		x + rg_col_width - rg_button_thickness - 1, y + rg_l - rg_button_thickness - 1, color);

	// Ideally would just use `getHighlightColor(color)` but the method isn't good enough yet.
	int text_color;
	switch (region_id) {
	case 2:
	case 3:
	case 5:
	case 7:
	case 9:
		// getHighlightColor currently looks awfule for these colors, so just use black.
		text_color = vc(0);
		break;
	default:
		text_color = getHighlightColor(color);
	}
	textprintf_centre_ex(bmp, f, x0 + rg_col_width / 2 - rg_button_thickness, y0 + rg_l / 2 - text_height / 2, text_color, -1, "%d", region_id);
}

int32_t d_region_grid_proc(int32_t msg, DIALOG* d, int32_t c)
{
	if (msg == MSG_DRAW && !should_allow_regions())
	{
		// InfoDialog("Regions are disabled because of QRs", "You must disable all of the following compat QRs to use regions." + QRHINT(qrs_that_prevent_regions)).show();
		// return D_O_K;
		// TODO z3 !
	}

	RegionGrid* widg = (RegionGrid*)d->dp2;

	FONT* nf = get_zc_font(font_nfont);

	int map = Map.getCurrMap();
	regions_data* local_regions_data = widg->getLocalRegionsData();

	switch (msg)
	{
	case MSG_START:
	case MSG_WANTFOCUS:
		return D_WANTFOCUS;

	case MSG_DRAW:
	{
		BITMAP* tempbmp = create_bitmap_ex(8, SCREEN_W, SCREEN_H);
		clear_bitmap(tempbmp);
		int32_t x = d->x;
		int32_t y = d->y;
		int32_t j = 0, k = 0;
		rectfill(tempbmp, x, y, x + d->w - 18, y + rg_header_height - 1, jwin_pal[jcBOX]);

		for (j = 0; j < 8; ++j)
		{
			textprintf_ex(tempbmp, nf, x, y + rg_header_height + rg_frame_thickness + 1 + (j * rg_l), jwin_pal[jcBOXFG], jwin_pal[jcBOX], "%d", j);
		}

		for (j = 0; j < rg_cols; ++j)
		{
			textprintf_ex(tempbmp, nf, x + rg_header_width + rg_frame_thickness + ((rg_col_width + 1) / 2) - (rg_header_width / 2) + (j * rg_col_width), y, jwin_pal[jcBOXFG], jwin_pal[jcBOX], "%X", j);
		}

		// why this not look good
		// jwin_draw_frame(tempbmp, x+header_width+is_large, y+rg_header_height+is_large, (is_large?180:116)*2, (is_large?84:60)*2, FR_DEEP);

		int txtheight = text_height(nf);
		for (j = 0; j < 8; ++j)
		{
			for (k = 0; k < rg_cols; ++k)
			{
				int screen = j * 16 + k;
				if (!Map.isValid(map, screen))
					continue;

				byte region_id = local_regions_data->get_region_id(k, j);

				int frame = Map.getCurrScr() == screen ? FR_GREEN : FR_MEDDARK;
				int x2 = x + rg_header_width + (k * rg_col_width) + rg_frame_thickness;
				int y2 = y + rg_header_height + (j * rg_l) + rg_frame_thickness;
				draw_region_square(tempbmp, frame, region_id, x2, y2, nf, txtheight);
			}
		}

		masked_blit(tempbmp, screen, 0, 0, 0, 0, SCREEN_W, SCREEN_H);
		destroy_bitmap(tempbmp);
	}
	break;

	case MSG_CHAR:
	{
		int32_t k = c >> 8;

		int num = -1;
		if ((k >= KEY_0_PAD) && (k <= KEY_9_PAD)) {
			num = k - KEY_0_PAD;
		}
		else if ((k >= KEY_0) && (k <= KEY_9)) {
			num = k - KEY_0;
		}

		if (num != -1)
		{
			rg_current_region_id = num;
			d->flags |= D_DIRTY;
			GUI_EVENT(d, geCHANGE_VALUE);
			return D_USED_CHAR;
		}
	}
	break;

	case MSG_LPRESS:
	{
		int32_t xx = -1;
		int32_t yy = -1;

		while (gui_mouse_b())  // Drag across to select multiple
		{
			int32_t x = (gui_mouse_x() - (d->x) - rg_frame_thickness - rg_header_width) / rg_col_width;
			int32_t y = (gui_mouse_y() - (d->y) - rg_frame_thickness - rg_header_height) / rg_l;

			if (xx != x || yy != y)
			{
				xx = x;
				yy = y;

				int screen = y * 16 + x;
				if (y >= 0 && y < 8 && x >= 0 && x < rg_cols && Map.isValid(map, screen))
					local_regions_data->set_region_id(screen, rg_current_region_id);
			}

			scare_mouse();
			object_message(d, MSG_DRAW, 0);
			unscare_mouse();
			rest(16);
			custom_vsync();
		}
	}
	break;
	}

	return D_O_K;
}

namespace GUI
{
	RegionGrid::RegionGrid() : 
		message(-1), localRegionsData(nullptr)
		
	{
		setPreferredWidth(465_px);
		setPreferredHeight(218_px);
	}

	void RegionGrid::setCurrentRegionIndex(int newindex)
	{
		rg_current_region_id = newindex;
	}
	int RegionGrid::getCurrentRegionIndex()
	{
		return rg_current_region_id;
	}

	void RegionGrid::applyVisibility(bool visible)
	{
		Widget::applyVisibility(visible);
		if (alDialog) alDialog.applyVisibility(visible);
	}

	void RegionGrid::applyDisabled(bool dis)
	{
		Widget::applyDisabled(dis);
		if (alDialog) alDialog.applyDisabled(dis);
	}

	void RegionGrid::realize(DialogRunner& runner)
	{
		Widget::realize(runner);
		alDialog = runner.push(shared_from_this(), DIALOG{
			newGUIProc<d_region_grid_proc>,
			x, y, getWidth(), getHeight(),
			fgColor, bgColor,
			0,
			getFlags(),
			0, 0, // d1, d2,
			NULL, this, NULL // dp, dp2, dp3
			});
	}

	int32_t RegionGrid::onEvent(int32_t event, MessageDispatcher& sendMessage)
	{
		assert(event == geCHANGE_VALUE);
		int ret = -1;
		if (onUpdateFunc)
		{
			onUpdateFunc();
			pendDraw();
		}
		return ret;
	}
}