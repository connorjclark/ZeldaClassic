#include "base/emscripten_utils.h"
#include <emscripten/emscripten.h>
#include <emscripten/val.h>
#include <filesystem>

namespace fs = std::filesystem;

EM_ASYNC_JS(void, em_init_fs_, (), {
  // Initialize the filesystem with 0-byte files for every quest.
  const manifest = await ZC.getQuestManifest();

  function writeFakeFile(path, url) {
    url = url || ZC.dataOrigin + path;
    FS.mkdirTree(PATH.dirname(path));
    FS.writeFile(path, '');
    window.ZC.pathToUrl[path] = url;
  }

  for (const [id, quest] of Object.entries(manifest)) {
    const questPrefix = '/' + id;

    for (const music of quest.music) {
        writeFakeFile(questPrefix + '/music/' + music);
    }

    for (const release of quest.releases) {
        const releasePrefix = questPrefix + '/' + release.name;
        for (const resource of release.resources) {
			const path = releasePrefix + '/' + resource;
			let url = ZC.dataOrigin + path;
			if (resource.endsWith('.qst')) url += '.gz';
            writeFakeFile(path, url);
        }
        for (const music of quest.music) {
            writeFakeFile(releasePrefix + '/music/' + music, ZC.dataOrigin + questPrefix + '/music/' + music);
        }
    }
  }

  // Setup the "lazy" files - these are part of the normal package resources, but are not always needed
  // so are excluded from the main data file.
  for (const file of window.ZC_Constants.files) {
    FS.mkdirTree(PATH.dirname(file));
    writeFakeFile(file, '../files' + file);
  }

  await ZC.configureMount();
});
void em_init_fs() {
  em_init_fs_();
}

EM_ASYNC_JS(void, em_sync_fs_, (), {
  await ZC.fsSync(false);
});
void em_sync_fs() {
  em_sync_fs_();
}

// Many files don't have real data until we know the user needs it.
// See em_init_fs
EM_ASYNC_JS(void, em_fetch_file_, (const char *path), {
  try {
    path = UTF8ToString(path);
    if (!path.startsWith('/')) path = '/' + path;
    try {
        if (FS.stat(path).size) return;
    } catch {
    }

	let url;
	if (path.startsWith('/host')) url = path; // /host is handled by webserver.mjs, for local development only.
	else if (path.startsWith('/https:/')) url = 'https://' + path.substring('/https:/'.length - 1);
	else url = window.ZC.pathToUrl[path];
    if (!url) return;

    const data = await ZC.fetchAsByteArray(url);
	if (!data) {
		// Fetch failed (could be offline). fetchAsByteArray logged why.
		console.error(`error fetching ${path}`);
		return;
	}

	FS.mkdirTree(PATH.dirname(path));
    FS.writeFile(path, data);
  } catch (e) {
    console.error(`error fetching ${path}`, e);
  }
});
void em_fetch_file(std::string path) {
  em_fetch_file_(path.c_str());
}

// TODO: is this necessary still? Can we just always attempt to fetch a file (em_fetch_file_ handles
// when a file is already present)?
bool em_is_lazy_file(std::string path) {
  path = (fs::current_path() / path).string();

  if (strncmp("/host/", path.c_str(), strlen("/host/")) == 0) {
    return true;
  }

  if (strncmp("/https:/", path.c_str(), strlen("/https:/")) == 0) {
    return true;
  }

  if (strncmp("/quests/", path.c_str(), strlen("/quests/")) == 0) {
    return true;
  }

  if (strncmp("/tilesets/", path.c_str(), strlen("/tilesets/")) == 0) {
    return true;
  }

  return EM_ASM_INT({
    return ZC_Constants.files.includes(UTF8ToString($0));
  }, path.c_str());
}

std::string get_initial_file_dialog_folder() {
  return "/local/";
}

void em_mark_initializing_status() {
	EM_ASM({
		ZC.setStatus('Initializing Runtime ...');
	});
}

void em_mark_ready_status() {
	EM_ASM({
		ZC.setStatus('Ready');
	});
}

bool em_is_mobile() {
  return EM_ASM_INT({
    return window.matchMedia("(hover: none)").matches;
  });
}

void em_open_link(std::string url) {
  EM_ASM({
    window.open(UTF8ToString($0), '_blank');
  }, url.c_str());
}

EM_ASYNC_JS(void, em_save_file_, (const char* path, const char* file_name), {
  await ZC.saveFile(UTF8ToString(path), UTF8ToString(file_name));
});
void em_save_file(std::string path, std::string file_name) {
  em_save_file_(path.c_str(), file_name.c_str());
}

void em_open_player(const std::vector<std::pair<std::string, std::string>>& params, bool track) {
	EM_ASM({
		ZC.pendingPlayerParams = [];
	});
	for (auto& [key, value] : params) {
		EM_ASM({
			ZC.pendingPlayerParams.push([UTF8ToString($0), UTF8ToString($1)]);
		}, key.c_str(), value.c_str());
	}
	EM_ASM({
		ZC.openPlayer(ZC.pendingPlayerParams, $0);
	}, track);
}

bool em_is_tracked_player_open() {
	return EM_ASM_INT({
		return ZC.isTrackedPlayerOpen();
	});
}

void em_close_tracked_player() {
	EM_ASM({
		ZC.closeTrackedPlayer();
	});
}

void em_player_exiting() {
	EM_ASM({
		ZC.onPlayerExiting();
	});
}

// Paths are passed to JS as one string, a path to a line.
static std::string join_paths(const std::vector<std::string>& paths) {
	std::string joined;
	for (auto& path : paths)
		joined += path + '\n';
	return joined;
}

EM_ASYNC_JS(void, em_persist_files_, (const char* paths), {
  try {
    await ZC.fsPersist(UTF8ToString(paths).split('\n'));
  } catch (e) {
    console.error(e);
  }
});
void em_persist_files(const std::vector<std::string>& paths) {
	em_persist_files_(join_paths(paths).c_str());
}

void em_persist_files_async(const std::vector<std::string>& paths) {
	EM_ASM({
		ZC.fsPersist(UTF8ToString($0).split('\n')).catch(e => console.error(e));
	}, join_paths(paths).c_str());
}

EM_ASYNC_JS(void, em_remove_persisted_files_, (const char* paths), {
  try {
    await ZC.fsRemove(UTF8ToString(paths).split('\n'));
  } catch (e) {
    console.error(e);
  }
});
void em_remove_persisted_files(const std::vector<std::string>& paths) {
	em_remove_persisted_files_(join_paths(paths).c_str());
}

EM_ASYNC_JS(void, em_pull_persisted_dir_, (const char* dir), {
  try {
    await ZC.fsPull(UTF8ToString(dir));
  } catch (e) {
    console.error(e);
  }
});
void em_pull_persisted_dir(std::string dir) {
	em_pull_persisted_dir_(dir.c_str());
}

EM_ASYNC_JS(int, em_compile_zscript_, (const char* script_path, const char* console_path, const char* qr), {
  script_path = UTF8ToString(script_path);
  console_path = UTF8ToString(console_path);
  qr = UTF8ToString(qr);
  const {code} = await ZC.runZscriptCompiler(script_path, console_path, qr);
  return code;
});
int em_compile_zscript(const char* script_path, const char* console_path, const char* qr) {
  return em_compile_zscript_(script_path, console_path, qr);
}
