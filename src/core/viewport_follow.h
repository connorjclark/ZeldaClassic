#ifndef CORE_VIEWPORT_FOLLOW_H_
#define CORE_VIEWPORT_FOLLOW_H_

#include "base/ints.h"
#include "base/zfix.h"

// How the viewport follows its target in scrolling regions: the "follow settings". Init Data
// holds the quest-wide defaults; a dmap with dmfVIEWPORT_SETTINGS supplies its own set instead.
// Only observable in scrolling regions.
struct ViewportFollowSettings
{
	// Size of the viewport deadzone box, in pixels. While the viewport target stays within this
	// box (centered on the viewport focus), the viewport doesn't move. 0 disables (viewport locked
	// to the target).
	byte deadzone_w = 0, deadzone_h = 0;
	// How far the viewport aims ahead of (positive) or behind (negative) the direction the
	// viewport target is moving on each axis (only movement in the direction it faces counts),
	// in pixels, and how fast that offset shifts in pixels per frame (fractional; 0 to 1 feels
	// best).
	int8_t lookahead_x = 0, lookahead_y = 0;
	zfix lookahead_speed = 1;
	// Idle recentering: once the viewport target has stood still for `recenter_delay` frames,
	// the viewport eases back at `recenter_speed` pixels per frame until the target is centered
	// in the deadzone box again. 0 speed disables.
	zfix recenter_speed = 0;
	word recenter_delay = 30;
};

#endif
