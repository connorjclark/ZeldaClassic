#include "render.h"
#include "zelda.h"
#include "base/gui.h"
#include <fmt/format.h>

RenderTreeItem rti_root;
RenderTreeItem rti_game;
RenderTreeItem rti_menu;
RenderTreeItem rti_gui;

static int zc_gui_mouse_x()
{
	if (rti_gui.visible)
	{
		return rti_gui.translate_mouse_x(mouse_x);
	}
	else if (rti_menu.visible)
	{
		return rti_menu.translate_mouse_x(mouse_x);
	}
	else
	{
		return rti_game.translate_mouse_x(mouse_x);
	}
}

static int zc_gui_mouse_y()
{
	if (rti_gui.visible)
	{
		return rti_gui.translate_mouse_y(mouse_y);
	}
	else if (rti_menu.visible)
	{
		return rti_menu.translate_mouse_y(mouse_y);
	}
	else
	{
		return rti_game.translate_mouse_x(mouse_y);
	}
}

static BITMAP* gui_bmp;

static void init_render_tree()
{
	if (!rti_root.children.empty())
		return;

	if (zc_get_config("zeldadx", "scaling_mode", 0) == 1)
		al_set_new_bitmap_flags(ALLEGRO_NO_PRESERVE_TEXTURE | ALLEGRO_MAG_LINEAR | ALLEGRO_MIN_LINEAR);
	else
		al_set_new_bitmap_flags(ALLEGRO_NO_PRESERVE_TEXTURE);
	rti_game.bitmap = al_create_bitmap(framebuf->w, framebuf->h);

	al_set_new_bitmap_flags(ALLEGRO_NO_PRESERVE_TEXTURE);
	rti_menu.bitmap = al_create_bitmap(menu_bmp->w, menu_bmp->h);

	gui_bmp = create_bitmap_ex(8, 640, 480);
	zc_set_gui_bmp(gui_bmp);
	al_set_new_bitmap_flags(ALLEGRO_NO_PRESERVE_TEXTURE);
	rti_gui.bitmap = al_create_bitmap(gui_bmp->w, gui_bmp->h);

	rti_root.children.push_back(&rti_game);
	rti_root.children.push_back(&rti_menu);
	rti_root.children.push_back(&rti_gui);

	gui_mouse_x = zc_gui_mouse_x;
	gui_mouse_y = zc_gui_mouse_y;
}

static void configure_render_tree()
{
	rti_root.transform.x = 0;
	rti_root.transform.y = 0;
	rti_root.transform.scale = 1;
	rti_root.visible = true;

	{
		static bool scaling_force_integer = zc_get_config("zeldadx", "scaling_force_integer", 1);
		
		int w = al_get_bitmap_width(rti_game.bitmap);
		int h = al_get_bitmap_height(rti_game.bitmap);
		float scale = std::min((float)resx/w, (float)resy/h);
		if (scaling_force_integer)
			scale = std::max((int) scale, 1);
		rti_game.transform.x = (resx - w*scale) / 2 / scale;
		rti_game.transform.y = (resy - h*scale) / 2 / scale;
		rti_game.transform.scale = scale;
		rti_game.visible = true;
	}

	{
		int w = al_get_bitmap_width(rti_menu.bitmap);
		int h = al_get_bitmap_height(rti_menu.bitmap);
		float scale = std::min((float)resx / w, (float)resy / h);
		rti_menu.transform.x = 0;
		rti_menu.transform.y = 0;
		rti_menu.transform.scale = scale;
		rti_menu.visible = MenuOpen;
	}

	{
		int w = al_get_bitmap_width(rti_gui.bitmap);
		int h = al_get_bitmap_height(rti_gui.bitmap);
		float scale = std::min((float)resx/w, (float)resy/h);
		rti_gui.transform.x = (resx - w*scale) / 2 / scale;
		rti_gui.transform.y = (resy - h*scale) / 2 / scale;
		rti_gui.transform.scale = scale;
		rti_gui.visible = (dialog_count >= 1 && !active_dialog) || dialog_count >= 2 || screen == gui_bmp;
		if (rti_gui.visible)
			rti_menu.visible = false;
	}

	bool freeze_game_bitmap = rti_menu.visible || rti_gui.visible;
	if (freeze_game_bitmap)
	{
		static ALLEGRO_COLOR tint = al_premul_rgba_f(0.4, 0.4, 0.8, 0.8);
		rti_game.tint = &tint;
	}
	else
	{
		all_render_a5_bitmap(framebuf, rti_game.bitmap);
		rti_game.tint = nullptr;
	}

	if (rti_menu.visible)
		all_render_a5_bitmap(menu_bmp, rti_menu.bitmap);
	if (rti_gui.visible)
		all_render_a5_bitmap(gui_bmp, rti_gui.bitmap);
}

static void render_debug_text(ALLEGRO_FONT* font, std::string text, int x, int y, int scale)
{
	int w = al_get_text_width(font, text.c_str());
	int h = al_get_font_line_height(font);

	al_set_new_bitmap_flags(ALLEGRO_NO_PRESERVE_TEXTURE);
	ALLEGRO_BITMAP* text_bitmap = al_create_bitmap(resx, 8);
	al_set_target_bitmap(text_bitmap);
	al_clear_to_color(al_map_rgba(0, 0, 0, 0));
	al_draw_filled_rectangle(0, 0, w, h, al_map_rgba_f(0, 0, 0, 0.6));
	al_draw_text(font, al_map_rgb_f(1,1,1), 0, 0, 0, text.c_str());

	al_set_target_backbuffer(all_get_display());
	al_draw_scaled_bitmap(text_bitmap,
		0, 0,
		al_get_bitmap_width(text_bitmap), al_get_bitmap_height(text_bitmap),
		x, y,
		al_get_bitmap_width(text_bitmap) * scale, al_get_bitmap_height(text_bitmap) * scale,
		0
	);
	al_destroy_bitmap(text_bitmap);
}

void render_zc()
{
	init_render_tree();
	configure_render_tree();

	al_set_target_backbuffer(all_get_display());
	al_clear_to_color(al_map_rgb_f(0, 0, 0));
	render_tree_draw(&rti_root);

	static ALLEGRO_FONT* font = al_create_builtin_font();
	static int font_scale = 5;
	int font_height = al_get_font_line_height(font);
	int debug_text_y = resy - font_scale*font_height - 5;
	if (Paused)
	{
		render_debug_text(font, "PAUSED", 5, debug_text_y, font_scale);
		debug_text_y -= font_scale*font_height + 3;
	}

	if (ShowFPS)
	{
		render_debug_text(font, fmt::format("fps: {}", (int)lastfps), 5, debug_text_y, font_scale);
		debug_text_y -= font_scale*font_height + 3;
	}

    al_flip_display();
}
