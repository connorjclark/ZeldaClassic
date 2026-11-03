#include "std.zh"
#include "auto/test_runner.zs"

// A weapon checks the triggers of a combo it overlaps every frame. Only some
// effects (secrets, combo change, copycat, ...) or 'Once Per Weapon' make it
// skip that combo afterwards, as the combo triggers guide documents.
generic script combo_trigger_weapon_once
{
	const int CMB = 3300;
	const int COPYCAT = 200;

	// Holds a still weapon on the combo at (5,5) and returns how many times the
	// trigger fired, counted by the rupee each firing consumes.
	int holdWeapon(int frames)
	{
		Game->Counter[CR_RUPEES] = 200;
		lweapon w = Screen->CreateLWeapon(LW_SCRIPT1);
		w->X = 5 * 16;
		w->Y = 5 * 16;
		w->Step = 0;
		Waitframes(frames);
		Remove(w);
		Waitframe();
		return 200 - Game->Counter[CR_RUPEES];
	}

	void run()
	{
		Test::Init();

		combodata cd = Test::setupCombo(CMB, CT_NONE);
		cd->NumTriggers = 1;
		combotrigger trig = cd->Triggers[0];
		trig->Flags[TRIGFLAG_SCRIPT1] = true;
		trig->Counter = CR_RUPEES;
		trig->CounterAmount = 1;
		trig->Flags[TRIGFLAG_COUNTER_EAT] = true;

		Hero->X = 32;
		Hero->Y = 128;
		Screen->ComboD[Test::pos(5, 5)] = CMB;
		// A second copy of the combo, which the weapon never touches.
		Screen->ComboD[Test::pos(10, 5)] = CMB;
		Waitframe();

		// The weapon's hitbox has 4 corners on the same combo, but only triggers it once a frame.
		Test::AssertEqual(holdWeapon(10), 10, "the trigger fires once every frame");

		trig->Flags[TRIGFLAG_ONCE_PER_WEAPON] = true;
		Test::AssertEqual(holdWeapon(10), 1, "with 'Once Per Weapon', the trigger fires once per weapon");
		Test::AssertEqual(holdWeapon(10), 1, "a new weapon fires it again");
		trig->Flags[TRIGFLAG_ONCE_PER_WEAPON] = false;

		// The copy of the combo at (10,5) shares the copycat.
		trig->Copycat = COPYCAT;
		trig->Flags[TRIGFLAG_NO_COPYCAT_CAUSE] = true;
		Test::AssertEqual(holdWeapon(10), 1, "a copycat makes the weapon skip the combo afterwards");

		trig->Flags[TRIGFLAG_NO_COPYCAT_CAUSE] = false;
		Test::AssertEqual(holdWeapon(10), 2, "without 'Copycat doesn't trigger this', the other copy fires too");
		trig->Copycat = 0;

		// A weapon skips all of a combo's triggers once one of them marks it, including a
		// 'Weapon Separate Triggers' one, which only runs its weapon effects.
		cd->NumTriggers = 2;
		combotrigger separate = cd->Triggers[1];
		separate->Flags[TRIGFLAG_SCRIPT1] = true;
		separate->Flags[TRIGFLAG_SEPARATE_WEAPON_TRIGGERS] = true;
		Test::AssertEqual(holdWeapon(10), 10, "a 'Weapon Separate Triggers' trigger doesn't mark the weapon");
		separate->Flags[TRIGFLAG_ONCE_PER_WEAPON] = true;
		Test::AssertEqual(holdWeapon(10), 1, "unless it has 'Once Per Weapon'");

		Test::End();
	}
}
