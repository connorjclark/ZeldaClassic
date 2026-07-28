// https://www.youtube.com/watch?v=JLaQevEaMUE

#include "std.zh"
#include "auto/test_runner.zs"

void fillScreen(mapdata scr)
{
	for (int x = 0; x < 16; x++)
	{
		for (int y = 0; y < 11; y++)
		{
			scr->ComboD[x + y*16] = 12 + (x + y) % 8;
		}
	}
}

void walk(int button, int frames)
{
	for (int i = 0; i < frames; i++)
	{
		press(button);
		Waitframe();
	}
}

lweapon script seeking_arrow
{
	void run()
	{
		Viewport->Target = this;

		Input->DisableButton[CB_UP] = true;
		Input->DisableButton[CB_DOWN] = true;
		Input->DisableButton[CB_LEFT] = true;
		Input->DisableButton[CB_RIGHT] = true;

		int rotation = 0;
		int time = 0;
		bool explode = false;

		while (this->DeadState != WDS_DEAD)
		{
			if (time % 60 == 0)
				Audio->PlaySound(SFX_WHIRLWIND);

			if (this->Vx)
			{
				if (Input->Button[CB_UP])
				{
					this->Y -= 1;
					rotation -= 1;
				}
				if (Input->Button[CB_DOWN])
				{
					this->Y += 1;
					rotation += 1;
				}
			}
			else
			{
				if (Input->Button[CB_LEFT])
				{
					this->X -= 1;
					rotation += 1;
				}
				if (Input->Button[CB_RIGHT])
				{
					this->X += 1;
					rotation -= 1;
				}
			}

			if (Input->Button[CB_A])
			{
				explode = true;
				break;
			}

			this->Rotation = rotation + 10*Cos(time++ * 15);

			Waitframe();
		}

		if (explode)
		{
			lweapon blast = Screen->CreateLWeapon(LW_BOMBBLAST);
			blast->X = this->X;
			blast->Y = this->Y;

			Audio->PlaySound(SFX_BOMB);

			this->DrawStyle = DS_PHANTOM;
			this->Vx = 0;
			this->Vy = 0;

			Waitframes(30);
		}

		Input->DisableButton[CB_UP] = false;
		Input->DisableButton[CB_DOWN] = false;
		Input->DisableButton[CB_LEFT] = false;
		Input->DisableButton[CB_RIGHT] = false;

		this->Remove();
	}
}

void press(int button)
{
	WaitTo(SCR_TIMING_POST_PLAYER_ACTIVE);
	Input->Button[button] = true;
}

