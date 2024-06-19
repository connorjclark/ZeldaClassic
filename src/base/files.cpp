#include "base/files.h"
#include "base/fonts.h"
#include "base/zsys.h"
#include "nfd.h"
#include <string>
#include <cstddef>
#include <optional>
#include <fmt/format.h>

#ifdef __APPLE__
#include <dispatch/dispatch.h>
#include <dispatch/queue.h>
#endif

char temppath[4096];

static bool init_dialog()
{
	static bool initialized, tried;
	if (!initialized)
	{
		if (tried)
			return false;

		tried = true;
		auto result = NFD_Init();
		if (result != NFD_OKAY)
		{
			Z_error("Error: %s", NFD_GetError());
			return false;
		}
		atexit(NFD_Quit);
		initialized = true;
	}

	return true;
}

// In allegro 4 `parse_extension_string` allows , ; and space, despite only documenting that ; is supported.
// Convert to `,` which is what NFD expects.
static void normalize_extension_string(std::string& str)
{
	util::replchar(str, ';', ',');
	util::replchar(str, ' ', ',');
}

struct filteritem_t
{
	std::string text, ext;
};

static std::vector<filteritem_t> create_filter_list(std::string ext, EXT_LIST *list)
{
	if (list == nullptr)
	{
		normalize_extension_string(ext);
		return {{"", ext}};
	}

	std::vector<filteritem_t> result;
	for (EXT_LIST* cur = list; cur->ext != nullptr; cur++)
	{
		std::string ext = cur->ext;
		normalize_extension_string(ext);
		result.push_back({cur->text, ext});
	}
	return result;
}

enum class FileMode
{
	Save,
	Open,
	Folder,
};

static std::optional<std::string> open_native_dialog_impl(FileMode mode, std::string initial_path, std::vector<nfdfilteritem_t> filters)
{
	const char* initial_path_ = initial_path.empty() ? nullptr : initial_path.c_str();
	nfdchar_t *outPath;
	nfdresult_t result;

	if (mode == FileMode::Folder)
		result = NFD_PickFolder(&outPath, initial_path_);
	else if (mode == FileMode::Save)
		result = NFD_SaveDialog(&outPath, filters.data(), filters.size(), initial_path_, nullptr);
	else
		result = NFD_OpenDialog(&outPath, filters.data(), filters.size(), initial_path_);

	if (result == NFD_OKAY)
	{
		std::string path = outPath;
		NFD_FreePath(outPath);
		return path;
	}

	return std::nullopt;
}

static std::optional<std::string> open_native_dialog(FileMode mode, std::string initial_path, std::vector<filteritem_t>& filters)
{
	NFD_ClearError();
	if (!init_dialog())
		return std::nullopt;

	std::vector<nfdfilteritem_t> filters_nfd;
	for (auto& filter : filters)
		filters_nfd.push_back({filter.text.c_str(), filter.ext.c_str()});

#ifdef __APPLE__
	__block std::string path;
	dispatch_sync(dispatch_get_main_queue(), ^{
		if (auto r = open_native_dialog_impl(mode, initial_path, filters_nfd))
			path = *r;
	});
#else
	std::string path;
	if (auto r = open_native_dialog_impl(mode, initial_path, filters_nfd))
		path = *r;
#endif

	if (path.empty())
	{
		if (NFD_GetError())
			Z_error("Error: %s", NFD_GetError());
		return std::nullopt;
	}

	return path;
}

static void trim_filename(std::string& path)
{
	if (path.empty())
		return;

	size_t i = path.size();
	while (i >= 0 && path[i] != '\\' && path[i] != '/')
		path[i--] = 0;
}

static bool USE_OLD = true;

static bool getname_nogo(std::string prompt, std::string ext, EXT_LIST *list, std::string initial_path, bool usefilename)
{
    int ret = 0;
    int sel = 0;

    if (list == NULL)
    {
        ret = jwin_file_select_ex(prompt.c_str(), temppath, ext.c_str(), 2048, -1, -1, get_zc_font(font_lfont));
    }
    else
    {
        ret = jwin_file_browse_ex(prompt.c_str(), temppath, list, &sel, 2048, -1, -1, get_zc_font(font_lfont));
    }

    return ret != 0;
}

static bool getname(std::string prompt, std::string ext, EXT_LIST *list, std::string initial_path, bool usefilename)
{
	extern BITMAP *tmp_scr;
    blit(screen,tmp_scr,0,0,0,0,screen->w,screen->h);
    int32_t ret=0;
    ret = getname_nogo(prompt,ext,list,initial_path,usefilename);
    blit(tmp_scr,screen,0,0,0,0,screen->w,screen->h);
    return ret != 0;
}

std::optional<std::string> prompt_for_existing_file(std::string prompt, std::string ext, EXT_LIST *list, std::string initial_path, bool usefilename)
{
	if (!usefilename)
		trim_filename(initial_path);

	if (USE_OLD)
	{
		if (!getname(prompt, ext, list, initial_path, usefilename))
			return std::nullopt;
		return temppath;
	}

	auto filters = create_filter_list(ext, list);
	return open_native_dialog(FileMode::Open, initial_path, filters);
}

std::optional<std::string> prompt_for_existing_folder(std::string prompt, std::string initial_path, std::string ext)
{
	if (USE_OLD)
	{
		extern BITMAP *tmp_scr;
		blit(screen,tmp_scr,0,0,0,0,screen->w,screen->h);

		char path[2048];
		strcpy(path, initial_path.c_str());
		if (!jwin_dfile_select_ex(prompt.c_str(), path, ext.c_str(), 2048, -1, -1, get_zc_font(font_lfont)))
		{
			blit(tmp_scr,screen,0,0,0,0,screen->w,screen->h);
			return std::nullopt;
		}

		blit(tmp_scr,screen,0,0,0,0,screen->w,screen->h);
		return path;
	}

	std::vector<filteritem_t> filters;
	return open_native_dialog(FileMode::Folder, initial_path, filters);
}

std::optional<std::string> prompt_for_new_file(std::string prompt, std::string ext, EXT_LIST *list, std::string initial_path, bool usefilename)
{
	if (!usefilename)
		trim_filename(initial_path);

	if (USE_OLD)
	{
		if (!getname(prompt, ext, list, initial_path, usefilename))
			return std::nullopt;
		return temppath;
	}

	auto filters = create_filter_list(ext, list);
	return open_native_dialog(FileMode::Save, initial_path, filters);
}

bool prompt_for_existing_file_compat(std::string prompt, std::string ext, EXT_LIST *list, std::string initial_path, bool usefilename)
{
	if (auto result = prompt_for_existing_file(prompt, ext, list, initial_path, usefilename))
	{
		strcpy(temppath, result->c_str());
		return true;
	}

	return false;
}

bool prompt_for_new_file_compat(std::string prompt, std::string ext, EXT_LIST *list, std::string initial_path, bool usefilename)
{
	if (auto result = prompt_for_new_file(prompt, ext, list, initial_path, usefilename))
	{
		strcpy(temppath, result->c_str());
		return true;
	}

	return false;
}
