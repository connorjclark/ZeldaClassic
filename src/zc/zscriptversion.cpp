#include "zc/zscriptversion.h"
#include "zc/hero.h"
#include <cstdint>

int32_t ZScriptVersion::CurrentVersion = V_FFSCRIPT; //Set to current version by default

void ZScriptVersion::ScrollingScript(int32_t scrolldir, int32_t cx, int32_t sx, int32_t sy, bool end_frames, bool waitdraw)
{
    Hero.run_scrolling_script(scrolldir, cx, sx, sy, end_frames, waitdraw);
}
