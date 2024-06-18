#include "base/files.h"
#include "base/fonts.h"
#include "base/zsys.h"
#include "nfd.h"
#include <cstddef>
#include <dispatch/dispatch.h>
#include <dispatch/queue.h>
#include <optional>

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

static std::vector<nfdfilteritem_t> create_filter_list(std::string ext, EXT_LIST *list)
{
	if (list == nullptr)
		return {{"", ext.c_str()}};

	std::vector<nfdfilteritem_t> result;
	for (EXT_LIST* cur = list; cur->ext != nullptr; cur++)
		result.push_back({cur->text, cur->ext});
	return result;
}

enum class FileMode
{
	Save,
	Open,
};

static std::optional<std::string> open_native_dialog_impl(std::string initial_path, std::vector<nfdfilteritem_t> filters, FileMode mode)
{
	const char* initial_path_ = initial_path.empty() ? nullptr : initial_path.c_str();
	nfdchar_t *outPath;
	nfdresult_t result = mode == FileMode::Save ?
		NFD_SaveDialog(&outPath, filters.data(), filters.size(), initial_path_, nullptr) :
		NFD_OpenDialog(&outPath, filters.data(), filters.size(), initial_path_);
	if (result == NFD_OKAY)
	{
		std::string path = outPath;
		NFD_FreePath(outPath);
		return path;
	}

	return std::nullopt;
}

static std::optional<std::string> open_native_dialog(std::string initial_path, std::vector<nfdfilteritem_t> filters, FileMode mode)
{
	NFD_ClearError();
	if (!init_dialog())
		return std::nullopt;

#ifdef __APPLE__
	__block std::string path;
	dispatch_sync(dispatch_get_main_queue(), ^{
		if (auto r = open_native_dialog_impl(initial_path, filters, mode))
			path = *r;
    });
#else
	if (auto r = open_native_dialog_impl(initial_path, filters, mode))
		path = *r;
#endif

	if (path.empty())
	{
		if (strlen(NFD_GetError()))
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

static bool USE_OLD = false;

static bool getname_nogo(std::string prompt, std::string ext, EXT_LIST *list, std::string initial_path, bool usefilename)
{
    int ret = 0;
    int sel = 0;

    if (list==NULL)
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
	return open_native_dialog(initial_path, create_filter_list(ext, list), FileMode::Open);
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
	return open_native_dialog(initial_path, create_filter_list(ext, list), FileMode::Save);
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
