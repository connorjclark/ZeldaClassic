// Draws a transparent red tint over the entire game window when hit.
generic script HeroDamageFlash
{
	const int SCREEN_WIDTH = 256;
	const int SCREEN_HEIGHT = 232;
	const int FLASH_COLOR = 0x74;
	const int DURATION_IN_FRAMES = 30; // 0.5 seconds.

	void run()
	{
		while (true)
		{
			if (HeroHitThisFrame())
			{
				for (int i = 0; i < DURATION_IN_FRAMES; i++)
				{
					// Anchor drawing to the absolute screen coordinates (0,0 is top-left of the game window).
					Screen->DrawOrigin = DRAW_ORIGIN_SCREEN;
					// Draw a transparent color across the whole screen.
					Screen->Rectangle(7, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, FLASH_COLOR, 1, 0, 0, 0, true, OP_TRANS);
					Waitframe();
				}
			}

			Waitframe();
		}
	}

	bool HeroHitThisFrame()
	{
		return Hero->HitBy[HIT_BY_NPC_UID] || Hero->HitBy[HIT_BY_EWEAPON_UID] || Hero->HitBy[HIT_BY_LWEAPON_UID] || Hero->HitBy[HIT_BY_FFC_UID];
	}
}
