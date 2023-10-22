#ifndef _ZQ_RENDER_TREE_H_
#define _ZQ_RENDER_TREE_H_

#include "base/render.h"

RenderTreeItem* get_root_rti();
RenderTreeItem* get_screen_rti();
void zq_hide_screen(bool hidden);
void render_zq();


#endif