generic script viewport
{
	void run()
	{
		Test::Init();

		int screen = 0;
		int map = Game->LoadDMapData(Test::TestingDmap)->Map;
		for (int x = 0; x < 4; x++)
		{
			for (int y = 0; y < 4; y++)
			{
				mapdata scr = Game->LoadMapData(map, screen + x + y*16);
				fillScreen(scr);
			}
		}
		Test::loadRegion(screen, 4);
		Player->Warp(Test::TestingDmap, 18);
		Waitframe();

		Viewport->Mode = VIEW_MODE_SCRIPT;

		int r = 20;
        int angle = 0;
        int da = 5;

		for (int i = 0; i < 60 * 3; i++)
		{
			Viewport->X = Hero->X - Viewport->Width / 2 + r * Cos(angle);
            Viewport->Y = Hero->Y - Viewport->Height / 2 + r * Sin(angle);

            angle += da;
            if (angle < -360) angle += 360;
            else if (angle > 360) angle -= 360;

			Waitframe();
		}

		Viewport->Mode = VIEW_MODE_CENTER_AND_BOUND;

		Hero->Item[Game->LoadItemData(I_BOW1)->ID] = true;
		Hero->Item[Game->LoadItemData(I_ARROW1)->ID] = true;
		Game->Counter[CR_ARROWS] = 10;
		Game->MCounter[CR_ARROWS] = 10;
		Game->LoadItemData(I_ARROW1)->WeaponData->Script = Game->GetLWeaponScript("seeking_arrow");

		Waitframes(30);
		press(CB_B);
		Waitframes(30);
		press(CB_A);
		Waitframes(60);

		Hero->Dir = DIR_RIGHT;
		Waitframes(30);
		press(CB_B);
		Waitframes(60);
		press(CB_A);
		Waitframes(60);

		Hero->Dir = DIR_UP;
		Waitframes(30);
		press(CB_B);
		Waitframes(30);
		press(CB_A);
		Waitframes(60);

		// The seeking_arrow script disabled the directional buttons; restore them so the
		// hero can walk.
		Input->DisableButton[CB_UP] = false;
		Input->DisableButton[CB_DOWN] = false;
		Input->DisableButton[CB_LEFT] = false;
		Input->DisableButton[CB_RIGHT] = false;

		// Deadzone: while the hero stays within the box, the camera must not move; once past
		// the box's edge, the hero stays pinned to it.
		Test::AssertEqual(Viewport->DeadzoneWidth, 0);
		Viewport->DeadzoneWidth = 64;
		Viewport->DeadzoneHeight = 48;
		Waitframe();

		int start_x = Viewport->X;
		int start_y = Viewport->Y;
		int hero_start_x = Hero->X;

		// A short walk (well under half the box width) must not move the camera.
		for (int i = 0; i < 12; i++)
		{
			press(CB_RIGHT);
			Waitframe();
		}
		Test::AssertEqual(Hero->X > hero_start_x, true, "hero didn't walk");
		Test::AssertEqual(Viewport->X, start_x, "camera moved inside deadzone");
		Test::AssertEqual(Viewport->Y, start_y, "camera moved inside deadzone");

		// A long walk pushes the hero to the box's edge: their center stays exactly
		// half the box width ahead of the camera's focus.
		for (int i = 0; i < 60; i++)
		{
			press(CB_RIGHT);
			Waitframe();
		}
		int focus_x = Viewport->X + Viewport->Width / 2;
		Test::AssertEqual(Hero->X + 8 - focus_x, Viewport->DeadzoneWidth / 2, "hero not pinned to deadzone edge");

		// Walking back the same distance re-crosses the box before the camera moves again.
		int came_from_x = Viewport->X;
		for (int i = 0; i < 12; i++)
		{
			press(CB_LEFT);
			Waitframe();
		}
		Test::AssertEqual(Viewport->X, came_from_x, "camera should idle while re-crossing the deadzone");

		Viewport->DeadzoneWidth = 0;
		Viewport->DeadzoneHeight = 0;
		Waitframe();

		// Lookahead: while the target moves, the camera gradually aims ahead of (positive)
		// or behind (negative) its direction of travel; while it stands still, the offset
		// holds.
		Test::AssertEqual(Viewport->Lookahead, 0);
		Test::AssertEqual(Viewport->LookaheadSpeed, 1);

		// Stand in the middle of the region so the ±40px aims stay clear of the region
		// clamp on both axes.
		Hero->X = 600;
		Hero->Y = 350;
		Viewport->Lookahead = 40;
		Viewport->LookaheadSpeed = 2;
		Waitframe();

		// Walking right builds a lead in front; it settles after 40/2 = 20 frames of movement.
		walk(CB_RIGHT, 30);
		Test::AssertEqual(Viewport->X + Viewport->Width / 2, Floor(Hero->X) + 8 + 40, "camera should lead ahead of movement");

		// Standing still (even turning in place) holds the offset.
		Hero->Dir = DIR_LEFT;
		Waitframes(20);
		Test::AssertEqual(Viewport->X + Viewport->Width / 2, Floor(Hero->X) + 8 + 40, "lookahead should hold while idle");

		// Walking the other way shifts the lead gradually, not instantly.
		walk(CB_LEFT, 10);
		int focus = Viewport->X + Viewport->Width / 2;
		Test::AssertEqual(focus > Floor(Hero->X) + 8 - 40 && focus < Floor(Hero->X) + 8 + 40, true, "lookahead should shift gradually");
		walk(CB_LEFT, 30);
		Test::AssertEqual(Viewport->X + Viewport->Width / 2, Floor(Hero->X) + 8 - 40, "camera should lead ahead of movement");

		// Negative lookahead trails behind the direction of travel instead.
		Viewport->Lookahead = -40;
		walk(CB_LEFT, 40);
		Test::AssertEqual(Viewport->X + Viewport->Width / 2, Floor(Hero->X) + 8 + 40, "camera should trail behind movement");

		// The vertical axis works the same way, and the horizontal offset eases out.
		Viewport->Lookahead = 40;
		walk(CB_UP, 40);
		Test::AssertEqual(Viewport->X + Viewport->Width / 2, Floor(Hero->X) + 8, "horizontal offset should ease out when moving up");
		Test::AssertEqual(Viewport->Y + Viewport->Height / 2, Floor(Hero->Y) + 8 - 40, "camera should lead vertically");

		// Disabling lookahead eases the held offset away even while idle.
		Viewport->Lookahead = 0;
		Waitframes(30);
		Test::AssertEqual(Viewport->Y + Viewport->Height / 2, Floor(Hero->Y) + 8, "lookahead should ease out when disabled");

		Test::End();
	}
}
