#include "render.h"

ALLEGRO_SHADER *gray_shader = nullptr;
static bool shader_invalid = false;

void use_gray_shader();
void init_shader();

static void render_tree_layout(RenderTreeItem* rti, RenderTreeItem* rti_parent)
{
	if (!rti_parent)
	{
		rti->computed.scale = rti->transform.scale;
		rti->computed.x = rti->transform.scale * rti->transform.x;
		rti->computed.y = rti->transform.scale * rti->transform.y;
	}
	else
	{
		rti->computed.scale = rti->transform.scale * rti_parent->computed.scale;
		rti->computed.x = rti->computed.scale * rti->transform.x + rti_parent->transform.x;
		rti->computed.y = rti->computed.scale * rti->transform.y + rti_parent->transform.y;
	}
	
	for (auto rti_child : rti->children)
	{
		render_tree_layout(rti_child, rti);
	}
}

static void set_grayshader(ALLEGRO_BITMAP* bmp, bool gray)
{
	ALLEGRO_BITMAP *oldtarg = al_get_target_bitmap();
	al_set_target_bitmap(bmp);
	if(gray)
	{
		use_gray_shader(); //This succeeds but does nothing?
	}
	else al_use_shader(NULL);
	al_set_target_bitmap(oldtarg);
}

static void render_tree_draw_item(RenderTreeItem* rti)
{
	if (!rti->visible)
		return;

	rti->monochrome = true; //!TODO set this elsewhere
	if (rti->bitmap)
	{
		set_grayshader(rti->bitmap, true/*rti->tint && rti->monochrome*/);
		
		if (rti->a4_bitmap && !rti->freeze_a4_bitmap_render)
		{
			all_set_transparent_palette_index(rti->transparency_index);
			all_render_a5_bitmap(rti->a4_bitmap, rti->bitmap);
		}

		int w = al_get_bitmap_width(rti->bitmap);
		int h = al_get_bitmap_height(rti->bitmap);

		if (rti->tint)
		{
			// if(rti->monochrome)
				// use_gray_shader(); //This FAILS here, no matter what?
			al_draw_bitmap(rti->bitmap,0,0,0); //Using a basic draw just to simplify testing
			//al_draw_tinted_scaled_bitmap(rti->bitmap, *rti->tint, 0, 0, w, h, rti->computed.x, rti->computed.y, w*rti->computed.scale, h*rti->computed.scale, 0);
			// if(rti->monochrome)
				// al_use_shader(NULL);
		}
		else
		{
			al_draw_scaled_bitmap(rti->bitmap, 0, 0, w, h, rti->computed.x, rti->computed.y, w*rti->computed.scale, h*rti->computed.scale, 0);
		}
	}

	for (auto rti_child : rti->children)
	{
		render_tree_draw_item(rti_child);
	}
}

void render_tree_draw(RenderTreeItem* rti)
{
	render_tree_layout(rti, nullptr);
	render_tree_draw_item(rti);
}

void init_shader()
{
	if(shader_invalid || gray_shader) return;
	shader_invalid = true;
	gray_shader = al_create_shader(ALLEGRO_SHADER_AUTO);
	if(!gray_shader)
	{
		al_trace("Failed create shader!\n");
		return;
	}
	ALLEGRO_SHADER_PLATFORM platform = al_get_shader_platform(gray_shader);
	char *pshdr, *vshdr;
	if (platform == ALLEGRO_SHADER_HLSL)
	{
		pshdr = "monochrome.hlsl";
		vshdr = "monochrome_vert.hlsl";
	}
	else if (platform == ALLEGRO_SHADER_GLSL)
	{
		pshdr = "monochrome.glsl";
		vshdr = "monochrome_vert.glsl";
	}
	else
	{
		al_trace("Failed shader platform calculation\n");
		return;
	}
	if(!al_attach_shader_source_file(gray_shader, ALLEGRO_VERTEX_SHADER, vshdr))
	{
		al_trace("Failed to attach vertex shader file '%s'\n", vshdr);
		char const* log = al_get_shader_log(gray_shader);
		al_trace("Shader error log:\n%s\nEnd shader error log.\n", log);
		return;
	}
	if(!al_attach_shader_source_file(gray_shader, ALLEGRO_PIXEL_SHADER, pshdr))
	{
		al_trace("Failed to attach pixel shader file '%s'\n", pshdr);
		char const* log = al_get_shader_log(gray_shader);
		al_trace("Shader error log:\n%s\nEnd shader error log.\n", log);
		return;
	}
	if(!al_build_shader(gray_shader))
	{
		al_trace("Failed to build pixel shader '%s'\n", pshdr);
		char const* log = al_get_shader_log(gray_shader);
		al_trace("Shader error log:\n%s\nEnd shader error log.\n", log);
		return;
	}
	
	al_trace("Successfully built vertex shader '%s'\n", vshdr);
	al_trace("Successfully built pixel shader '%s'\n", pshdr);
	shader_invalid = false;
}

void use_gray_shader()
{
	if(shader_invalid)
		return;
	if(!gray_shader)
		init_shader();
	if(shader_invalid)
		return;
	if(al_use_shader(gray_shader))
		al_trace("Using gray shader!\n");
	else
		al_trace("Failed gray shader!\n");
}

