#ifndef _BASE_FILES_H_
#define _BASE_FILES_H_

#include "base/jwinfsel.h"
#include <optional>
#include <string>

extern char temppath[4096];

// ext is comma separated list of file types. Ex: qst,qsu
std::optional<std::string> prompt_for_existing_file(std::string prompt, std::string ext, EXT_LIST *list, std::string initial_path, bool usefilename);
std::optional<std::string> prompt_for_new_file(std::string prompt, std::string ext, EXT_LIST *list, std::string initial_path, bool usefilename);

// Same as above, but return value is a bool and the result is placed in a global `temppath`. Prefer the other methods.
bool prompt_for_existing_file_compat(std::string prompt, std::string ext, EXT_LIST *list, std::string initial_path, bool usefilename);
bool prompt_for_new_file_compat(std::string prompt, std::string ext, EXT_LIST *list, std::string initial_path, bool usefilename);

#endif
