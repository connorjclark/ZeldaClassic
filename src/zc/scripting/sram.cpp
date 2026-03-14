#include "zc/scripting/sram.h"
#include "zc/ffscript.h"

extern int32_t sarg1;

void do_loadgamestructs([[maybe_unused]] const bool v, [[maybe_unused]] const bool v2)
{
	set_register(sarg1, -10000);
	Z_scripterrlog("LoadSRAM() has been removed from ZC in 3.0, as it was repeatedly broken with updates.\n");
}

void do_savegamestructs([[maybe_unused]] const bool v, [[maybe_unused]] const bool v2)
{
	set_register(sarg1, -10000);
	Z_scripterrlog("SaveSRAM() has been removed from ZC in 3.0, as it was repeatedly broken with updates.\n");
}
