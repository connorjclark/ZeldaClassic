void fn()
{
	// OK.
	Screen->Pattern = 1;
	Screen->Pattern = PATTERN_STANDARD;

	{
		#option WEAK_TYPES off
		// @weak
		EnemyPattern x;
		x = 1; // Error.
		x = PATTERN_STANDARD;
	}

	{
		#option WEAK_TYPES on
		// @weak
		EnemyPattern x;
		x = 1;
		x = PATTERN_STANDARD;
	}

	Screen->Door[0] = Screen->Door[1];

	auto x = Hero->Sliding;
	// Error "Cannot cast from bitmap to int".
	x = new bitmap();
}
