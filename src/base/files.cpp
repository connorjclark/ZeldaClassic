#include "base/files.h"
#include "base/fonts.h"
#include "base/util.h"
#include "base/zsys.h"
#include "base/zc_alleg.h"
#include "allegro5/allegro_native_dialog.h"
#include "nfd.h"
#include <dispatch/dispatch.h>
#include <dispatch/queue.h>
#include <optional>

extern char temppath[4096];

static std::optional<std::string> open_dialog(std::string prompt, std::string initial_path, std::string ext, int mode)
{
	ALLEGRO_FILECHOOSER* dialog = al_create_native_file_dialog(initial_path.c_str(), prompt.c_str(), ext.c_str(), mode);
	if (!dialog)
	{
		return std::nullopt;
	}
	if (!al_show_native_file_dialog(all_get_display(), dialog))
	{
		al_destroy_native_file_dialog(dialog);
		return std::nullopt;
	}
	if (al_get_native_file_dialog_count(dialog) != 1)
	{
		al_destroy_native_file_dialog(dialog);
		return std::nullopt;
	}
	std::string path = al_get_native_file_dialog_path(dialog, 0);
	if (path.empty())
	{
		al_destroy_native_file_dialog(dialog);
		return std::nullopt;
	}

	al_destroy_native_file_dialog(dialog);
	return path;
}

static std::optional<std::string> open_dialog2(std::string prompt, std::string initial_path, std::string ext, int mode)
{
	static bool initialized;
	if (!initialized)
	{
		NFD_Init();
		atexit(NFD_Quit);
		initialized = true;
	}

	__block std::string path;
	__block bool error;
	dispatch_sync(dispatch_get_main_queue(), ^{
		nfdchar_t *outPath;
		nfdfilteritem_t filterItem[2] = { { "Source code", "c,cpp,cc" }, { "Headers", "h,hpp" } };
		nfdresult_t result = NFD_OpenDialog(&outPath, filterItem, 2, NULL);
		if (result == NFD_OKAY)
		{
			path = outPath;
			NFD_FreePath(outPath);
		}
		else if (result == NFD_ERROR)
		{
			error = true;
		}
    });

	if (path.empty())
	{
		if (error)
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

static std::string parse_ext(std::string ext, EXT_LIST *list)
{
	if (list == nullptr)
	{
		util::replchar(ext, ',', ';');
		return ext;
	}

	ext = "";
	EXT_LIST* cur = list;
	while (cur->ext != nullptr)
	{
		ext += cur->ext;
		cur++;
		if (cur->ext != nullptr)
			ext += ';';
	}

	return ext;
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
	return open_dialog2(prompt, initial_path, parse_ext(ext, list), ALLEGRO_FILECHOOSER_FILE_MUST_EXIST);
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
	return open_dialog2(prompt, initial_path, parse_ext(ext, list), ALLEGRO_FILECHOOSER_SAVE);
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
