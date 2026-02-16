#ifndef INIT_H_
#define INIT_H_

#include "gui/tab_ctl.h"
#include "zalleg/zalleg.h"
#include <string>
#include "core/initdata.h"
#include "gamedata.h"

int32_t doInit(zinitdata *zinit, bool isZC);
void resetItems(gamedata *data, zinitdata *zinit, bool freshquest);

std::string serialize_init_data_delta(zinitdata *base, zinitdata *changed);
zinitdata *apply_init_data_delta(zinitdata *base, std::string delta, std::string& out_error);
#endif
