// Tints the playable area with a blue overlay for an underwater effect.
generic script UnderwaterTint
{
	const int TINT_COLOR = 0x74;

	void run()
	{
		while (true)
		{
			if (Hero->Action == LA_DIVING)
			{
				// Anchor drawing to the playing field (0,0 is just below the passive subscreen).
				Screen->DrawOrigin = DRAW_ORIGIN_PLAYING_FIELD;

				// Draw an overlay using the active viewport.
				int w = Viewport->Width;
				int h = Viewport->Height;

				Screen->Rectangle(7, 0, 0, w-1, h-1, TINT_COLOR, 1, 0, 0, 0, true, OP_TRANS);
			}

			Waitframe();
		}
	}
}
