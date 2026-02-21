// Draws a combo at a specific coordinate in a scrolling region.
generic script RegionWaypoint
{
	void run()
	{
		const int WAYPOINT_COMBO = 100;
		const int TARGET_X = 256 * 2.5; // ~2.5 screens to the right and down.
		const int TARGET_Y = 176 * 2.5;

		while (true)
		{
			// Anchor drawing to the top-left of the entire region.
			// Note: this is actually optional, and you could remove and rely on the defaults.
			Screen->DrawOrigin = DRAW_ORIGIN_REGION;

			// If the camera is near the target point, this will be drawn on screen.
			// The engine automatically handles clipping and viewport math.
			// It also handles adding appropriate offsets when scrolling between screens.
			Screen->DrawCombo(7, TARGET_X, TARGET_Y, WAYPOINT_COMBO, 1, 1, 3, -1, -1, 0, 0, 0, 0, 0, true, OP_TRANS);

			Waitframe();
		}
	}
}
