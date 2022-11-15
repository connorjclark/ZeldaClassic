#include "render.h"
// #include "zelda.h"

static RenderTreeItem rti_root;
static RenderTreeItem rti_screen;

static int zc_gui_mouse_x()
{
	return rti_screen.global_to_local_x(mouse_x);
}

static int zc_gui_mouse_y()
{
	return rti_screen.global_to_local_y(mouse_y);
}

static void init_render_tree()
{
	if (!rti_root.children.empty())
		return;

	if (zc_get_config("zquest", "scaling_mode", 0) == 1)
		al_set_new_bitmap_flags(ALLEGRO_NO_PRESERVE_TEXTURE | ALLEGRO_MAG_LINEAR | ALLEGRO_MIN_LINEAR);
	else
		al_set_new_bitmap_flags(ALLEGRO_NO_PRESERVE_TEXTURE);
	rti_screen.bitmap = al_create_bitmap(screen->w, screen->h);

	rti_root.children.push_back(&rti_screen);

	gui_mouse_x = zc_gui_mouse_x;
	gui_mouse_y = zc_gui_mouse_y;
}

static void configure_render_tree()
{
	int resx = al_get_display_width(all_get_display());
	int resy = al_get_display_height(all_get_display());

	rti_root.transform.x = 0;
	rti_root.transform.y = 0;
	rti_root.transform.scale = 1;
	rti_root.visible = true;

	{
		static bool scaling_force_integer = zc_get_config("zquest", "scaling_force_integer", 0) != 0;

		int w = al_get_bitmap_width(rti_screen.bitmap);
		int h = al_get_bitmap_height(rti_screen.bitmap);
		float scale = std::min((float)resx/w, (float)resy/h);
		if (scaling_force_integer)
			scale = (int) scale;
		rti_screen.transform.x = (resx - w*scale) / 2 / scale;
		rti_screen.transform.y = (resy - h*scale) / 2 / scale;
		rti_screen.transform.scale = scale;
		rti_screen.visible = true;
	}

	all_render_a5_bitmap(screen, rti_screen.bitmap);
}

void render_zq()
{
	init_render_tree();
	configure_render_tree();

	al_set_target_backbuffer(all_get_display());
	al_clear_to_color(al_map_rgb_f(0, 0, 0));
	render_tree_draw(&rti_root);

	al_flip_display();
}
