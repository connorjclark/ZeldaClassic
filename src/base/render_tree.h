#ifndef _BASE_RENDER_TREE_H_
#define _BASE_RENDER_TREE_H_

#include "zc_alleg.h"
#include <vector>

class RenderTreeItemProps
{
public:
	int x, y;
	float scale;
};

class RenderTreeItem
{
public:
	RenderTreeItemProps transform;
	RenderTreeItemProps computed;
	bool visible;
	ALLEGRO_BITMAP* bitmap;
	ALLEGRO_COLOR* tint;
	std::vector<RenderTreeItem*> children;

	int translate_mouse_x(int x)
	{
		return (x - computed.x) / computed.scale;
	}
	int translate_mouse_y(int y)
	{
		return (y - computed.y) / computed.scale;
	}
};

void render_tree_layout(RenderTreeItem* rti, RenderTreeItem* rti_parent);
void render_tree_draw(RenderTreeItem* rti);

#endif
