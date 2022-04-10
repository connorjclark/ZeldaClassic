#ifndef __emscripten_utils_h_
#define __emscripten_utils_h_

#include <string>

struct QueryParams
{
  std::string quest;
};

void init_fs_em();
void sync_fs_em();
void fetch_quest_em(const char *path);
QueryParams get_query_params();

#endif
