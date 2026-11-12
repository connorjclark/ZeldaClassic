#ifndef BASE_EMSCRIPTEN_UTILS_H_
#define BASE_EMSCRIPTEN_UTILS_H_

#include <emscripten/emscripten.h>
#include <string>
#include <utility>
#include <vector>

void em_init_fs();
void em_sync_fs();
void em_fetch_file(std::string path);
bool em_is_lazy_file(std::string path);
std::string get_initial_file_dialog_folder();
void em_mark_initializing_status();
void em_mark_ready_status();
void em_open_link(std::string url);
// Hands a file in the browser's own files to the user to save, like a download.
void em_save_file(std::string path, std::string file_name);
bool em_is_mobile();
// Opens the player in a new tab, or in a hidden frame if it's headless. The params are its
// command line switches as URL params, without the dash in front (ex: replay-debug), where an
// empty value is a switch that takes none. A tracked player can be checked on and closed with the functions
// below; there's one at a time.
void em_open_player(const std::vector<std::pair<std::string, std::string>>& params, bool track = false);
bool em_is_tracked_player_open();
void em_close_tracked_player();
// Tells the page that the player is about to exit. Shutting down can fail in a browser, so the
// page isn't told once it's done.
void em_player_exiting();

// em_sync_fs makes the persisted copy of /local match this tab's, which loses what another tab
// has written there since this one loaded. These share single files with other tabs instead.
//
// Saves these files (and the folders they're in), waiting until it's done.
void em_persist_files(const std::vector<std::string>& paths);
// The same, without waiting. For where the program can't be suspended.
void em_persist_files_async(const std::vector<std::string>& paths);
// Deletes the saved copies of these files, which are already gone here.
void em_remove_persisted_files(const std::vector<std::string>& paths);
// Loads the files saved in this folder that are missing or older here.
void em_pull_persisted_dir(std::string dir);
int em_compile_zscript(const char* script_path, const char* console_path, const char* qr);

#endif
