// Draws a target circle relative to a spites's position.
generic script LockOnTarget
{
	void run()
	{
		while (true)
		{
			// Anchor drawing relative to a specific sprite.
			Screen->DrawOrigin = DRAW_ORIGIN_SPRITE;
			Screen->DrawOriginTarget = Hero; // Could be any sprite.

			// Draw relative to the sprite's top-left coordinate.
			// 0,0 is exactly where the Hero is located (the sprite's top-left pixel).
			int offsetX = Screen->DrawOriginTarget->TileWidth * 16 / 2;
			int offsetY = Screen->DrawOriginTarget->TileHeight * 16 / 2;

			// This will be drawn relative to the sprite, even when scrolling between screens.
			Screen->Circle(6, offsetX, offsetY, 12, 0x81, 1, 0, 0, 0, false, OP_OPAQUE);

			Waitframe();
		}
	}
}
