// This runs just the tests defined in cpp files.
// For all other tests, see the `tests` folder.

#include "zalleg/zalleg.h"
#include "test_runner/test_runner.h"
#include <allegro5/allegro.h>

#include <cstdint>

int32_t main(int32_t argc, char* argv[])
{
	bool success = false;
	bool verbose = argc >= 2 && (strcmp(argv[1], "-verbose") == 0 || strcmp(argv[1], "-v") == 0);

	extern TestResults test_scc(bool);

	run_tests(test_scc, "test_scc", verbose);

	return success ? 1 : 0;
}
END_OF_MAIN()

// TODO: make this not needed to compile...
void zprint2(const char * const format,...)
{
	char buf[8192];
	
	va_list ap;
	va_start(ap, format);
	vsnprintf(buf, 8192, format, ap);
	va_end(ap);

	printf("%s", buf);
}
