//--------------------------------------------------------
//  Zelda Classic
//  by Jeremy Craner, 1999-2000
//
//  zc_sys.cc
//
//  System functions, input handlers, GUI stuff, etc.
//  for Zelda Classic.
//
//--------------------------------------------------------

// to prevent <map> from generating errors
#define __GTHREAD_HIDE_WIN32API 1

#include "precompiled.h" //always first
#include "zc_sys.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <map>
#include <filesystem>
#include <ctype.h>
#include <sstream>
#include "base/zc_alleg.h"
#include "gamedata.h"
#include "zc_init.h"
//#include "zquest.h"
#include "init.h"
#include "replay.h"
#include "cheats.h"
#include "base/zc_math.h"

#ifdef ALLEGRO_DOS
#include <unistd.h>
#endif

#include "metadata/metadata.h"
#include "zelda.h"
#include "tiles.h"
#include "base/colors.h"
#include "pal.h"
#include "base/zsys.h"
#include "qst.h"
#include "zc_sys.h"
#include "debug.h"
#include "jwin.h"
#include "base/jwinfsel.h"
#include "base/gui.h"
#include "midi.h"
#include "subscr.h"
#include "maps.h"
#include "sprite.h"
#include "guys.h"
#include "hero.h"
#include "title.h"
#include "particles.h"
#include "zconsole.h"
#include "ffscript.h"
#include "dialog/info.h"
#include "dialog/alert.h"
#include <fmt/format.h>

#define XXH_STATIC_LINKING_ONLY
#define XXH_IMPLEMENTATION
#include <xxhash.h>

extern FFScript FFCore;
extern bool Playing;
int32_t sfx_voice[WAV_COUNT];
int32_t d_stringloader(int32_t msg,DIALOG *d,int32_t c);
int32_t d_midilist_proc(int32_t msg,DIALOG *d,int32_t c);

extern byte monochrome_console;

extern FONT *lfont;
extern HeroClass Hero;
extern FFScript FFCore;
extern ZModule zcm;
extern zcmodule moduledata;
extern sprite_list  guys, items, Ewpns, Lwpns, Sitems, chainlinks, decorations;
extern particle_list particles;
extern int32_t loadlast;
extern word passive_subscreen_doscript;
extern bool passive_subscreen_waitdraw;
byte disable_direct_updating;
byte use_dwm_flush;
byte use_save_indicator;
byte midi_patch_fix;
bool midi_paused=false;
int32_t paused_midi_pos = 0;
byte midi_suspended = 0;
byte callback_switchin = 0;
byte zc_192b163_warp_compatibility;
char modulepath[2048];
byte epilepsyFlashReduction;
signed char pause_in_background_menu_init = 0;
byte pause_in_background = 0;
bool is_sys_pal = false;
extern PALETTE* hw_palette;
extern bool update_hw_pal;
extern const char* dmaplist(int32_t index, int32_t* list_size);


extern bool kb_typing_mode; //script only, for disbaling key presses affecting Hero, etc. 
extern int32_t cheat_modifier_keys[4]; //two options each, default either control and either shift
//extern byte refresh_select_screen;
//extern movingblock mblock2; //mblock[4]?
//extern int32_t db;

static const char *ZC_str = "Zelda Classic";
extern char save_file_name[1024];
#ifdef ALLEGRO_DOS
const char *qst_dir_name = "dos_qst_dir";
#elif defined(ALLEGRO_WINDOWS)
const char *qst_dir_name = "win_qst_dir";
static  const char *qst_module_name = "current_module";
#elif defined(ALLEGRO_LINUX)
const char *qst_dir_name = "linux_qst_dir";
static  const char *qst_module_name = "current_module";
#elif defined(__APPLE__)
const char *qst_dir_name = "osx_qst_dir";
static  const char *qst_module_name = "current_module";
#endif
#ifdef ALLEGRO_LINUX
static  const char *samplepath = "samplesoundset/patches.dat";
#endif
char qst_files_path[2048];

#ifdef _MSC_VER
#define getcwd _getcwd
#endif

bool rF12();
bool rF5();
bool rF11();
bool rI();
bool rQ();
bool zc_key_pressed();

#ifdef _WIN32

// This should only be necessary for MinGW, since it doesn't have a dwmapi.h. Add another #ifdef if you like.
extern "C"
{
	typedef HRESULT(WINAPI *t_DwmFlush)();
	typedef HRESULT(WINAPI *t_DwmIsCompositionEnabled)(BOOL *pfEnabled);
}

void do_DwmFlush()
{
	static HMODULE shell = LoadLibrary("dwmapi.dll");
	
	if(!shell)
		return;
		
	static t_DwmFlush flush=reinterpret_cast<t_DwmFlush>(GetProcAddress(shell, "DwmFlush"));
	static t_DwmIsCompositionEnabled isEnabled=reinterpret_cast<t_DwmIsCompositionEnabled>(GetProcAddress(shell, "DwmIsCompositionEnabled"));
	
	BOOL enabled;
	isEnabled(&enabled);
	
	if(isEnabled)
		flush();
}

#endif // _WIN32

// Dialogue largening
void large_dialog(DIALOG *d)
{
	large_dialog(d, 1.5);
}

void large_dialog(DIALOG *d, float RESIZE_AMT)
{
	if(!d[0].d1)
	{
		d[0].d1 = 1;
		int32_t oldwidth = d[0].w;
		int32_t oldheight = d[0].h;
		int32_t oldx = d[0].x;
		int32_t oldy = d[0].y;
		d[0].x -= int32_t(d[0].w/RESIZE_AMT);
		d[0].y -= int32_t(d[0].h/RESIZE_AMT);
		d[0].w = int32_t(d[0].w*RESIZE_AMT);
		d[0].h = int32_t(d[0].h*RESIZE_AMT);
		
		for(int32_t i=1; d[i].proc !=NULL; i++)
		{
			// Place elements horizontally
			double xpc = ((double)(d[i].x - oldx) / (double)oldwidth);
			d[i].x = int32_t(d[0].x + (xpc*d[0].w));
			
			if(d[i].proc != d_stringloader)
			{
				if(d[i].proc==d_bitmap_proc)
				{
					d[i].w *= 2;
				}
				else d[i].w = int32_t(d[i].w*RESIZE_AMT);
			}
			
			// Place elements vertically
			double ypc = ((double)(d[i].y - oldy) / (double)oldheight);
			d[i].y = int32_t(d[0].y + (ypc*d[0].h));
			
			// Vertically resize elements
			if(d[i].proc == jwin_edit_proc || d[i].proc == jwin_check_proc || d[i].proc == jwin_checkfont_proc)
			{
				d[i].h = int32_t((double)d[i].h*1.5);
			}
			else if(d[i].proc == jwin_droplist_proc)
			{
				d[i].y += int32_t((double)d[i].h*0.25);
				d[i].h = int32_t((double)d[i].h*1.25);
			}
			else if(d[i].proc==d_bitmap_proc)
			{
				d[i].h *= 2;
			}
			else d[i].h = int32_t(d[i].h*RESIZE_AMT);
			
			// Fix frames
			if(d[i].proc == jwin_frame_proc)
			{
				d[i].x++;
				d[i].y++;
				d[i].w-=4;
				d[i].h-=4;
			}
		}
	}
	
	for(int32_t i=1; d[i].proc!=NULL; i++)
	{
		if(d[i].proc==jwin_slider_proc)
			continue;
			
		// Bigger font
		bool bigfontproc = (d[i].proc != d_midilist_proc && d[i].proc != jwin_droplist_proc && d[i].proc != jwin_abclist_proc && d[i].proc != jwin_list_proc);
		
		if(!d[i].dp2 && bigfontproc)
		{
			//d[i].dp2 = (d[i].proc == jwin_edit_proc) ? sfont3 : lfont_l;
			d[i].dp2 = lfont_l;
		}
		else if(!bigfontproc)
		{
//	  ((ListData *)d[i].dp)->font = &sfont3;
			((ListData *)d[i].dp)->font = &lfont_l;
		}
		
		// Make checkboxes work
		if(d[i].proc == jwin_check_proc)
			d[i].proc = jwin_checkfont_proc;
		else if(d[i].proc == jwin_radio_proc)
			d[i].proc = jwin_radiofont_proc;
	}
	
	jwin_center_dialog(d);
}


/**********************************/
/******** System functions ********/
/**********************************/

static char cfg_sect[] = "zeldadx"; //We need to rename this.

int32_t d_dummy_proc(int32_t,DIALOG *,int32_t)
{
	return D_O_K;
}

void load_game_configs()
{
	//set_config_file("zc.cfg"); //shift back when done
	//load the module
	strcpy(moduledata.module_name,get_config_string("ZCMODULE",qst_module_name,"classic.zmod"));
	joystick_index = zc_get_config(cfg_sect,"joystick_index",0);
	js_stick_1_x_stick = zc_get_config(cfg_sect,"js_stick_1_x_stick",0);
	js_stick_1_x_axis = zc_get_config(cfg_sect,"js_stick_1_x_axis",0);
	js_stick_1_x_offset = zc_get_config(cfg_sect,"js_stick_1_x_offset",0) ? 128 : 0;
	js_stick_1_y_stick = zc_get_config(cfg_sect,"js_stick_1_y_stick",0);
	js_stick_1_y_axis = zc_get_config(cfg_sect,"js_stick_1_y_axis",1);
	js_stick_1_y_offset = zc_get_config(cfg_sect,"js_stick_1_y_offset",0) ? 128 : 0;
	js_stick_2_x_stick = zc_get_config(cfg_sect,"js_stick_2_x_stick",1);
	js_stick_2_x_axis = zc_get_config(cfg_sect,"js_stick_2_x_axis",0);
	js_stick_2_x_offset = zc_get_config(cfg_sect,"js_stick_2_x_offset",0) ? 128 : 0;
	js_stick_2_y_stick = zc_get_config(cfg_sect,"js_stick_2_y_stick",1);
	js_stick_2_y_axis = zc_get_config(cfg_sect,"js_stick_2_y_axis",1);
	js_stick_2_y_offset = zc_get_config(cfg_sect,"js_stick_2_y_offset",0) ? 128 : 0;
	analog_movement = (zc_get_config(cfg_sect,"analog_movement",1));
   
	//cheat modifier keya
	cheat_modifier_keys[0] = zc_get_config(cfg_sect,"key_cheatmod_a1",KEY_LSHIFT);
	cheat_modifier_keys[1] = zc_get_config(cfg_sect,"key_cheatmod_a2",0);
	cheat_modifier_keys[2] = zc_get_config(cfg_sect,"key_cheatmod_b1",KEY_RSHIFT);
	cheat_modifier_keys[3] = zc_get_config(cfg_sect,"key_cheatmod_b2",0);
   
	if((uint32_t)joystick_index >= MAX_JOYSTICKS)
		joystick_index = 0;
	   
	Akey = zc_get_config(cfg_sect,"key_a",KEY_ALT);
	Bkey = zc_get_config(cfg_sect,"key_b",KEY_ZC_LCONTROL);
	Skey = zc_get_config(cfg_sect,"key_s",KEY_ENTER);
	Lkey = zc_get_config(cfg_sect,"key_l",KEY_Z);
	Rkey = zc_get_config(cfg_sect,"key_r",KEY_X);
	Pkey = zc_get_config(cfg_sect,"key_p",KEY_SPACE);
	Exkey1 = zc_get_config(cfg_sect,"key_ex1",KEY_Q);
	Exkey2 = zc_get_config(cfg_sect,"key_ex2",KEY_W);
	Exkey3 = zc_get_config(cfg_sect,"key_ex3",KEY_A);
	Exkey4 = zc_get_config(cfg_sect,"key_ex4",KEY_S);
   
	DUkey = zc_get_config(cfg_sect,"key_up",   KEY_UP);
	DDkey = zc_get_config(cfg_sect,"key_down", KEY_DOWN);
	DLkey = zc_get_config(cfg_sect,"key_left", KEY_LEFT);
	DRkey = zc_get_config(cfg_sect,"key_right",KEY_RIGHT);
   
	Abtn = zc_get_config(cfg_sect,"btn_a",2);
	Bbtn = zc_get_config(cfg_sect,"btn_b",1);
	Sbtn = zc_get_config(cfg_sect,"btn_s",10);
	Mbtn = zc_get_config(cfg_sect,"btn_m",9);
	Lbtn = zc_get_config(cfg_sect,"btn_l",5);
	Rbtn = zc_get_config(cfg_sect,"btn_r",6);
	Pbtn = zc_get_config(cfg_sect,"btn_p",12);
	Exbtn1 = zc_get_config(cfg_sect,"btn_ex1",7);
	Exbtn2 = zc_get_config(cfg_sect,"btn_ex2",8);
	Exbtn3 = zc_get_config(cfg_sect,"btn_ex3",4);
	Exbtn4 = zc_get_config(cfg_sect,"btn_ex4",3);
   
	DUbtn = zc_get_config(cfg_sect,"btn_up",13);
	DDbtn = zc_get_config(cfg_sect,"btn_down",14);
	DLbtn = zc_get_config(cfg_sect,"btn_left",15);
	DRbtn = zc_get_config(cfg_sect,"btn_right",16);
	
	epilepsyFlashReduction = zc_get_config(cfg_sect,"epilepsy_flash_reduction",0);
   
	digi_volume = zc_get_config(cfg_sect,"digi",248);
	midi_volume = zc_get_config(cfg_sect,"midi",255);
	sfx_volume = zc_get_config(cfg_sect,"sfx",248);
	emusic_volume = zc_get_config(cfg_sect,"emusic",248);
	pan_style = zc_get_config(cfg_sect,"pan",1);
	// 1 <= zcmusic_bufsz <= 128
	zcmusic_bufsz = vbound(zc_get_config(cfg_sect,"zcmusic_bufsz",64),1,128);
	volkeys = zc_get_config(cfg_sect,"volkeys",0)!=0;
	zc_vsync = zc_get_config(cfg_sect,"vsync",0);
	Throttlefps = zc_get_config(cfg_sect,"throttlefps",1)!=0;
	TransLayers = zc_get_config(cfg_sect,"translayers",1)!=0;
	SnapshotFormat = zc_get_config(cfg_sect,"snapshot_format",3);
	NameEntryMode = zc_get_config(cfg_sect,"name_entry_mode",0);
	ShowFPS = zc_get_config(cfg_sect,"showfps",0)!=0;
	NESquit = zc_get_config(cfg_sect,"fastquit",0)!=0;
	ClickToFreeze = zc_get_config(cfg_sect,"clicktofreeze",1)!=0;
	title_version = zc_get_config(cfg_sect,"title",2);
	abc_patternmatch = zc_get_config(cfg_sect, "lister_pattern_matching", 1);
	pause_in_background = zc_get_config(cfg_sect, "pause_in_background", 0);
   
	//default - scale x2, 640 x 480
	window_width = resx = zc_get_config(cfg_sect,"resx",640);
	window_height = resy = zc_get_config(cfg_sect,"resy",480);
	//screen_scale = zc_get_config(cfg_sect,"screen_scale",2);
   
	scanlines = zc_get_config(cfg_sect,"scanlines",0)!=0;
	loadlast = zc_get_config(cfg_sect,"load_last",0);
   
// Fullscreen may be problematic on newer windows systems.
#ifdef _WIN32
	fullscreen = zc_get_config(cfg_sect,"fullscreen",0);
#else
	fullscreen = zc_get_config(cfg_sect,"fullscreen",1);
#endif
   
	zc_color_depth = (byte) zc_get_config(cfg_sect,"color_depth",8);
   
	//workaround for the 100% CPU bug. -Gleeok
#ifdef ALLEGRO_MACOSX //IIRC rest(0) was a mac issue fix.
	frame_rest_suggest = (byte) zc_get_config(cfg_sect,"frame_rest_suggest",0);
#else
	frame_rest_suggest = (byte) zc_get_config(cfg_sect,"frame_rest_suggest",1);
#endif
	frame_rest_suggest = zc_min(2, frame_rest_suggest);
   
	forceExit = (byte) zc_get_config(cfg_sect,"force_exit",0);
   
#ifdef _WIN32
	use_debug_console = (byte) zc_get_config(cfg_sect,"debug_console",0);
	zasm_debugger = (byte) zc_get_config("CONSOLE","print_ZASM",0);
	zscript_debugger = (byte) zc_get_config("CONSOLE","ZScript_Debugger",0);
	console_on_top = (byte) zc_get_config("CONSOLE","console_on_top",0);
	//use_win7_keyboard_fix = (byte) zc_get_config(cfg_sect,"use_win7_key_fix",0);
	use_win32_proc = (byte) zc_get_config(cfg_sect,"zc_win_proc_fix",0); //buggy
   
	// This seems to fix some problems on Windows 7
	disable_direct_updating = (byte) zc_get_config("graphics","disable_direct_updating",1);
   
	// This one's for Aero
	use_dwm_flush = (byte) zc_get_config("zeldadx","use_dwm_flush",0);
   
	// And this one fixes patches unloading on some MIDI setups
	midi_patch_fix = (byte) zc_get_config("zeldadx","midi_patch_fix",1);
	monochrome_console = (byte) zc_get_config("CONSOLE","monochrome_debuggers",0);
#else //UNIX
	use_debug_console = false;//(byte) zc_get_config(cfg_sect,"debug_console",0);
	zasm_debugger = (byte) zc_get_config("CONSOLE","print_ZASM",0);
	zscript_debugger = (byte) zc_get_config("CONSOLE","ZScript_Debugger",0);
	monochrome_console = (byte) zc_get_config("CONSOLE","monochrome_debuggers",0);
#endif

	char* default_path = "";
	strcpy(qstdir,get_config_string(cfg_sect,qst_dir_name,default_path));
   
	if(strlen(qstdir)==0)
	{
		getcwd(qstdir,2048);
		fix_filename_case(qstdir);
		fix_filename_slashes(qstdir);
		put_backslash(qstdir);
	}
	else
	{
		chop_path(qstdir);
	}
   
	strcpy(qstpath,qstdir); //qstpath is the local (for this run of ZC) quest path, qstdir is the universal quest dir.
	ss_enable = zc_get_config(cfg_sect,"ss_enable",1) ? 1 : 0;
	ss_after = vbound(zc_get_config(cfg_sect,"ss_after",14), 0, 14);
	ss_speed = vbound(zc_get_config(cfg_sect,"ss_speed",2), 0, 6);
	ss_density = vbound(zc_get_config(cfg_sect,"ss_density",3), 0, 6);
	heart_beep = zc_get_config(cfg_sect,"heart_beep",1)!=0;
	gui_colorset = zc_get_config(cfg_sect,"gui_colorset",0);
	sfxdat = zc_get_config(cfg_sect,"use_sfx_dat",1);
	fullscreen = zc_get_config(cfg_sect,"fullscreen",1);
	use_save_indicator = zc_get_config(cfg_sect,"save_indicator",0);
	zc_192b163_warp_compatibility = zc_get_config(cfg_sect,"zc_192b163_warp_compatibility",0);
}

void save_game_configs()
{
	packfile_password("");
 
	set_config_string("ZCMODULE",qst_module_name,moduledata.module_name);
	
	set_config_int(cfg_sect,"joystick_index",joystick_index);
	set_config_int(cfg_sect,"js_stick_1_x_stick",js_stick_1_x_stick);
	set_config_int(cfg_sect,"js_stick_1_x_axis",js_stick_1_x_axis);
	set_config_int(cfg_sect,"js_stick_1_x_offset",js_stick_1_x_offset ? 1 : 0);
	set_config_int(cfg_sect,"js_stick_1_y_stick",js_stick_1_y_stick);
	set_config_int(cfg_sect,"js_stick_1_y_axis",js_stick_1_y_axis);
	set_config_int(cfg_sect,"js_stick_1_y_offset",js_stick_1_y_offset ? 1 : 0);
	set_config_int(cfg_sect,"js_stick_2_x_stick",js_stick_2_x_stick);
	set_config_int(cfg_sect,"js_stick_2_x_axis",js_stick_2_x_axis);
	set_config_int(cfg_sect,"js_stick_2_x_offset",js_stick_2_x_offset ? 1 : 0);
	set_config_int(cfg_sect,"js_stick_2_y_stick",js_stick_2_y_stick);
	set_config_int(cfg_sect,"js_stick_2_y_axis",js_stick_2_y_axis);
	set_config_int(cfg_sect,"js_stick_2_y_offset",js_stick_2_y_offset ? 1 : 0);
	set_config_int(cfg_sect,"analog_movement",analog_movement);
	
	 //cheat modifier keya
   
	set_config_int(cfg_sect,"key_cheatmod_a1",cheat_modifier_keys[0]);
	set_config_int(cfg_sect,"key_cheatmod_a2",cheat_modifier_keys[1]);
	set_config_int(cfg_sect,"key_cheatmod_b1",cheat_modifier_keys[2]);
	set_config_int(cfg_sect,"key_cheatmod_b2",cheat_modifier_keys[3]);
   
   
   
   
	set_config_int(cfg_sect,"key_a",Akey);
	set_config_int(cfg_sect,"key_b",Bkey);
	set_config_int(cfg_sect,"key_s",Skey);
	set_config_int(cfg_sect,"key_l",Lkey);
	set_config_int(cfg_sect,"key_r",Rkey);
	set_config_int(cfg_sect,"key_p",Pkey);
	set_config_int(cfg_sect,"key_ex1",Exkey1);
	set_config_int(cfg_sect,"key_ex2",Exkey2);
	set_config_int(cfg_sect,"key_ex3",Exkey3);
	set_config_int(cfg_sect,"key_ex4",Exkey4);
   
	set_config_int(cfg_sect,"key_up",   DUkey);
	set_config_int(cfg_sect,"key_down", DDkey);
	set_config_int(cfg_sect,"key_left", DLkey);
	set_config_int(cfg_sect,"key_right",DRkey);
   
	set_config_int(cfg_sect,"btn_a",Abtn);
	set_config_int(cfg_sect,"btn_b",Bbtn);
	set_config_int(cfg_sect,"btn_s",Sbtn);
	set_config_int(cfg_sect,"btn_m",Mbtn);
	set_config_int(cfg_sect,"btn_l",Lbtn);
	set_config_int(cfg_sect,"btn_r",Rbtn);
	set_config_int(cfg_sect,"btn_p",Pbtn);
	set_config_int(cfg_sect,"btn_ex1",Exbtn1);
	set_config_int(cfg_sect,"btn_ex2",Exbtn2);
	set_config_int(cfg_sect,"btn_ex3",Exbtn3);
	set_config_int(cfg_sect,"btn_ex4",Exbtn4);
   
	set_config_int(cfg_sect,"btn_up",DUbtn);
	set_config_int(cfg_sect,"btn_down",DDbtn);
	set_config_int(cfg_sect,"btn_left",DLbtn);
	set_config_int(cfg_sect,"btn_right",DRbtn);
	
	set_config_int(cfg_sect,"epilepsy_flash_reduction",epilepsyFlashReduction);
   
	set_config_int(cfg_sect,"digi",digi_volume);
	set_config_int(cfg_sect,"midi",midi_volume);
	set_config_int(cfg_sect,"sfx",sfx_volume);
	set_config_int(cfg_sect,"emusic",emusic_volume);
	set_config_int(cfg_sect,"pan",pan_style);
	set_config_int(cfg_sect,"zcmusic_bufsz",zcmusic_bufsz);
	set_config_int(cfg_sect,"volkeys",(int32_t)volkeys);
	set_config_int(cfg_sect,"vsync",zc_vsync);
	set_config_int(cfg_sect,"throttlefps", (int32_t)Throttlefps);
	set_config_int(cfg_sect,"translayers",(int32_t)TransLayers);
	set_config_int(cfg_sect,"snapshot_format",SnapshotFormat);
	set_config_int(cfg_sect,"name_entry_mode",NameEntryMode);
	set_config_int(cfg_sect,"showfps",(int32_t)ShowFPS);
	set_config_int(cfg_sect,"fastquit",(int32_t)NESquit);
	set_config_int(cfg_sect,"clicktofreeze", (int32_t)ClickToFreeze);
	set_config_int(cfg_sect,"title",title_version);
	//set_config_int(cfg_sect,"lister_pattern_matching",abc_patternmatch);  //Enable once there is a GUI way to toggle this. 
   
	
   
	set_config_int(cfg_sect,"resx",window_width);
	set_config_int(cfg_sect,"resy",window_height);
   
	//sbig depricated as of 2.5 RC3. handled exclusively by resx, resy now.
	//set_config_int(cfg_sect,"screen_scale",screen_scale);
	//set_config_int(cfg_sect,"sbig",sbig);
	//set_config_int(cfg_sect,"sbig2",sbig2);
   
	set_config_int(cfg_sect,"scanlines",scanlines);
	set_config_int(cfg_sect,"load_last",loadlast);
	chop_path(qstdir);
	set_config_string(cfg_sect,qst_dir_name,qstdir);
	set_config_string("SAVEFILE","save_filename",save_file_name);
	set_config_int(cfg_sect,"ss_enable",ss_enable);
	set_config_int(cfg_sect,"ss_after",ss_after);
	set_config_int(cfg_sect,"ss_speed",ss_speed);
	set_config_int(cfg_sect,"ss_density",ss_density);
	set_config_int(cfg_sect,"heart_beep",heart_beep);
	set_config_int(cfg_sect,"gui_colorset",gui_colorset);
	set_config_int(cfg_sect,"use_sfx_dat",sfxdat);
	set_config_int(cfg_sect,"fullscreen",fullscreen);
	set_config_int(cfg_sect,"color_depth",zc_color_depth);
	set_config_int(cfg_sect,"frame_rest_suggest",frame_rest_suggest);
	set_config_int(cfg_sect,"force_exit",forceExit);
   
#ifdef _WIN32
	set_config_int(cfg_sect,"debug_console",use_debug_console);
	set_config_int("CONSOLE","print_ZASM",zasm_debugger);
	set_config_int("CONSOLE","ZScript_Debugger",zscript_debugger);
	set_config_int("CONSOLE","console_on_top",console_on_top);
	//set_config_int(cfg_sect,"use_win7_key_fix",use_win7_keyboard_fix);
	set_config_int(cfg_sect,"zc_win_proc_fix",use_win32_proc);
	set_config_int("graphics","disable_direct_updating",disable_direct_updating);
	set_config_int("zeldadx","use_dwm_flush",use_dwm_flush);
	set_config_int("zeldadx","midi_patch_fix",midi_patch_fix);
	set_config_int("CONSOLE","monochrome_debuggers",monochrome_console);
	set_config_int("zeldadx","debug_console",zconsole);
#endif
   
#ifdef ALLEGRO_LINUX
	set_config_string("sound","patches",samplepath); // set to sample sound path set for DIGMIDI driver in Linux ~ Takuya
#endif
   
	set_config_int(cfg_sect,"save_indicator",use_save_indicator);
	set_config_int(cfg_sect,"zc_192b163_warp_compatibility",zc_192b163_warp_compatibility);
   
	flush_config_file();
}

//----------------------------------------------------------------

// Timers

void fps_callback()
{
	lastfps=framecnt;
	dword tempsecs = fps_secs;
	++tempsecs;
	//avgfps=((long double)avgfps*fps_secs+lastfps)/(++fps_secs); // DJGPP doesn't like this
	avgfps=((long double)avgfps*fps_secs+lastfps)/(tempsecs);
	++fps_secs;
	framecnt=0;
}

END_OF_FUNCTION(fps_callback)

int32_t Z_init_timers()
{
	static bool didit = false;
	const static char *err_str = "Couldn't allocate timer";
	err_str = err_str; //Unused variable warning
	
	if(didit)
		return 1;
		
	didit = true;
	
	LOCK_VARIABLE(lastfps);
	LOCK_VARIABLE(framecnt);
	LOCK_FUNCTION(fps_callback);
	
	if(install_int_ex(fps_callback,SECS_TO_TIMER(1)))
		return 0;
		
	return 1;
}

void Z_remove_timers()
{
	remove_int(fps_callback);
}

//----------------------------------------------------------------

void go()
{
	scare_mouse();
	blit(screen,tmp_scr,scrx,scry,0,0,screen->w,screen->h);
	unscare_mouse();
}

void comeback()
{
	scare_mouse();
	blit(tmp_scr,screen,0,0,scrx,scry,screen->w,screen->h);
	unscare_mouse();
}

void dump_pal(BITMAP *dest)
{
	for(int32_t i=0; i<256; i++)
		rectfill(dest,(i&63)<<2,(i&0xFC0)>>4,((i&63)<<2)+3,((i&0xFC0)>>4)+3,i);
}

void show_paused(BITMAP *target)
{
	//  return;
	char buf[7] = "PAUSED";
	
	for(int32_t i=0; buf[i]!=0; i++)
		buf[i]+=0x60;
		
	//  text_mode(-1);
	if(sbig)
	{
		int32_t x = scrx+40-((screen_scale-1)*120);
		int32_t y = scry+224+((screen_scale-1)*104);
		textout_ex(target,zfont,buf,x,y,-1,-1);
	}
	else
		textout_ex(target,zfont,buf,scrx+40,scry+224,-1,-1);
}

void show_fps(BITMAP *target)
{
	char buf[50];
	
	//  text_mode(-1);
	sprintf(buf,"%2d/60",lastfps);
	
	//  sprintf(buf,"%d/%u/%f/%u",lastfps,int32_t(avgfps),avgfps,fps_secs);
	for(int32_t i=0; buf[i]!=0; i++)
		if(buf[i]!=' ')
			buf[i]+=0x60;
			
	if(sbig)
	{
		int32_t x = scrx+40-((screen_scale-1)*120);
		int32_t y = scry+216+((screen_scale-1)*104);
		textout_ex(target,zfont,buf,x,y,-1,-1);
		// textout_ex(target,zfont,buf,scrx+40-120,scry+216+104,-1,-1);
	}
	else
	{
		textout_ex(target,zfont,buf,scrx+40,scry+216,-1,-1);
	}
}

void show_saving(BITMAP *target)
{
	if(!use_save_indicator)
		return;
	
	char buf[10] = "SAVING...";
	
	for(int32_t i=0; buf[i]!=0; i++)
		buf[i]+=0x60;
		
	if(sbig)
	{
		int32_t x = scrx+200+((screen_scale-1)*120);
		int32_t y = scry+224+((screen_scale-1)*104);
		textout_ex(target,zfont,buf,x,y,-1,-1);
	}
	else
		textout_ex(target,zfont,buf,scrx+200,scry+224,-1,-1);
}

void show_replay_controls(BITMAP *target)
{
	if (!replay_is_replaying())
		return;
	
	std::string text = replay_get_buttons_string();
		
	if(sbig)
	{
		int32_t x = scrx+140+((screen_scale-1)*120);
		int32_t y = scry+224+((screen_scale-1)*104);
		textout_ex(target,zfont,text.c_str(),x,y,-1,0);
	}
	else
		textout_ex(target,zfont,text.c_str(),scrx+140,scry+224,-1,0);
}

//----------------------------------------------------------------

//Handles converting the mouse sprite from the .dat file
void load_mouse()
{
	system_pal();
	scare_mouse();
	set_mouse_sprite(NULL);
	int32_t sz = vbound(int32_t(16*(is_large ? get_config_float("zeldadx","cursor_scale_large",1.5) : get_config_float("zeldadx","cursor_scale_small",1))),16,80);
	for(int32_t j = 0; j < 4; ++j)
	{
		BITMAP* tmpbmp = create_bitmap_ex(8,16,16);
		BITMAP* subbmp = create_bitmap_ex(8,16,16);
		if(zcmouse[j])
			destroy_bitmap(zcmouse[j]);
		zcmouse[j] = create_bitmap_ex(8,sz,sz);
		clear_bitmap(zcmouse[j]);
		clear_bitmap(tmpbmp);
		clear_bitmap(subbmp);
		blit((BITMAP*)data[BMP_MOUSE].dat,tmpbmp,1,j*17+1,0,0,16,16);
		for(int32_t x = 0; x < 16; ++x)
		{
			for(int32_t y = 0; y < 16; ++y)
			{
				int32_t color = getpixel(tmpbmp, x, y);
				switch(color)
				{
					case dvc(1):
						color = jwin_pal[jcCURSORMISC];
						break;
					case dvc(2):
						color = jwin_pal[jcCURSOROUTLINE];
						break;
					case dvc(3):
						color = jwin_pal[jcCURSORLIGHT];
						break;
					case dvc(5):
						color = jwin_pal[jcCURSORDARK];
						break;
				}
				putpixel(subbmp, x, y, color);
			}
		}
		if(sz!=16)
			stretch_blit(subbmp, zcmouse[j], 0, 0, 16, 16, 0, 0, sz, sz);
		else
			blit(subbmp, zcmouse[j], 0, 0, 0, 0, 16, 16);
		destroy_bitmap(tmpbmp);
		destroy_bitmap(subbmp);
	}
	set_mouse_sprite(zcmouse[0]);
	unscare_mouse();
	game_pal();
}

// sets the video mode and initializes the palette and mouse sprite
bool game_vid_mode(int32_t mode,int32_t wait)
{
	if(set_gfx_mode(mode,resx,resy,0,0)!=0)
	{
		return false;
	}
	
	scrx = (resx-320)>>1;
	scry = (resy-240)>>1;
	for(int32_t q = 0; q < 4; ++q)
		zcmouse[q] = NULL;
	load_mouse();
	set_mouse_sprite(zcmouse[0]);
	
	for(int32_t i=240; i<256; i++)
		RAMpal[i]=((RGB*)data[PAL_GUI].dat)[i];
		
	set_palette(RAMpal);
	clear_to_color(screen,BLACK);
	
	rest(wait);
	return true;
}

void null_quest()
{
	char qstdat_string[2048];
	strcpy(qstdat_string,moduledata.datafiles[qst_dat]);
	strcat(qstdat_string,"#NESQST_NEW_QST");
	
	byte skip_flags[4] = { 0 };
	
	loadquest(qstdat_string,&QHeader,&QMisc,tunes+ZC_MIDI_COUNT,false,true,true,true,skip_flags,0,false);
}

void init_NES_mode()
{
	/*
	// qst.dat may not load correctly without this...
	QHeader.templatepath[0]='\0';
	
	if(!init_colordata(true, &QHeader, &QMisc))
	{
		return;
	}
	
	loadfullpal();
	init_tiles(false, &QHeader);
	*/
	null_quest();
}

//----------------------------------------------------------------

qword trianglelines[16]=
{
	0x0000000000000000ULL,
	0xFD00000000000000ULL,
	0xFDFD000000000000ULL,
	0xFDFDFD0000000000ULL,
	0xFDFDFDFD00000000ULL,
	0xFDFDFDFDFD000000ULL,
	0xFDFDFDFDFDFD0000ULL,
	0xFDFDFDFDFDFDFD00ULL,
	0xFDFDFDFDFDFDFDFDULL,
	0x00FDFDFDFDFDFDFDULL,
	0x0000FDFDFDFDFDFDULL,
	0x000000FDFDFDFDFDULL,
	0x00000000FDFDFDFDULL,
	0x0000000000FDFDFDULL,
	0x000000000000FDFDULL,
	0x00000000000000FDULL,
};

word screen_triangles[28][32];
/*
  qword triangles[4][16]= //[direction][value]
  {
  {
  0x00000000, 0x10000000, 0x21000000, 0x32100000, 0x43210000, 0x54321000, 0x65432100, 0x76543210, 0x87654321, 0x88765432, 0x88876543, 0x88887654, 0x88888765, 0x88888876, 0x88888887, 0x88888888
  },
  {
  0x00000000, 0xF0000000, 0xEF000000, 0xFDF00000, 0xCFDF0000, 0xBCFDF000, 0xABCFDF00, 0x9ABCFDF0, 0x89ABCFDF, 0x889ABCFD, 0x8889ABCD, 0x88889ABC, 0x888889AB, 0x8888889A, 0x88888889, 0x88888888
  },
  {
  0x00000000, 0x00000001, 0x00000012, 0x00000123, 0x00001234, 0x00012345, 0x00123456, 0x01234567, 0x12345678, 0x23456788, 0x34567888, 0x45678888, 0x56788888, 0x67888888, 0x78888888, 0x88888888
  },
  {
  0x00000000, 0x0000000F, 0x000000FE, 0x00000FED, 0x0000FEDC, 0x000FEDCB, 0x00FEDCBA, 0x0FEDCBA9, 0xFEDCBA98, 0xEDCBA988, 0xDCBA9888, 0xCBA98888, 0xBA988888, 0xA9888888, 0x98888888, 0x88888888
  }
  };
  */


/*
  byte triangles[4][16][8]= //[direction][value][line]
  {
  {
  {
  0,  0,  0,  0,  0,  0,  0,  0
  },
  {
  1,  0,  0,  0,  0,  0,  0,  0
  },
  {
  2,  1,  0,  0,  0,  0,  0,  0
  },
  {
  3,  2,  1,  0,  0,  0,  0,  0
  },
  {
  4,  3,  2,  1,  0,  0,  0,  0
  },
  {
  5,  4,  3,  2,  1,  0,  0,  0
  },
  {
  6,  5,  4,  3,  2,  1,  0,  0
  },
  {
  7,  6,  5,  4,  3,  2,  1,  0
  },
  {
  8,  7,  6,  5,  4,  3,  2,  1
  },
  {
  8,  8,  7,  6,  5,  4,  3,  2
  },
  {
  8,  8,  8,  7,  6,  5,  4,  3
  },
  {
  8,  8,  8,  8,  7,  6,  5,  4
  },
  {
  8,  8,  8,  8,  8,  7,  6,  5
  },
  {
  8,  8,  8,  8,  8,  8,  7,  6
  },
  {
  8,  8,  8,  8,  8,  8,  8,  7
  },
  {
  8,  8,  8,  8,  8,  8,  8,  8
  }
  },
  {
  {
  0,  0,  0,  0,  0,  0,  0,  0
  },
  {
  15,  0,  0,  0,  0,  0,  0,  0
  },
  {
  14, 15,  0,  0,  0,  0,  0,  0
  },
  {
  13, 14, 15,  0,  0,  0,  0,  0
  },
  {
  12, 13, 14, 15,  0,  0,  0,  0
  },
  {
  11, 12, 13, 14, 15,  0,  0,  0
  },
  {
  10, 11, 12, 13, 14, 15,  0,  0
  },
  {
  9, 10, 11, 12, 13, 14, 15,  0
  },
  {
  8,  9, 10, 11, 12, 13, 14, 15
  },
  {
  8,  8,  9, 10, 11, 12, 13, 14
  },
  {
  8,  8,  8,  9, 10, 11, 12, 13
  },
  {
  8,  8,  8,  8,  9, 10, 11, 12
  },
  {
  8,  8,  8,  8,  8,  9, 10, 11
  },
  {
  8,  8,  8,  8,  8,  8,  9, 10
  },
  {
  8,  8,  8,  8,  8,  8,  8,  9
  },
  {
  8,  8,  8,  8,  8,  8,  8,  8
  }
  },
  {
  {
  0,  0,  0,  0,  0,  0,  0,  0
  },
  {
  0,  0,  0,  0,  0,  0,  0,  1
  },
  {
  0,  0,  0,  0,  0,  0,  1,  2
  },
  {
  0,  0,  0,  0,  0,  1,  2,  3
  },
  {
  0,  0,  0,  0,  1,  2,  3,  4
  },
  {
  0,  0,  0,  1,  2,  3,  4,  5
  },
  {
  0,  0,  1,  2,  3,  4,  5,  6
  },
  {
  0,  1,  2,  3,  4,  5,  6,  7
  },
  {
  1,  2,  3,  4,  5,  6,  7,  8
  },
  {
  2,  3,  4,  5,  6,  7,  8,  8
  },
  {
  3,  4,  5,  6,  7,  8,  8,  8
  },
  {
  4,  5,  6,  7,  8,  8,  8,  8
  },
  {
  5,  6,  7,  8,  8,  8,  8,  8
  },
  {
  6,  7,  8,  8,  8,  8,  8,  8
  },
  {
  7,  8,  8,  8,  8,  8,  8,  8
  },
  {
  8,  8,  8,  8,  8,  8,  8,  8
  }
  },
  {
  {
  0,  0,  0,  0,  0,  0,  0,  0
  },
  {
  0,  0,  0,  0,  0,  0,  0, 15
  },
  {
  0,  0,  0,  0,  0,  0, 15, 14
  },
  {
  0,  0,  0,  0,  0, 15, 14, 13
  },
  {
  0,  0,  0,  0, 15, 14, 13, 12
  },
  {
  0,  0,  0, 15, 14, 13, 12, 11
  },
  {
  0,  0, 15, 14, 13, 12, 11, 10
  },
  {
  0, 15, 14, 13, 12, 11, 10,  9
  },
  {
  15, 14, 13, 12, 11, 10,  9,  8
  },
  {
  14, 13, 12, 11, 10,  9,  8,  8
  },
  {
  13, 12, 11, 10,  9,  8,  8,  8
  },
  {
  12, 11, 10,  9,  8,  8,  8,  8
  },
  {
  11, 10,  9,  8,  8,  8,  8,  8
  },
  {
  10,  9,  8,  8,  8,  8,  8,  8
  },
  {
  9,  8,  8,  8,  8,  8,  8,  8
  },
  {
  8,  8,  8,  8,  8,  8,  8,  8
  }
  }
  };
  */



/*
  for (int32_t blockrow=0; blockrow<30; ++i)
  {
  for (int32_t linerow=0; linerow<8; ++i)
  {
  qword *triangleline=(qword*)(tmp_scr->line[(blockrow*8+linerow)]);
  for (int32_t blockcolumn=0; blockcolumn<40; ++i)
  {
  triangleline=triangles[0][screen_triangles[blockrow][blockcolumn]][linerow];
  ++triangleline;
  }
  }
  }
  */

// the ULL suffixes are to prevent this warning:
// warning: integer constant is too large for "int32_t" type

qword triangles[4][16][8]= //[direction][value][line]
{
	{
		{
			0x0000000000000000ULL,
			0x0000000000000000ULL,
			0x0000000000000000ULL,
			0x0000000000000000ULL,
			0x0000000000000000ULL,
			0x0000000000000000ULL,
			0x0000000000000000ULL,
			0x0000000000000000ULL
		},
		{
			0xFD00000000000000ULL,
			0x0000000000000000ULL,
			0x0000000000000000ULL,
			0x0000000000000000ULL,
			0x0000000000000000ULL,
			0x0000000000000000ULL,
			0x0000000000000000ULL,
			0x0000000000000000ULL
		},
		{
			0xFDFD000000000000ULL,
			0xFD00000000000000ULL,
			0x0000000000000000ULL,
			0x0000000000000000ULL,
			0x0000000000000000ULL,
			0x0000000000000000ULL,
			0x0000000000000000ULL,
			0x0000000000000000ULL
		},
		{
			0xFDFDFD0000000000ULL,
			0xFDFD000000000000ULL,
			0xFD00000000000000ULL,
			0x0000000000000000ULL,
			0x0000000000000000ULL,
			0x0000000000000000ULL,
			0x0000000000000000ULL,
			0x0000000000000000ULL
		},
		{
			0xFDFDFDFD00000000ULL,
			0xFDFDFD0000000000ULL,
			0xFDFD000000000000ULL,
			0xFD00000000000000ULL,
			0x0000000000000000ULL,
			0x0000000000000000ULL,
			0x0000000000000000ULL,
			0x0000000000000000ULL
		},
		{
			0xFDFDFDFDFD000000ULL,
			0xFDFDFDFD00000000ULL,
			0xFDFDFD0000000000ULL,
			0xFDFD000000000000ULL,
			0xFD00000000000000ULL,
			0x0000000000000000ULL,
			0x0000000000000000ULL,
			0x0000000000000000ULL
		},
		{
			0xFDFDFDFDFDFD0000ULL,
			0xFDFDFDFDFD000000ULL,
			0xFDFDFDFD00000000ULL,
			0xFDFDFD0000000000ULL,
			0xFDFD000000000000ULL,
			0xFD00000000000000ULL,
			0x0000000000000000ULL,
			0x0000000000000000ULL
		},
		{
			0xFDFDFDFDFDFDFD00ULL,
			0xFDFDFDFDFDFD0000ULL,
			0xFDFDFDFDFD000000ULL,
			0xFDFDFDFD00000000ULL,
			0xFDFDFD0000000000ULL,
			0xFDFD000000000000ULL,
			0xFD00000000000000ULL,
			0x0000000000000000ULL
		},
		{
			0xFDFDFDFDFDFDFDFDULL,
			0xFDFDFDFDFDFDFD00ULL,
			0xFDFDFDFDFDFD0000ULL,
			0xFDFDFDFDFD000000ULL,
			0xFDFDFDFD00000000ULL,
			0xFDFDFD0000000000ULL,
			0xFDFD000000000000ULL,
			0xFD00000000000000ULL
		},
		{
			0xFDFDFDFDFDFDFDFDULL,
			0xFDFDFDFDFDFDFDFDULL,
			0xFDFDFDFDFDFDFD00ULL,
			0xFDFDFDFDFDFD0000ULL,
			0xFDFDFDFDFD000000ULL,
			0xFDFDFDFD00000000ULL,
			0xFDFDFD0000000000ULL,
			0xFDFD000000000000ULL
		},
		{
			0xFDFDFDFDFDFDFDFDULL,
			0xFDFDFDFDFDFDFDFDULL,
			0xFDFDFDFDFDFDFDFDULL,
			0xFDFDFDFDFDFDFD00ULL,
			0xFDFDFDFDFDFD0000ULL,
			0xFDFDFDFDFD000000ULL,
			0xFDFDFDFD00000000ULL,
			0xFDFDFD0000000000ULL
		},
		{
			0xFDFDFDFDFDFDFDFDULL,
			0xFDFDFDFDFDFDFDFDULL,
			0xFDFDFDFDFDFDFDFDULL,
			0xFDFDFDFDFDFDFDFDULL,
			0xFDFDFDFDFDFDFD00ULL,
			0xFDFDFDFDFDFD0000ULL,
			0xFDFDFDFDFD000000ULL,
			0xFDFDFDFD00000000ULL
		},
		{
			0xFDFDFDFDFDFDFDFDULL,
			0xFDFDFDFDFDFDFDFDULL,
			0xFDFDFDFDFDFDFDFDULL,
			0xFDFDFDFDFDFDFDFDULL,
			0xFDFDFDFDFDFDFDFDULL,
			0xFDFDFDFDFDFDFD00ULL,
			0xFDFDFDFDFDFD0000ULL,
			0xFDFDFDFDFD000000ULL
		},
		{
			0xFDFDFDFDFDFDFDFDULL,
			0xFDFDFDFDFDFDFDFDULL,
			0xFDFDFDFDFDFDFDFDULL,
			0xFDFDFDFDFDFDFDFDULL,
			0xFDFDFDFDFDFDFDFDULL,
			0xFDFDFDFDFDFDFDFDULL,
			0xFDFDFDFDFDFDFD00ULL,
			0xFDFDFDFDFDFD0000ULL
		},
		{
			0xFDFDFDFDFDFDFDFDULL,
			0xFDFDFDFDFDFDFDFDULL,
			0xFDFDFDFDFDFDFDFDULL,
			0xFDFDFDFDFDFDFDFDULL,
			0xFDFDFDFDFDFDFDFDULL,
			0xFDFDFDFDFDFDFDFDULL,
			0xFDFDFDFDFDFDFDFDULL,
			0xFDFDFDFDFDFDFD00ULL
		},
		{
			0xFDFDFDFDFDFDFDFDULL,
			0xFDFDFDFDFDFDFDFDULL,
			0xFDFDFDFDFDFDFDFDULL,
			0xFDFDFDFDFDFDFDFDULL,
			0xFDFDFDFDFDFDFDFDULL,
			0xFDFDFDFDFDFDFDFDULL,
			0xFDFDFDFDFDFDFDFDULL,
			0xFDFDFDFDFDFDFDFDULL
		}
	},
	{
		{
			0x0000000000000000ULL,
			0x0000000000000000ULL,
			0x0000000000000000ULL,
			0x0000000000000000ULL,
			0x0000000000000000ULL,
			0x0000000000000000ULL,
			0x0000000000000000ULL,
			0x0000000000000000ULL
		},
		{
			0x00000000000000FDULL,
			0x0000000000000000ULL,
			0x0000000000000000ULL,
			0x0000000000000000ULL,
			0x0000000000000000ULL,
			0x0000000000000000ULL,
			0x0000000000000000ULL,
			0x0000000000000000ULL
		},
		{
			0x000000000000FDFDULL,
			0x00000000000000FDULL,
			0x0000000000000000ULL,
			0x0000000000000000ULL,
			0x0000000000000000ULL,
			0x0000000000000000ULL,
			0x0000000000000000ULL,
			0x0000000000000000ULL
		},
		{
			0x0000000000FDFDFDULL,
			0x000000000000FDFDULL,
			0x00000000000000FDULL,
			0x0000000000000000ULL,
			0x0000000000000000ULL,
			0x0000000000000000ULL,
			0x0000000000000000ULL,
			0x0000000000000000ULL
		},
		{
			0x00000000FDFDFDFDULL,
			0x0000000000FDFDFDULL,
			0x000000000000FDFDULL,
			0x00000000000000FDULL,
			0x0000000000000000ULL,
			0x0000000000000000ULL,
			0x0000000000000000ULL,
			0x0000000000000000ULL
		},
		{
			0x000000FDFDFDFDFDULL,
			0x00000000FDFDFDFDULL,
			0x0000000000FDFDFDULL,
			0x000000000000FDFDULL,
			0x00000000000000FDULL,
			0x0000000000000000ULL,
			0x0000000000000000ULL,
			0x0000000000000000ULL
		},
		{
			0x0000FDFDFDFDFDFDULL,
			0x000000FDFDFDFDFDULL,
			0x00000000FDFDFDFDULL,
			0x0000000000FDFDFDULL,
			0x000000000000FDFDULL,
			0x00000000000000FDULL,
			0x0000000000000000ULL,
			0x0000000000000000ULL
		},
		{
			0x00FDFDFDFDFDFDFDULL,
			0x0000FDFDFDFDFDFDULL,
			0x000000FDFDFDFDFDULL,
			0x00000000FDFDFDFDULL,
			0x0000000000FDFDFDULL,
			0x000000000000FDFDULL,
			0x00000000000000FDULL,
			0x0000000000000000ULL
		},
		{
			0xFDFDFDFDFDFDFDFDULL,
			0x00FDFDFDFDFDFDFDULL,
			0x0000FDFDFDFDFDFDULL,
			0x000000FDFDFDFDFDULL,
			0x00000000FDFDFDFDULL,
			0x0000000000FDFDFDULL,
			0x000000000000FDFDULL,
			0x00000000000000FDULL
		},
		{
			0xFDFDFDFDFDFDFDFDULL,
			0xFDFDFDFDFDFDFDFDULL,
			0x00FDFDFDFDFDFDFDULL,
			0x0000FDFDFDFDFDFDULL,
			0x000000FDFDFDFDFDULL,
			0x00000000FDFDFDFDULL,
			0x0000000000FDFDFDULL,
			0x000000000000FDFDULL
		},
		{
			0xFDFDFDFDFDFDFDFDULL,
			0xFDFDFDFDFDFDFDFDULL,
			0xFDFDFDFDFDFDFDFDULL,
			0x00FDFDFDFDFDFDFDULL,
			0x0000FDFDFDFDFDFDULL,
			0x000000FDFDFDFDFDULL,
			0x00000000FDFDFDFDULL,
			0x0000000000FDFDFDULL
		},
		{
			0xFDFDFDFDFDFDFDFDULL,
			0xFDFDFDFDFDFDFDFDULL,
			0xFDFDFDFDFDFDFDFDULL,
			0xFDFDFDFDFDFDFDFDULL,
			0x00FDFDFDFDFDFDFDULL,
			0x0000FDFDFDFDFDFDULL,
			0x000000FDFDFDFDFDULL,
			0x00000000FDFDFDFDULL
		},
		{
			0xFDFDFDFDFDFDFDFDULL,
			0xFDFDFDFDFDFDFDFDULL,
			0xFDFDFDFDFDFDFDFDULL,
			0xFDFDFDFDFDFDFDFDULL,
			0xFDFDFDFDFDFDFDFDULL,
			0x00FDFDFDFDFDFDFDULL,
			0x0000FDFDFDFDFDFDULL,
			0x000000FDFDFDFDFDULL
		},
		{
			0xFDFDFDFDFDFDFDFDULL,
			0xFDFDFDFDFDFDFDFDULL,
			0xFDFDFDFDFDFDFDFDULL,
			0xFDFDFDFDFDFDFDFDULL,
			0xFDFDFDFDFDFDFDFDULL,
			0xFDFDFDFDFDFDFDFDULL,
			0x00FDFDFDFDFDFDFDULL,
			0x0000FDFDFDFDFDFDULL
		},
		{
			0xFDFDFDFDFDFDFDFDULL,
			0xFDFDFDFDFDFDFDFDULL,
			0xFDFDFDFDFDFDFDFDULL,
			0xFDFDFDFDFDFDFDFDULL,
			0xFDFDFDFDFDFDFDFDULL,
			0xFDFDFDFDFDFDFDFDULL,
			0xFDFDFDFDFDFDFDFDULL,
			0x00FDFDFDFDFDFDFDULL
		},
		{
			0xFDFDFDFDFDFDFDFDULL,
			0xFDFDFDFDFDFDFDFDULL,
			0xFDFDFDFDFDFDFDFDULL,
			0xFDFDFDFDFDFDFDFDULL,
			0xFDFDFDFDFDFDFDFDULL,
			0xFDFDFDFDFDFDFDFDULL,
			0xFDFDFDFDFDFDFDFDULL,
			0xFDFDFDFDFDFDFDFDULL
		}
	},
	{
		{
			0x0000000000000000ULL,
			0x0000000000000000ULL,
			0x0000000000000000ULL,
			0x0000000000000000ULL,
			0x0000000000000000ULL,
			0x0000000000000000ULL,
			0x0000000000000000ULL,
			0x0000000000000000ULL
		},
		{
			0x0000000000000000ULL,
			0x0000000000000000ULL,
			0x0000000000000000ULL,
			0x0000000000000000ULL,
			0x0000000000000000ULL,
			0x0000000000000000ULL,
			0x0000000000000000ULL,
			0xFD00000000000000ULL
		},
		{
			0x0000000000000000ULL,
			0x0000000000000000ULL,
			0x0000000000000000ULL,
			0x0000000000000000ULL,
			0x0000000000000000ULL,
			0x0000000000000000ULL,
			0xFD00000000000000ULL,
			0xFDFD000000000000ULL
		},
		{
			0x0000000000000000ULL,
			0x0000000000000000ULL,
			0x0000000000000000ULL,
			0x0000000000000000ULL,
			0x0000000000000000ULL,
			0xFD00000000000000ULL,
			0xFDFD000000000000ULL,
			0xFDFDFD0000000000ULL
		},
		{
			0x0000000000000000ULL,
			0x0000000000000000ULL,
			0x0000000000000000ULL,
			0x0000000000000000ULL,
			0xFD00000000000000ULL,
			0xFDFD000000000000ULL,
			0xFDFDFD0000000000ULL,
			0xFDFDFDFD00000000ULL
		},
		{
			0x0000000000000000ULL,
			0x0000000000000000ULL,
			0x0000000000000000ULL,
			0xFD00000000000000ULL,
			0xFDFD000000000000ULL,
			0xFDFDFD0000000000ULL,
			0xFDFDFDFD00000000ULL,
			0xFDFDFDFDFD000000ULL
		},
		{
			0x0000000000000000ULL,
			0x0000000000000000ULL,
			0xFD00000000000000ULL,
			0xFDFD000000000000ULL,
			0xFDFDFD0000000000ULL,
			0xFDFDFDFD00000000ULL,
			0xFDFDFDFDFD000000ULL,
			0xFDFDFDFDFDFD0000ULL
		},
		{
			0x0000000000000000ULL,
			0xFD00000000000000ULL,
			0xFDFD000000000000ULL,
			0xFDFDFD0000000000ULL,
			0xFDFDFDFD00000000ULL,
			0xFDFDFDFDFD000000ULL,
			0xFDFDFDFDFDFD0000ULL,
			0xFDFDFDFDFDFDFD00ULL
		},
		{
			0xFD00000000000000ULL,
			0xFDFD000000000000ULL,
			0xFDFDFD0000000000ULL,
			0xFDFDFDFD00000000ULL,
			0xFDFDFDFDFD000000ULL,
			0xFDFDFDFDFDFD0000ULL,
			0xFDFDFDFDFDFDFD00ULL,
			0xFDFDFDFDFDFDFDFDULL
		},
		{
			0xFDFD000000000000ULL,
			0xFDFDFD0000000000ULL,
			0xFDFDFDFD00000000ULL,
			0xFDFDFDFDFD000000ULL,
			0xFDFDFDFDFDFD0000ULL,
			0xFDFDFDFDFDFDFD00ULL,
			0xFDFDFDFDFDFDFDFDULL,
			0xFDFDFDFDFDFDFDFDULL
		},
		{
			0xFDFDFD0000000000ULL,
			0xFDFDFDFD00000000ULL,
			0xFDFDFDFDFD000000ULL,
			0xFDFDFDFDFDFD0000ULL,
			0xFDFDFDFDFDFDFD00ULL,
			0xFDFDFDFDFDFDFDFDULL,
			0xFDFDFDFDFDFDFDFDULL,
			0xFDFDFDFDFDFDFDFDULL
		},
		{
			0xFDFDFDFD00000000ULL,
			0xFDFDFDFDFD000000ULL,
			0xFDFDFDFDFDFD0000ULL,
			0xFDFDFDFDFDFDFD00ULL,
			0xFDFDFDFDFDFDFDFDULL,
			0xFDFDFDFDFDFDFDFDULL,
			0xFDFDFDFDFDFDFDFDULL,
			0xFDFDFDFDFDFDFDFDULL
		},
		{
			0xFDFDFDFDFD000000ULL,
			0xFDFDFDFDFDFD0000ULL,
			0xFDFDFDFDFDFDFD00ULL,
			0xFDFDFDFDFDFDFDFDULL,
			0xFDFDFDFDFDFDFDFDULL,
			0xFDFDFDFDFDFDFDFDULL,
			0xFDFDFDFDFDFDFDFDULL,
			0xFDFDFDFDFDFDFDFDULL
		},
		{
			0xFDFDFDFDFDFD0000ULL,
			0xFDFDFDFDFDFDFD00ULL,
			0xFDFDFDFDFDFDFDFDULL,
			0xFDFDFDFDFDFDFDFDULL,
			0xFDFDFDFDFDFDFDFDULL,
			0xFDFDFDFDFDFDFDFDULL,
			0xFDFDFDFDFDFDFDFDULL,
			0xFDFDFDFDFDFDFDFDULL
		},
		{
			0xFDFDFDFDFDFDFD00ULL,
			0xFDFDFDFDFDFDFDFDULL,
			0xFDFDFDFDFDFDFDFDULL,
			0xFDFDFDFDFDFDFDFDULL,
			0xFDFDFDFDFDFDFDFDULL,
			0xFDFDFDFDFDFDFDFDULL,
			0xFDFDFDFDFDFDFDFDULL,
			0xFDFDFDFDFDFDFDFDULL
		},
		{
			0xFDFDFDFDFDFDFDFDULL,
			0xFDFDFDFDFDFDFDFDULL,
			0xFDFDFDFDFDFDFDFDULL,
			0xFDFDFDFDFDFDFDFDULL,
			0xFDFDFDFDFDFDFDFDULL,
			0xFDFDFDFDFDFDFDFDULL,
			0xFDFDFDFDFDFDFDFDULL,
			0xFDFDFDFDFDFDFDFDULL
		}
	},
	{
		{
			0x0000000000000000ULL,
			0x0000000000000000ULL,
			0x0000000000000000ULL,
			0x0000000000000000ULL,
			0x0000000000000000ULL,
			0x0000000000000000ULL,
			0x0000000000000000ULL,
			0x0000000000000000ULL
		},
		{
			0x0000000000000000ULL,
			0x0000000000000000ULL,
			0x0000000000000000ULL,
			0x0000000000000000ULL,
			0x0000000000000000ULL,
			0x0000000000000000ULL,
			0x0000000000000000ULL,
			0x00000000000000FDULL
		},
		{
			0x0000000000000000ULL,
			0x0000000000000000ULL,
			0x0000000000000000ULL,
			0x0000000000000000ULL,
			0x0000000000000000ULL,
			0x0000000000000000ULL,
			0x00000000000000FDULL,
			0x000000000000FDFDULL
		},
		{
			0x0000000000000000ULL,
			0x0000000000000000ULL,
			0x0000000000000000ULL,
			0x0000000000000000ULL,
			0x0000000000000000ULL,
			0x00000000000000FDULL,
			0x000000000000FDFDULL,
			0x0000000000FDFDFDULL
		},
		{
			0x0000000000000000ULL,
			0x0000000000000000ULL,
			0x0000000000000000ULL,
			0x0000000000000000ULL,
			0x00000000000000FDULL,
			0x000000000000FDFDULL,
			0x0000000000FDFDFDULL,
			0x00000000FDFDFDFDULL
		},
		{
			0x0000000000000000ULL,
			0x0000000000000000ULL,
			0x0000000000000000ULL,
			0x00000000000000FDULL,
			0x000000000000FDFDULL,
			0x0000000000FDFDFDULL,
			0x00000000FDFDFDFDULL,
			0x000000FDFDFDFDFDULL
		},
		{
			0x0000000000000000ULL,
			0x0000000000000000ULL,
			0x00000000000000FDULL,
			0x000000000000FDFDULL,
			0x0000000000FDFDFDULL,
			0x00000000FDFDFDFDULL,
			0x000000FDFDFDFDFDULL,
			0x0000FDFDFDFDFDFDULL
		},
		{
			0x0000000000000000ULL,
			0x00000000000000FDULL,
			0x000000000000FDFDULL,
			0x0000000000FDFDFDULL,
			0x00000000FDFDFDFDULL,
			0x000000FDFDFDFDFDULL,
			0x0000FDFDFDFDFDFDULL,
			0x00FDFDFDFDFDFDFDULL
		},
		{
			0x00000000000000FDULL,
			0x000000000000FDFDULL,
			0x0000000000FDFDFDULL,
			0x00000000FDFDFDFDULL,
			0x000000FDFDFDFDFDULL,
			0x0000FDFDFDFDFDFDULL,
			0x00FDFDFDFDFDFDFDULL,
			0xFDFDFDFDFDFDFDFDULL
		},
		{
			0x000000000000FDFDULL,
			0x0000000000FDFDFDULL,
			0x00000000FDFDFDFDULL,
			0x000000FDFDFDFDFDULL,
			0x0000FDFDFDFDFDFDULL,
			0x00FDFDFDFDFDFDFDULL,
			0xFDFDFDFDFDFDFDFDULL,
			0xFDFDFDFDFDFDFDFDULL
		},
		{
			0x0000000000FDFDFDULL,
			0x00000000FDFDFDFDULL,
			0x000000FDFDFDFDFDULL,
			0x0000FDFDFDFDFDFDULL,
			0x00FDFDFDFDFDFDFDULL,
			0xFDFDFDFDFDFDFDFDULL,
			0xFDFDFDFDFDFDFDFDULL,
			0xFDFDFDFDFDFDFDFDULL
		},
		{
			0x00000000FDFDFDFDULL,
			0x000000FDFDFDFDFDULL,
			0x0000FDFDFDFDFDFDULL,
			0x00FDFDFDFDFDFDFDULL,
			0xFDFDFDFDFDFDFDFDULL,
			0xFDFDFDFDFDFDFDFDULL,
			0xFDFDFDFDFDFDFDFDULL,
			0xFDFDFDFDFDFDFDFDULL
		},
		{
			0x000000FDFDFDFDFDULL,
			0x0000FDFDFDFDFDFDULL,
			0x00FDFDFDFDFDFDFDULL,
			0xFDFDFDFDFDFDFDFDULL,
			0xFDFDFDFDFDFDFDFDULL,
			0xFDFDFDFDFDFDFDFDULL,
			0xFDFDFDFDFDFDFDFDULL,
			0xFDFDFDFDFDFDFDFDULL
		},
		{
			0x0000FDFDFDFDFDFDULL,
			0x00FDFDFDFDFDFDFDULL,
			0xFDFDFDFDFDFDFDFDULL,
			0xFDFDFDFDFDFDFDFDULL,
			0xFDFDFDFDFDFDFDFDULL,
			0xFDFDFDFDFDFDFDFDULL,
			0xFDFDFDFDFDFDFDFDULL,
			0xFDFDFDFDFDFDFDFDULL
		},
		{
			0x00FDFDFDFDFDFDFDULL,
			0xFDFDFDFDFDFDFDFDULL,
			0xFDFDFDFDFDFDFDFDULL,
			0xFDFDFDFDFDFDFDFDULL,
			0xFDFDFDFDFDFDFDFDULL,
			0xFDFDFDFDFDFDFDFDULL,
			0xFDFDFDFDFDFDFDFDULL,
			0xFDFDFDFDFDFDFDFDULL
		},
		{
			0xFDFDFDFDFDFDFDFDULL,
			0xFDFDFDFDFDFDFDFDULL,
			0xFDFDFDFDFDFDFDFDULL,
			0xFDFDFDFDFDFDFDFDULL,
			0xFDFDFDFDFDFDFDFDULL,
			0xFDFDFDFDFDFDFDFDULL,
			0xFDFDFDFDFDFDFDFDULL,
			0xFDFDFDFDFDFDFDFDULL
		}
	}
};

int32_t black_opening_count=0;
int32_t black_opening_x,black_opening_y;
int32_t black_opening_shape;

int32_t choose_opening_shape()
{
	// First, count how many bits are set
	int32_t numBits=0;
	int32_t bitCounter;
	
	for(int32_t i=0; i<bosMAX; i++)
	{
		if(COOLSCROLL&(1<<i))
			numBits++;
	}
	
	// Shouldn't happen...
	if(numBits==0)
		return bosCIRCLE;
		
	// Pick a bit
	bitCounter=zc_rand()%numBits+1;
	
	for(int32_t i=0; i<bosMAX; i++)
	{
		// If this bit is set, decrement the bit counter
		if(COOLSCROLL&(1<<i))
			bitCounter--;
			
		// When the counter hits 0, return a value based on
		// which bit it stopped on.
		// Reminder: enum {bosCIRCLE=0, bosOVAL, bosTRIANGLE, bosSMAS, bosFADEBLACK, bosMAX};
		if(bitCounter==0)
			return i;
	}
	
	// Shouldn't be necessary, but the compiler might complain, at least
	return bosCIRCLE;
}

void close_black_opening(int32_t x, int32_t y, bool wait, int32_t shape)
{
	black_opening_shape= (shape>-1 ? shape : choose_opening_shape());
	
	int32_t w=256, h=224;
	int32_t blockrows=28, blockcolumns=32;
	int32_t xoffset=(x-(w/2))/8, yoffset=(y-(h/2))/8;
	
	for(int32_t blockrow=0; blockrow<blockrows; ++blockrow)  //30
	{
		for(int32_t blockcolumn=0; blockcolumn<blockcolumns; ++blockcolumn)  //40
		{
			screen_triangles[blockrow][blockcolumn]=zc_max(abs(int32_t(double(blockcolumns-1)/2-blockcolumn+xoffset)),abs(int32_t(double(blockrows-1)/2-blockrow+yoffset)))|0x0100|((blockrow-yoffset<blockrows/2)?0:0x8000)|((blockcolumn-xoffset<blockcolumns/2)?0x4000:0);
		}
	}
	
	black_opening_count = 66;
	black_opening_x = x;
	black_opening_y = y;
	lensclk = 0;
	//black_opening_shape=(black_opening_shape+1)%bosMAX;
	
	
	if(black_opening_shape == bosFADEBLACK)
	{
		refreshTints();
		memcpy(tempblackpal, RAMpal, sizeof(RAMpal)); //Store palette in temp palette for fade effect
	}
	if(wait)
	{
		FFCore.warpScriptCheck();
		for(int32_t i=0; i<66; i++)
		{
			draw_screen(tmpscr);
			//put_passive_subscr(framebuf,&QMisc,0,passive_subscreen_offset,false,sspUP);
			syskeys();
			advanceframe(true);
			
			if(Quit)
			{
				break;
			}
		}
	}
}

void open_black_opening(int32_t x, int32_t y, bool wait, int32_t shape)
{
	black_opening_shape= (shape>-1 ? shape : choose_opening_shape());
	
	int32_t w=256, h=224;
	int32_t blockrows=28, blockcolumns=32;
	int32_t xoffset=(x-(w/2))/8, yoffset=(y-(h/2))/8;
	
	for(int32_t blockrow=0; blockrow<blockrows; ++blockrow)  //30
	{
		for(int32_t blockcolumn=0; blockcolumn<blockcolumns; ++blockcolumn)  //40
		{
			screen_triangles[blockrow][blockcolumn]=zc_max(abs(int32_t(double(blockcolumns-1)/2-blockcolumn+xoffset)),abs(int32_t(double(blockrows-1)/2-blockrow+yoffset)))|0x0100|((blockrow-yoffset<blockrows/2)?0:0x8000)|((blockcolumn-xoffset<blockcolumns/2)?0x4000:0);
		}
	}
	
	black_opening_count = -66;
	black_opening_x = x;
	black_opening_y = y;
	lensclk = 0;
	if(black_opening_shape == bosFADEBLACK)
	{
		refreshTints();
		memcpy(tempblackpal, RAMpal, sizeof(RAMpal)); //Store palette in temp palette for fade effect
	}
	if(wait)
	{
		FFCore.warpScriptCheck();
		for(int32_t i=0; i<66; i++)
		{
			draw_screen(tmpscr);
			//put_passive_subscr(framebuf,&QMisc,0,passive_subscreen_offset,false,sspUP);
			syskeys();
			advanceframe(true);
			
			if(Quit)
			{
				break;
			}
		}
	}
}

void black_opening(BITMAP *dest,int32_t x,int32_t y,int32_t a,int32_t max_a)
{
	clear_to_color(tmp_scr,BLACK);
	int32_t w=256, h=224;
	
	switch(black_opening_shape)
	{
	case bosOVAL:
	{
		double new_w=(w/2)+abs(w/2-x);
		double new_h=(h/2)+abs(h/2-y);
		double b=sqrt(((new_w*new_w)/4)+(new_h*new_h));
		ellipsefill(tmp_scr,x,y,int32_t(2*a*b/max_a)/8*8,int32_t(a*b/max_a)/8*8,0);
		break;
	}
	
	case bosTRIANGLE:
	{
		double new_w=(w/2)+abs(w/2-x);
		double new_h=(h/2)+abs(h/2-y);
		double r=a*(new_w*sqrt((double)3)+new_h)/max_a;
		double P2= (PI/2);
		double P23=(2*PI/3);
		double P43=(4*PI/3);
		double Pa= (-4*PI*a/(3*max_a));
		double angle=P2+Pa;
		double a0=angle;
		double a2=angle+P23;
		double a4=angle+P43;
		triangle(tmp_scr, x+int32_t(zc::math::Cos(a0)*r), y-int32_t(zc::math::Sin(a0)*r),
				 x+int32_t(zc::math::Cos(a2)*r), y-int32_t(zc::math::Sin(a2)*r),
				 x+int32_t(zc::math::Cos(a4)*r), y-int32_t(zc::math::Sin(a4)*r),
				 0);
		break;
	}
	
	case bosSMAS:
	{
		int32_t distance=zc_max(abs(w/2-x),abs(h/2-y))/8;
		
		for(int32_t blockrow=0; blockrow<28; ++blockrow)  //30
		{
			for(int32_t linerow=0; linerow<8; ++linerow)
			{
				qword *triangleline=(qword*)(tmp_scr->line[(blockrow*8+linerow)]);
				
				for(int32_t blockcolumn=0; blockcolumn<32; ++blockcolumn)  //40
				{
					*triangleline=triangles[(screen_triangles[blockrow][blockcolumn]&0xC000)>>14]
								  [zc_min(zc_max((((31+distance)*(max_a-a)/max_a)+((screen_triangles[blockrow][blockcolumn]&0x0FFF)-0x0100)-(15+distance)),0),15)]
								  [linerow];
					++triangleline;
					
					if(linerow==0)
					{
					}
				}
			}
		}
		
		break;
	}
	
	case bosFADEBLACK:
	{
		if(black_opening_count<0)
		{
			black_fade(zc_min(-black_opening_count,63));
		}
		else if(black_opening_count>0)
		{
			black_fade(63-zc_max(black_opening_count-3,0));
		}
		else black_fade(0);
		return; //no blitting from tmp_scr!
	}
	
	case bosCIRCLE:
	default:
	{
		double new_w=(w/2)+abs(w/2-x);
		double new_h=(h/2)+abs(h/2-y);
		int32_t r=int32_t(sqrt((new_w*new_w)+(new_h*new_h))*a/max_a);
		//circlefill(tmp_scr,x,y,a<<3,0);
		circlefill(tmp_scr,x,y,r,0);
		break;
	}
	}
	
	masked_blit(tmp_scr,dest,0,0,0,0,320,240);
}


void black_fade(int32_t fadeamnt)
{
	for(int32_t i=0; i < 0xEF; i++)
	{
		RAMpal[i].r = vbound(tempblackpal[i].r-fadeamnt,0,63);
		RAMpal[i].g = vbound(tempblackpal[i].g-fadeamnt,0,63);
		RAMpal[i].b = vbound(tempblackpal[i].b-fadeamnt,0,63);
	}
	
	refreshpal = true;
}

//----------------------------------------------------------------

bool item_disabled(int32_t item)				 //is this item disabled?
{
	return (item>=0 && game->items_off[item] != 0);
}

bool can_use_item(int32_t item_type, int32_t item)				  //can Hero use this item?
{
	if(current_item(item_type, true) >=item)
	{
		return true;
	}
	
	return false;
}

bool has_item(int32_t item_type, int32_t it)						//does Hero possess this item?
{
	switch(item_type)
	{
		case itype_bomb:
		case itype_sbomb:
		{
			int32_t itemid = getItemID(itemsbuf, item_type, it);
			
			if(itemid == -1)
				return false;
				
			return (game->get_item(itemid));
		}
		
		case itype_clock:
		{
			int32_t itemid = getItemID(itemsbuf, item_type, it);
			
			if(itemid != -1 && (itemsbuf[itemid].flags & ITEM_FLAG1)) //Active clock
				return (game->get_item(itemid));
			return Hero.getClock()?1:0;
		}
			
		case itype_key:
			return (game->get_keys()>0);
			
		case itype_magiccontainer:
			return (game->get_maxmagic()>=game->get_mp_per_block());
			
		case itype_triforcepiece:							   //it: -2=any, -1=current level, other=that level
		{
			switch(it)
			{
				case -2:
				{
					for(int32_t i=0; i<MAXLEVELS; i++)
					{
						if(game->lvlitems[i]&liTRIFORCE)
						{
							return true;
						}
					}
					
					return false;
				}
				
				case -1:
					return (game->lvlitems[dlevel]&liTRIFORCE);
					
				default:
					if(it>=0&&it<MAXLEVELS)
					{
						return (game->lvlitems[it]&liTRIFORCE);
					}
					
					break;
			}
			
			return 0;
		}
		
		case itype_map:										 //it: -2=any, -1=current level, other=that level
		{
			switch(it)
			{
				case -2:
				{
					for(int32_t i=0; i<MAXLEVELS; i++)
					{
						if(game->lvlitems[i]&liMAP)
						{
							return true;
						}
					}
					
					return false;
				}
				
				case -1:
					return (game->lvlitems[dlevel]&liMAP)!=0;
					
				default:
					if(it>=0&&it<MAXLEVELS)
					{
						return (game->lvlitems[it]&liMAP)!=0;
					}
					
					break;
			}
			
			return 0;
		}
		
		case itype_compass:									 //it: -2=any, -1=current level, other=that level
		{
			switch(it)
			{
				case -2:
				{
					for(int32_t i=0; i<MAXLEVELS; i++)
					{
						if(game->lvlitems[i]&liCOMPASS)
						{
							return true;
						}
					}
					
					return false;
				}
				
				case -1:
					return (game->lvlitems[dlevel]&liCOMPASS)!=0;
					
				default:
					if(it>=0&&it<MAXLEVELS)
					{
						return (game->lvlitems[it]&liCOMPASS)!=0;
					}
					
					break;
			}
			return 0;
		}
		
		case itype_bosskey:									 //it: -2=any, -1=current level, other=that level
		{
			switch(it)
			{
				case -2:
				{
					for(int32_t i=0; i<MAXLEVELS; i++)
					{
						if(game->lvlitems[i]&liBOSSKEY)
						{
							return true;
						}
					}
					
					return false;
				}
				
				case -1:
					return (game->lvlitems[dlevel]&liBOSSKEY)?1:0;
					
				default:
					if(it>=0&&it<MAXLEVELS)
					{
						return (game->lvlitems[it]&liBOSSKEY)?1:0;
					}
					break;
			}
			return 0;
		}
		
		default:
			//it=(1<<(it-1));
			/*if (item_type>=itype_max)
			{
			  system_pal();
			  jwin_alert("Error","has_item exception",NULL,NULL,"O&K",NULL,'k',0,lfont);
			  game_pal();
			
			  return false;
			}*/
			int32_t itemid = getItemID(itemsbuf, item_type, it);
			
			if(itemid == -1)
				return false;
				
			return game->get_item(itemid);
	}
}


int32_t current_item(int32_t item_type, bool checkenabled)		   //item currently being used
{
	switch(item_type)
	{
		case itype_clock:
		{
			int32_t maxid = getHighestLevelOfFamily(game, itemsbuf, item_type, checkenabled);
			
			if(maxid != -1 && (itemsbuf[maxid].flags & ITEM_FLAG1)) //Active clock
				return itemsbuf[maxid].fam_type;
			
			return has_item(itype_clock,1) ? 1 : 0;
		}
			
		case itype_key:
			return game->get_keys();
			
		case itype_lkey:
			return game->lvlkeys[get_dlevel()];
			
		case itype_magiccontainer:
			return game->get_maxmagic()/game->get_mp_per_block();
			
		case itype_triforcepiece:
		{
			int32_t count=0;
			
			for(int32_t i=0; i<MAXLEVELS; i++)
			{
				count+=(game->lvlitems[i]&liTRIFORCE)?1:0;
			}
			
			return count;
		}
		
		case itype_map:
		{
			int32_t count=0;
			
			for(int32_t i=0; i<MAXLEVELS; i++)
			{
				count+=(game->lvlitems[i]&liMAP)?1:0;
			}
			
			return count;
		}
		
		case itype_compass:
		{
			int32_t count=0;
			
			for(int32_t i=0; i<MAXLEVELS; i++)
			{
				count+=(game->lvlitems[i]&liCOMPASS)?1:0;
			}
			
			return count;
		}
		
		case itype_bosskey:
		{
			int32_t count=0;
			
			for(int32_t i=0; i<MAXLEVELS; i++)
			{
				count+=(game->lvlitems[i]&liBOSSKEY)?1:0;
			}
			
			return count;
		}
		
		default:
			int32_t maxid = getHighestLevelOfFamily(game, itemsbuf, item_type, checkenabled);
			
			if(maxid == -1)
				return 0;
				
			return itemsbuf[maxid].fam_type;
	}
}

int32_t current_item(int32_t item_type)		   //item currently being used
{
	return current_item(item_type, true);
}

std::map<int32_t, int32_t> itemcache;

// Not actually used by anything at the moment...
void removeFromItemCache(int32_t itemid)
{
	itemcache.erase(itemid);
}

void flushItemCache()
{
	itemcache.clear();
	
	//also fix the active subscreen if items were deleted -DD
	if(game != NULL)
	{
		verifyBothWeapons();
		load_Sitems(&QMisc);
	}
}

// This is used often, so it should be as direct as possible.
int32_t _c_item_id_internal(int32_t itemtype, bool checkmagic, bool jinx_check)
{
	if(jinx_check)
	{
		if(!(HeroSwordClk() || HeroItemClk()))
			jinx_check = false; //not jinxed
	}
	if(itemtype!=itype_ring && !jinx_check)  // Rings must always be checked, as must jinx checks...
	{
		std::map<int32_t,int32_t>::iterator res = itemcache.find(itemtype);
		
		if(res != itemcache.end())
			return res->second;
	}
	
	int32_t result = -1;
	int32_t highestlevel = -1;
	
	for(int32_t i=0; i<MAXITEMS; i++)
	{
		if(game->get_item(i) && itemsbuf[i].family==itemtype && !item_disabled(i))
		{
			if((checkmagic || itemtype == itype_ring) && itemtype != itype_magicring)
			{
				//printf("Checkmagic for %d: %d (%d %d)\n",i,checkmagiccost(i),itemsbuf[i].magic*game->get_magicdrainrate(),game->get_magic());
				if(!checkmagiccost(i))
				{
					if ( !get_bit(quest_rules,qr_NEVERDISABLEAMMOONSUBSCREEN) ) continue; //don't make items with a magic cost vanish!! -Z
				}
			}
			if(jinx_check && (usesSwordJinx(i) ? HeroSwordClk() : HeroItemClk()))
			{
				if(!(itemsbuf[i].flags & ITEM_JINX_IMMUNE))
					continue;
			}
			
			if(itemsbuf[i].fam_type >= highestlevel)
			{
				highestlevel = itemsbuf[i].fam_type;
				result=i;
			}
		}
	}
	
	if(!jinx_check) //Can't cache jinx_check results
		itemcache[itemtype] = result;
	return result;
}

// 'jinx_check' indicates that the highest level item *immune to jinxes* should be returned.
int32_t current_item_id(int32_t itemtype, bool checkmagic, bool jinx_check)
{
	auto ret = _c_item_id_internal(itemtype,checkmagic,jinx_check);
	if(!jinx_check) //If not already a jinx-immune-only check...
	{
		//And the player IS jinxed...
		if(HeroSwordClk() || HeroItemClk())
		{
			//Then do a jinx-immune-only check here
			auto ret2 = _c_item_id_internal(itemtype,checkmagic,true);
			//And *IF IT FINDS A VALID ITEM*, return that one instead! -Em
			//Should NOT need a compat rule, as this should always return -1 in old quests.
			if(ret2 > -1) return ret2;
		}
	}
	return ret;
}
int32_t current_item_power(int32_t itemtype)
{
	int32_t result = current_item_id(itemtype,true);
	return (result<0) ? 0 : itemsbuf[result].power;
}

int32_t heart_container_id()
{
	for(int32_t i=0; i<MAXITEMS; i++)
	{
		if(itemsbuf[i].family == itype_heartcontainer)
		{
			return i;
		}
	}
	return -1;
}

int32_t item_tile_mod()
{
	int32_t tile=0;
	
	if(game->get_bombs())
	{
		int32_t itemid = current_item_id(itype_bomb,false);
		if(itemid > -1 && checkbunny(itemid))
			tile+=itemsbuf[itemid].ltm;
	}
	
	if(game->get_sbombs())
	{
		int32_t itemid = current_item_id(itype_sbomb,false);
		if(itemid > -1 && checkbunny(itemid))
			tile+=itemsbuf[itemid].ltm;
	}
	
	if(current_item(itype_clock))
	{
		int32_t itemid =
			get_bit(quest_rules, qr_HARDCODED_LITEM_LTMS)
				? iClock
				: getHighestLevelEvenUnowned(itemsbuf, itype_clock);
		if(itemid > -1 && checkbunny(itemid))
			tile+=itemsbuf[itemid].ltm;
	}
	
	if(current_item(itype_key))
	{
		int32_t itemid =
			get_bit(quest_rules, qr_HARDCODED_LITEM_LTMS)
				? iKey
				: getHighestLevelEvenUnowned(itemsbuf, itype_key);
		if(itemid > -1 && checkbunny(itemid))
			tile+=itemsbuf[itemid].ltm;
	}
	
	if(current_item(itype_lkey))
	{
		int32_t itemid =
			get_bit(quest_rules, qr_HARDCODED_LITEM_LTMS)
				? iLevelKey
				: getHighestLevelEvenUnowned(itemsbuf, itype_lkey);
		if(itemid > -1 && checkbunny(itemid))
			tile+=itemsbuf[itemid].ltm;
	}
	
	if(current_item(itype_map))
	{
		int32_t itemid =
			get_bit(quest_rules, qr_HARDCODED_LITEM_LTMS)
				? iMap
				: getHighestLevelEvenUnowned(itemsbuf, itype_map);
		if(itemid > -1 && checkbunny(itemid))
			tile+=itemsbuf[itemid].ltm;
	}
	
	if(current_item(itype_compass))
	{
		int32_t itemid =
			get_bit(quest_rules, qr_HARDCODED_LITEM_LTMS)
				? iCompass
				: getHighestLevelEvenUnowned(itemsbuf, itype_compass);
		if(itemid > -1 && checkbunny(itemid))
			tile+=itemsbuf[itemid].ltm;
	}
	
	if(current_item(itype_bosskey))
	{
		int32_t itemid =
			get_bit(quest_rules, qr_HARDCODED_LITEM_LTMS)
				? iBossKey
				: getHighestLevelEvenUnowned(itemsbuf, itype_bosskey);
		if(itemid > -1 && checkbunny(itemid))
			tile+=itemsbuf[itemid].ltm;
	}
	
	if(current_item(itype_magiccontainer))
	{
		int32_t itemid =
			get_bit(quest_rules, qr_HARDCODED_LITEM_LTMS)
				? iMagicC
				: getHighestLevelEvenUnowned(itemsbuf, itype_magiccontainer);
		if(itemid > -1 && checkbunny(itemid))
			tile+=itemsbuf[itemid].ltm;
	}
	
	if(current_item(itype_triforcepiece))
	{
		int32_t itemid =
			get_bit(quest_rules, qr_HARDCODED_LITEM_LTMS)
				? iTriforce
				: getHighestLevelEvenUnowned(itemsbuf, itype_triforcepiece);
		if(itemid > -1 && checkbunny(itemid))
			tile+=itemsbuf[itemid].ltm;
	}
	
	for(int32_t i=0; i<itype_max; i++)
	{
		if(!get_bit(quest_rules, qr_HARDCODED_LITEM_LTMS))
		{
			switch(i)
			{
				case itype_bomb:
				case itype_sbomb:
				case itype_clock:
				case itype_key:
				case itype_lkey:
				case itype_map:
				case itype_compass:
				case itype_bosskey:
				case itype_magiccontainer:
				case itype_triforcepiece:
					continue; //already handled
			}
		}
		int32_t itemid = current_item_id(i,false);
		if(i == itype_shield)
			itemid = getCurrentShield(false);
		
		if(itemid < 0 || !checkbunny(itemid))
			continue;
		
		itemdata const& itm = itemsbuf[itemid];
		
		switch(itm.family)
		{
			case itype_shield:
				if(itm.flags & ITEM_FLAG9) //active shield
				{
					if(!usingActiveShield(itemid))
					{
						tile+=itm.misc6; //'Inactive PTM'
						continue;
					}
				}
				break;
		}
		
		tile+=itm.ltm;
	}
	
	return tile;
}

int32_t bunny_tile_mod()
{
	if(Hero.BunnyClock())
	{
		return game->get_bunny_ltm();
	}
	return 0;
}

// Hints are drawn on a separate layer to combo reveals.
void draw_lens_under(BITMAP *dest, bool layer)
{
	//Lens flag 1: Replacement for qr_LENSHINTS; if set, lens will show hints. Does nothing if flag 2 is set.
	//Lens flag 2: Disable "hints", prevent rendering of Secret Combos
	//Lens flag 3: Don't show armos/chest/dive items
	//Lens flag 4: Show Raft Paths
	//Lens flag 5: Show Invisible Enemies
	bool hints = (itemsbuf[Hero.getLastLensID()].flags & ITEM_FLAG2) ? false : (layer && (itemsbuf[Hero.getLastLensID()].flags & ITEM_FLAG1));
	
	int32_t strike_hint_table[11]=
	{
		mfARROW, mfBOMB, mfBRANG, mfWANDMAGIC,
		mfSWORD, mfREFMAGIC, mfHOOKSHOT,
		mfREFFIREBALL, mfHAMMER, mfSWORDBEAM, mfWAND
	};
	
	//  int32_t page = tmpscr->cpage;
	{
		int32_t blink_rate=((get_bit(quest_rules,qr_EPILEPSY) || epilepsyFlashReduction)?6:1);
		//	int32_t temptimer=0;
		int32_t tempitem, tempweapon=0;
		strike_hint=strike_hint_table[strike_hint_counter];
		
		if(strike_hint_timer>32)
		{
			strike_hint_timer=0;
			strike_hint_counter=((strike_hint_counter+1)%11);
		}
		
		++strike_hint_timer;
		
		for(int32_t i=0; i<176; i++)
		{
			int32_t x = (i & 15) << 4;
			int32_t y = (i & 0xF0) + playing_field_offset;
			int32_t tempitemx=-16, tempitemy=-16;
			int32_t tempweaponx=-16, tempweapony=-16;
			
			for(int32_t iter=0; iter<2; ++iter)
			{
				int32_t checkflag=0;
				
				if(iter==0)
				{
					checkflag=combobuf[tmpscr->data[i]].flag;
				}
				else
				{
					checkflag=tmpscr->sflag[i];
				}
				
				if(checkflag==mfSTRIKE)
				{
					if(!hints)
					{
						if(!(itemsbuf[Hero.getLastLensID()].flags & ITEM_FLAG2))putcombo(dest,x,y,tmpscr->secretcombo[sSTRIKE],tmpscr->secretcset[sSTRIKE]);
					}
					else
					{
						checkflag = strike_hint;
					}
				}
				
				switch(checkflag)
				{
					case 0:
					case mfZELDA:
					case mfPUSHED:
					case mfENEMY0:
					case mfENEMY1:
					case mfENEMY2:
					case mfENEMY3:
					case mfENEMY4:
					case mfENEMY5:
					case mfENEMY6:
					case mfENEMY7:
					case mfENEMY8:
					case mfENEMY9:
					case mfSINGLE:
					case mfSINGLE16:
					case mfNOENEMY:
					case mfTRAP_H:
					case mfTRAP_V:
					case mfTRAP_4:
					case mfTRAP_LR:
					case mfTRAP_UD:
					case mfNOGROUNDENEMY:
					case mfNOBLOCKS:
					case mfSCRIPT1:
					case mfSCRIPT2:
					case mfSCRIPT3:
					case mfSCRIPT4:
					case mfSCRIPT5:
					case mfSCRIPT6:
					case mfSCRIPT7:
					case mfSCRIPT8:
					case mfSCRIPT9:
					case mfSCRIPT10:
					case mfSCRIPT11:
					case mfSCRIPT12:
					case mfSCRIPT13:
					case mfSCRIPT14:
					case mfSCRIPT15:
					case mfSCRIPT16:
					case mfSCRIPT17:
					case mfSCRIPT18:
					case mfSCRIPT19:
					case mfSCRIPT20:
					case mfPITHOLE:
					case mfPITFALLFLOOR:
					case mfLAVA:
					case mfICE:
					case mfICEDAMAGE:
					case mfDAMAGE1:
					case mfDAMAGE2:
					case mfDAMAGE4:
					case mfDAMAGE8:
					case mfDAMAGE16:
					case mfDAMAGE32:
					case mfFREEZEALL:
					case mfFREZEALLANSFFCS:
					case mfFREEZEFFCSOLY:
					case mfSCRITPTW1TRIG:
					case mfSCRITPTW2TRIG:
					case mfSCRITPTW3TRIG:
					case mfSCRITPTW4TRIG:
					case mfSCRITPTW5TRIG:
					case mfSCRITPTW6TRIG:
					case mfSCRITPTW7TRIG:
					case mfSCRITPTW8TRIG:
					case mfSCRITPTW9TRIG:
					case mfSCRITPTW10TRIG:
					case mfTROWEL:
					case mfTROWELNEXT:
					case mfTROWELSPECIALITEM:
					case mfSLASHPOT:
					case mfLIFTPOT:
					case mfLIFTORSLASH:
					case mfLIFTROCK:
					case mfLIFTROCKHEAVY:
					case mfDROPITEM:
					case mfSPECIALITEM:
					case mfDROPKEY:
					case mfDROPLKEY:
					case mfDROPCOMPASS:
					case mfDROPMAP:
					case mfDROPBOSSKEY:
					case mfSPAWNNPC:
					case mfSWITCHHOOK:
					case mfSIDEVIEWLADDER:
					case mfSIDEVIEWPLATFORM:
					case mfNOENEMYSPAWN:
					case mfENEMYALL:
					case mfNOMIRROR:
					case mfUNSAFEGROUND:
					case mf168:
					case mf169:
					case mf170:
					case mf171:
					case mf172:
					case mf173:
					case mf174:
					case mf175:
					case mf176:
					case mf177:
					case mf178:
					case mf179:
					case mf180:
					case mf181:
					case mf182:
					case mf183:
					case mf184:
					case mf185:
					case mf186:
					case mf187:
					case mf188:
					case mf189:
					case mf190:
					case mf191:
					case mf192:
					case mf193:
					case mf194:
					case mf195:
					case mf196:
					case mf197:
					case mf198:
					case mf199:
					case mf200:
					case mf201:
					case mf202:
					case mf203:
					case mf204:
					case mf205:
					case mf206:
					case mf207:
					case mf208:
					case mf209:
					case mf210:
					case mf211:
					case mf212:
					case mf213:
					case mf214:
					case mf215:
					case mf216:
					case mf217:
					case mf218:
					case mf219:
					case mf220:
					case mf221:
					case mf222:
					case mf223:
					case mf224:
					case mf225:
					case mf226:
					case mf227:
					case mf228:
					case mf229:
					case mf230:
					case mf231:
					case mf232:
					case mf233:
					case mf234:
					case mf235:
					case mf236:
					case mf237:
					case mf238:
					case mf239:
					case mf240:
					case mf241:
					case mf242:
					case mf243:
					case mf244:
					case mf245:
					case mf246:
					case mf247:
					case mf248:
					case mf249:
					case mf250:
					case mf251:
					case mf252:
					case mf253:
					case mf254:
					case mfEXTENDED:
						break;
						
					case mfPUSHUD:
					case mfPUSHLR:
					case mfPUSH4:
					case mfPUSHU:
					case mfPUSHD:
					case mfPUSHL:
					case mfPUSHR:
					case mfPUSHUDNS:
					case mfPUSHLRNS:
					case mfPUSH4NS:
					case mfPUSHUNS:
					case mfPUSHDNS:
					case mfPUSHLNS:
					case mfPUSHRNS:
					case mfPUSHUDINS:
					case mfPUSHLRINS:
					case mfPUSH4INS:
					case mfPUSHUINS:
					case mfPUSHDINS:
					case mfPUSHLINS:
					case mfPUSHRINS:
						if(!hints && ((!(get_debug() && zc_getkey(KEY_N)) && (lensclk&16))
									  || ((get_debug() && zc_getkey(KEY_N)) && (frame&16))))
						{
							if(!(itemsbuf[Hero.getLastLensID()].flags & ITEM_FLAG2))putcombo(dest,x,y,tmpscr->undercombo,tmpscr->undercset);
						}
						
						if((!(get_debug() && zc_getkey(KEY_N)) && (lensclk&blink_rate))
								|| ((get_debug() && zc_getkey(KEY_N)) && (frame&blink_rate)))
						{
							if(hints)
							{
								switch(combobuf[tmpscr->data[i]].type)
								{
								case cPUSH_HEAVY:
								case cPUSH_HW:
									tempitem=getItemIDPower(itemsbuf,itype_bracelet,1);
									tempitemx=x, tempitemy=y;
									
									if(tempitem>-1)
										putitem2(dest,tempitemx,tempitemy,tempitem, lens_hint_item[tempitem][0], lens_hint_item[tempitem][1], 0);
										
									break;
									
								case cPUSH_HEAVY2:
								case cPUSH_HW2:
									tempitem=getItemIDPower(itemsbuf,itype_bracelet,2);
									tempitemx=x, tempitemy=y;
									
									if(tempitem>-1)
										putitem2(dest,tempitemx,tempitemy,tempitem, lens_hint_item[tempitem][0], lens_hint_item[tempitem][1], 0);
										
									break;
								}
							}
						}
						
						break;
						
					case mfWHISTLE:
						if(hints)
						{
							tempitem=getItemID(itemsbuf,itype_whistle,1);
							
							if(tempitem<0) break;
							
							if((!(get_debug() && zc_getkey(KEY_N)) && (lensclk&blink_rate))
									|| ((get_debug() && zc_getkey(KEY_N)) && (frame&blink_rate)))
							{
								tempitemx=x;
								tempitemy=y;
							}
							
							putitem2(dest,tempitemx,tempitemy,tempitem, lens_hint_item[tempitem][0], lens_hint_item[tempitem][1], 0);
						}
						
						break;
						
						//Why is this here?
					case mfFAIRY:
					case mfMAGICFAIRY:
					case mfALLFAIRY:
						if(hints)
						{
							tempitem=getItemID(itemsbuf, itype_fairy,1);//iFairyMoving;
							
							if(tempitem < 0) break;
							
							if((!(get_debug() && zc_getkey(KEY_N)) && (lensclk&blink_rate))
									|| ((get_debug() && zc_getkey(KEY_N)) && (frame&blink_rate)))
							{
								tempitemx=x;
								tempitemy=y;
							}
							
							putitem2(dest,tempitemx,tempitemy,tempitem, lens_hint_item[tempitem][0], lens_hint_item[tempitem][1], 0);
						}
						
						break;
						
					case mfBCANDLE:
						if(!hints)
						{
							if(!(itemsbuf[Hero.getLastLensID()].flags & ITEM_FLAG2))putcombo(dest,x,y,tmpscr->secretcombo[sBCANDLE],tmpscr->secretcset[sBCANDLE]);
						}
						else
						{
							tempitem=getItemID(itemsbuf,itype_candle,1);
							
							if(tempitem<0) break;
							
							if((!(get_debug() && zc_getkey(KEY_N)) && (lensclk&blink_rate))
									|| ((get_debug() && zc_getkey(KEY_N)) && (frame&blink_rate)))
							{
								tempitemx=x;
								tempitemy=y;
							}
							
							putitem2(dest,tempitemx,tempitemy,tempitem, lens_hint_item[tempitem][0], lens_hint_item[tempitem][1], 0);
						}
						
						break;
						
					case mfRCANDLE:
						if(!hints)
						{
							if(!(itemsbuf[Hero.getLastLensID()].flags & ITEM_FLAG2))putcombo(dest,x,y,tmpscr->secretcombo[sRCANDLE],tmpscr->secretcset[sRCANDLE]);
						}
						else
						{
							tempitem=getItemID(itemsbuf,itype_candle,2);
							
							if(tempitem<0) break;
							
							if((!(get_debug() && zc_getkey(KEY_N)) && (lensclk&blink_rate))
									|| ((get_debug() && zc_getkey(KEY_N)) && (frame&blink_rate)))
							{
								tempitemx=x;
								tempitemy=y;
							}
							
							putitem2(dest,tempitemx,tempitemy,tempitem, lens_hint_item[tempitem][0], lens_hint_item[tempitem][1], 0);
						}
						
						break;
						
					case mfWANDFIRE:
						if(!hints)
						{
							if(!(itemsbuf[Hero.getLastLensID()].flags & ITEM_FLAG2))putcombo(dest,x,y,tmpscr->secretcombo[sWANDFIRE],tmpscr->secretcset[sWANDFIRE]);
						}
						else
						{
							tempitem=getItemID(itemsbuf,itype_wand,1);
							
							if(tempitem<0) break;
							
							tempweapon=wFire;
							
							if((!(get_debug() && zc_getkey(KEY_N)) && (lensclk&blink_rate))
									|| ((get_debug() && zc_getkey(KEY_N)) && (frame&blink_rate)))
							{
								tempitemx=x;
								tempitemy=y;
							}
							else
							{
								tempweaponx=x;
								tempweapony=y;
							}
							
							putweapon(dest,tempweaponx,tempweapony,tempweapon, 0, up, lens_hint_weapon[tempweapon][0], lens_hint_weapon[tempweapon][1],-1);
							putitem2(dest,tempitemx,tempitemy,tempitem, lens_hint_item[tempitem][0], lens_hint_item[tempitem][1], 0);
						}
						
						break;
						
					case mfDINSFIRE:
						if(!hints)
						{
							if(!(itemsbuf[Hero.getLastLensID()].flags & ITEM_FLAG2))putcombo(dest,x,y,tmpscr->secretcombo[sDINSFIRE],tmpscr->secretcset[sDINSFIRE]);
						}
						else
						{
							tempitem=getItemID(itemsbuf,itype_dinsfire,1);
							
							if(tempitem<0) break;
							
							if((!(get_debug() && zc_getkey(KEY_N)) && (lensclk&blink_rate))
									|| ((get_debug() && zc_getkey(KEY_N)) && (frame&blink_rate)))
							{
								tempitemx=x;
								tempitemy=y;
							}
							
							putitem2(dest,tempitemx,tempitemy,tempitem, lens_hint_item[tempitem][0], lens_hint_item[tempitem][1], 0);
						}
						
						break;
						
					case mfARROW:
						if(!hints)
						{
							if(!(itemsbuf[Hero.getLastLensID()].flags & ITEM_FLAG2))putcombo(dest,x,y,tmpscr->secretcombo[sARROW],tmpscr->secretcset[sARROW]);
						}
						else
						{
							tempitem=getItemID(itemsbuf,itype_arrow,1);
							
							if(tempitem<0) break;
							
							if((!(get_debug() && zc_getkey(KEY_N)) && (lensclk&blink_rate))
									|| ((get_debug() && zc_getkey(KEY_N)) && (frame&blink_rate)))
							{
								tempitemx=x;
								tempitemy=y;
							}
							
							putitem2(dest,tempitemx,tempitemy,tempitem, lens_hint_item[tempitem][0], lens_hint_item[tempitem][1], 0);
						}
						
						break;
						
					case mfSARROW:
						if(!hints)
						{
							if(!(itemsbuf[Hero.getLastLensID()].flags & ITEM_FLAG2))putcombo(dest,x,y,tmpscr->secretcombo[sSARROW],tmpscr->secretcset[sSARROW]);
						}
						else
						{
							tempitem=getItemID(itemsbuf,itype_arrow,2);
							
							if(tempitem<0) break;
							
							if((!(get_debug() && zc_getkey(KEY_N)) && (lensclk&blink_rate))
									|| ((get_debug() && zc_getkey(KEY_N)) && (frame&blink_rate)))
							{
								tempitemx=x;
								tempitemy=y;
							}
							
							putitem2(dest,tempitemx,tempitemy,tempitem, lens_hint_item[tempitem][0], lens_hint_item[tempitem][1], 0);
						}
						
						break;
						
					case mfGARROW:
						if(!hints)
						{
							if(!(itemsbuf[Hero.getLastLensID()].flags & ITEM_FLAG2))putcombo(dest,x,y,tmpscr->secretcombo[sGARROW],tmpscr->secretcset[sGARROW]);
						}
						else
						{
							tempitem=getItemID(itemsbuf,itype_arrow,3);
							
							if(tempitem<0) break;
							
							if((!(get_debug() && zc_getkey(KEY_N)) && (lensclk&blink_rate))
									|| ((get_debug() && zc_getkey(KEY_N)) && (frame&blink_rate)))
							{
								tempitemx=x;
								tempitemy=y;
							}
							
							putitem2(dest,tempitemx,tempitemy,tempitem, lens_hint_item[tempitem][0], lens_hint_item[tempitem][1], 0);
						}
						
						break;
						
					case mfBOMB:
						if(!hints)
						{
							if(!(itemsbuf[Hero.getLastLensID()].flags & ITEM_FLAG2))putcombo(dest,x,y,tmpscr->secretcombo[sBOMB],tmpscr->secretcset[sBOMB]);
						}
						else
						{
							//tempitem=getItemID(itemsbuf,itype_bomb,1);
							tempweapon = wLitBomb;
							
							//if (tempitem<0) break;
							if((!(get_debug() && zc_getkey(KEY_N)) && (lensclk&blink_rate))
									|| ((get_debug() && zc_getkey(KEY_N)) && (frame&blink_rate)))
							{
								tempweaponx=x;
								tempweapony=y;
							}
							
							putweapon(dest,tempweaponx,tempweapony+lens_hint_weapon[tempweapon][4],tempweapon, 0, up, lens_hint_weapon[tempweapon][0], lens_hint_weapon[tempweapon][1],-1);
						}
						
						break;
						
					case mfSBOMB:
						if(!hints)
						{
							if(!(itemsbuf[Hero.getLastLensID()].flags & ITEM_FLAG2))putcombo(dest,x,y,tmpscr->secretcombo[sSBOMB],tmpscr->secretcset[sSBOMB]);
						}
						else
						{
							//tempitem=getItemID(itemsbuf,itype_sbomb,1);
							//if (tempitem<0) break;
							tempweapon = wLitSBomb;
							
							if((!(get_debug() && zc_getkey(KEY_N)) && (lensclk&blink_rate))
									|| ((get_debug() && zc_getkey(KEY_N)) && (frame&blink_rate)))
							{
								tempweaponx=x;
								tempweapony=y;
							}
							
							putweapon(dest,tempweaponx,tempweapony+lens_hint_weapon[tempweapon][4],tempweapon, 0, up, lens_hint_weapon[tempweapon][0], lens_hint_weapon[tempweapon][1],-1);
						}
						
						break;
						
					case mfARMOS_SECRET:
						if(!hints)
						{
							if(!(itemsbuf[Hero.getLastLensID()].flags & ITEM_FLAG2))putcombo(dest,x,y,tmpscr->secretcombo[sSTAIRS],tmpscr->secretcset[sSTAIRS]);
						}	
						break;
						
					case mfBRANG:
						if(!hints)
						{
							if(!(itemsbuf[Hero.getLastLensID()].flags & ITEM_FLAG2))putcombo(dest,x,y,tmpscr->secretcombo[sBRANG],tmpscr->secretcset[sBRANG]);
						}
						else
						{
							tempitem=getItemID(itemsbuf,itype_brang,1);
							
							if(tempitem<0) break;
							
							if((!(get_debug() && zc_getkey(KEY_N)) && (lensclk&blink_rate))
									|| ((get_debug() && zc_getkey(KEY_N)) && (frame&blink_rate)))
							{
								tempitemx=x;
								tempitemy=y;
							}
							
							putitem2(dest,tempitemx,tempitemy,tempitem, lens_hint_item[tempitem][0], lens_hint_item[tempitem][1], 0);
						}
						
						break;
						
					case mfMBRANG:
						if(!hints)
				{
							if(!(itemsbuf[Hero.getLastLensID()].flags & ITEM_FLAG2))putcombo(dest,x,y,tmpscr->secretcombo[sMBRANG],tmpscr->secretcset[sMBRANG]);
						}
						else
						{
							tempitem=getItemID(itemsbuf,itype_brang,2);
							
							if(tempitem<0) break;
							
							if((!(get_debug() && zc_getkey(KEY_N)) && (lensclk&blink_rate))
									|| ((get_debug() && zc_getkey(KEY_N)) && (frame&blink_rate)))
							{
								tempitemx=x;
								tempitemy=y;
							}
							
							putitem2(dest,tempitemx,tempitemy,tempitem, lens_hint_item[tempitem][0], lens_hint_item[tempitem][1], 0);
						}
						
						break;
						
					case mfFBRANG:
						if(!hints)
						{
							if(!(itemsbuf[Hero.getLastLensID()].flags & ITEM_FLAG2))putcombo(dest,x,y,tmpscr->secretcombo[sFBRANG],tmpscr->secretcset[sFBRANG]);
						}
						else
						{
							tempitem=getItemID(itemsbuf,itype_brang,3);
							
							if(tempitem<0) break;
							
							if((!(get_debug() && zc_getkey(KEY_N)) && (lensclk&blink_rate))
									|| ((get_debug() && zc_getkey(KEY_N)) && (frame&blink_rate)))
							{
								tempitemx=x;
								tempitemy=y;
							}
							
							putitem2(dest,tempitemx,tempitemy,tempitem, lens_hint_item[tempitem][0], lens_hint_item[tempitem][1], 0);
						}
						
						break;
						
					case mfWANDMAGIC:
						if(!hints)
						{
							if(!(itemsbuf[Hero.getLastLensID()].flags & ITEM_FLAG2))putcombo(dest,x,y,tmpscr->secretcombo[sWANDMAGIC],tmpscr->secretcset[sWANDMAGIC]);
						}
						else
						{
							tempitem=getItemID(itemsbuf,itype_wand,1);
							
							if(tempitem<0) break;
							
							tempweapon=itemsbuf[tempitem].wpn3;
							
							if((!(get_debug() && zc_getkey(KEY_N)) && (lensclk&blink_rate))
									|| ((get_debug() && zc_getkey(KEY_N)) && (frame&blink_rate)))
							{
								tempitemx=x;
								tempitemy=y;
							}
							else
							{
								tempweaponx=x;
								tempweapony=y;
								--lens_hint_weapon[wMagic][4];
								
								if(lens_hint_weapon[wMagic][4]<-8)
								{
									lens_hint_weapon[wMagic][4]=8;
								}
							}
							
							putweapon(dest,tempweaponx,tempweapony+lens_hint_weapon[tempweapon][4],tempweapon, 0, up, lens_hint_weapon[tempweapon][0], lens_hint_weapon[tempweapon][1],-1);
							putitem2(dest,tempitemx,tempitemy,tempitem, lens_hint_item[tempitem][0], lens_hint_item[tempitem][1], 0);
						}
						
						break;
						
					case mfREFMAGIC:
						if(!hints)
						{
							if(!(itemsbuf[Hero.getLastLensID()].flags & ITEM_FLAG2))putcombo(dest,x,y,tmpscr->secretcombo[sREFMAGIC],tmpscr->secretcset[sREFMAGIC]);
						}
						else
						{
							tempitem=getItemID(itemsbuf,itype_shield,3);
							
							if(tempitem<0) break;
							
							tempweapon=ewMagic;
							
							if((!(get_debug() && zc_getkey(KEY_N)) && (lensclk&blink_rate))
									|| ((get_debug() && zc_getkey(KEY_N)) && (frame&blink_rate)))
							{
								tempitemx=x;
								tempitemy=y;
							}
							else
							{
								tempweaponx=x;
								tempweapony=y;
								
								if(lens_hint_weapon[ewMagic][2]==up)
								{
									--lens_hint_weapon[ewMagic][4];
								}
								else
								{
									++lens_hint_weapon[ewMagic][4];
								}
								
								if(lens_hint_weapon[ewMagic][4]>8)
								{
									lens_hint_weapon[ewMagic][2]=up;
								}
								
								if(lens_hint_weapon[ewMagic][4]<=0)
								{
									lens_hint_weapon[ewMagic][2]=down;
								}
							}
							
							putitem2(dest,tempitemx,tempitemy,tempitem, lens_hint_item[tempitem][0], lens_hint_item[tempitem][1], 0);
							putweapon(dest,tempweaponx,tempweapony+lens_hint_weapon[tempweapon][4],tempweapon, 0, lens_hint_weapon[ewMagic][2], lens_hint_weapon[tempweapon][0], lens_hint_weapon[tempweapon][1],-1);
						}
						
						break;
						
					case mfREFFIREBALL:
						if(!hints)
						{
							if(!(itemsbuf[Hero.getLastLensID()].flags & ITEM_FLAG2))putcombo(dest,x,y,tmpscr->secretcombo[sREFFIREBALL],tmpscr->secretcset[sREFFIREBALL]);
						}
						else
						{
							tempitem=getItemID(itemsbuf,itype_shield,3);
							
							if(tempitem<0) break;
							
							tempweapon=ewFireball;
							
							if((!(get_debug() && zc_getkey(KEY_N)) && (lensclk&blink_rate))
									|| ((get_debug() && zc_getkey(KEY_N)) && (frame&blink_rate)))
							{
								tempitemx=x;
								tempitemy=y;
								tempweaponx=x;
								tempweapony=y;
								++lens_hint_weapon[ewFireball][3];
								
								if(lens_hint_weapon[ewFireball][3]>8)
								{
									lens_hint_weapon[ewFireball][3]=-8;
									lens_hint_weapon[ewFireball][4]=8;
								}
								
								if(lens_hint_weapon[ewFireball][3]>0)
								{
									++lens_hint_weapon[ewFireball][4];
								}
								else
								{
									--lens_hint_weapon[ewFireball][4];
								}
							}
							
							putitem2(dest,tempitemx,tempitemy,tempitem, lens_hint_item[tempitem][0], lens_hint_item[tempitem][1], 0);
							putweapon(dest,tempweaponx+lens_hint_weapon[tempweapon][3],tempweapony+lens_hint_weapon[ewFireball][4],tempweapon, 0, up, lens_hint_weapon[tempweapon][0], lens_hint_weapon[tempweapon][1],-1);
						}
						
						break;
						
					case mfSWORD:
						if(!hints)
						{
							if(!(itemsbuf[Hero.getLastLensID()].flags & ITEM_FLAG2))putcombo(dest,x,y,tmpscr->secretcombo[sSWORD],tmpscr->secretcset[sSWORD]);
						}
						else
						{
							tempitem=getItemID(itemsbuf,itype_sword,1);
							
							if(tempitem<0) break;
							
							if((!(get_debug() && zc_getkey(KEY_N)) && (lensclk&blink_rate))
									|| ((get_debug() && zc_getkey(KEY_N)) && (frame&blink_rate)))
							{
								tempitemx=x;
								tempitemy=y;
							}
							
							putitem2(dest,tempitemx,tempitemy,tempitem, lens_hint_item[tempitem][0], lens_hint_item[tempitem][1], 0);
						}
						
						break;
						
					case mfWSWORD:
						if(!hints)
						{
							if(!(itemsbuf[Hero.getLastLensID()].flags & ITEM_FLAG2))putcombo(dest,x,y,tmpscr->secretcombo[sWSWORD],tmpscr->secretcset[sWSWORD]);
						}
						else
						{
							tempitem=getItemID(itemsbuf,itype_sword,2);
							
							if(tempitem<0) break;
							
							if((!(get_debug() && zc_getkey(KEY_N)) && (lensclk&blink_rate))
									|| ((get_debug() && zc_getkey(KEY_N)) && (frame&blink_rate)))
							{
								tempitemx=x;
								tempitemy=y;
							}
							
							putitem2(dest,tempitemx,tempitemy,tempitem, lens_hint_item[tempitem][0], lens_hint_item[tempitem][1], 0);
						}
						
						break;
						
					case mfMSWORD:
						if(!hints)
						{
							if(!(itemsbuf[Hero.getLastLensID()].flags & ITEM_FLAG2))putcombo(dest,x,y,tmpscr->secretcombo[sMSWORD],tmpscr->secretcset[sMSWORD]);
						}
						else
						{
							tempitem=getItemID(itemsbuf,itype_sword,3);
							
							if(tempitem<0) break;
							
							if((!(get_debug() && zc_getkey(KEY_N)) && (lensclk&blink_rate))
									|| ((get_debug() && zc_getkey(KEY_N)) && (frame&blink_rate)))
							{
								tempitemx=x;
								tempitemy=y;
							}
							
							putitem2(dest,tempitemx,tempitemy,tempitem, lens_hint_item[tempitem][0], lens_hint_item[tempitem][1], 0);
						}
						
						break;
						
					case mfXSWORD:
						if(!hints)
						{
							if(!(itemsbuf[Hero.getLastLensID()].flags & ITEM_FLAG2))putcombo(dest,x,y,tmpscr->secretcombo[sXSWORD],tmpscr->secretcset[sXSWORD]);
						}
						else
						{
							tempitem=getItemID(itemsbuf,itype_sword,4);
							
							if(tempitem<0) break;
							
							if((!(get_debug() && zc_getkey(KEY_N)) && (lensclk&blink_rate))
									|| ((get_debug() && zc_getkey(KEY_N)) && (frame&blink_rate)))
							{
								tempitemx=x;
								tempitemy=y;
							}
							
							putitem2(dest,tempitemx,tempitemy,tempitem, lens_hint_item[tempitem][0], lens_hint_item[tempitem][1], 0);
						}
						
						break;
						
					case mfSWORDBEAM:
						if(!hints)
						{
							if(!(itemsbuf[Hero.getLastLensID()].flags & ITEM_FLAG2))putcombo(dest,x,y,tmpscr->secretcombo[sSWORDBEAM],tmpscr->secretcset[sSWORDBEAM]);
						}
						else
						{
							tempitem=getItemID(itemsbuf,itype_sword,1);
							
							if(tempitem<0) break;
							
							if((!(get_debug() && zc_getkey(KEY_N)) && (lensclk&blink_rate))
									|| ((get_debug() && zc_getkey(KEY_N)) && (frame&blink_rate)))
							{
								tempitemx=x;
								tempitemy=y;
							}
							
							putitem2(dest,tempitemx,tempitemy,tempitem, lens_hint_item[tempitem][0], lens_hint_item[tempitem][1], 1);
						}
						
						break;
						
					case mfWSWORDBEAM:
						if(!hints)
						{
							if(!(itemsbuf[Hero.getLastLensID()].flags & ITEM_FLAG2))putcombo(dest,x,y,tmpscr->secretcombo[sWSWORDBEAM],tmpscr->secretcset[sWSWORDBEAM]);
						}
						else
						{
							tempitem=getItemID(itemsbuf,itype_sword,2);
							
							if(tempitem<0) break;
							
							if((!(get_debug() && zc_getkey(KEY_N)) && (lensclk&blink_rate))
									|| ((get_debug() && zc_getkey(KEY_N)) && (frame&blink_rate)))
							{
								tempitemx=x;
								tempitemy=y;
							}
							
							putitem2(dest,tempitemx,tempitemy,tempitem, lens_hint_item[tempitem][0], lens_hint_item[tempitem][1], 2);
						}
						
						break;
						
					case mfMSWORDBEAM:
						if(!hints)
						{
							if(!(itemsbuf[Hero.getLastLensID()].flags & ITEM_FLAG2))putcombo(dest,x,y,tmpscr->secretcombo[sMSWORDBEAM],tmpscr->secretcset[sMSWORDBEAM]);
						}
						else
						{
							tempitem=getItemID(itemsbuf,itype_sword,3);
							
							if(tempitem<0) break;
							
							if((!(get_debug() && zc_getkey(KEY_N)) && (lensclk&blink_rate))
									|| ((get_debug() && zc_getkey(KEY_N)) && (frame&blink_rate)))
							{
								tempitemx=x;
								tempitemy=y;
							}
							
							putitem2(dest,tempitemx,tempitemy,tempitem, lens_hint_item[tempitem][0], lens_hint_item[tempitem][1], 3);
						}
						
						break;
						
					case mfXSWORDBEAM:
						if(!hints)
						{
							if(!(itemsbuf[Hero.getLastLensID()].flags & ITEM_FLAG2))putcombo(dest,x,y,tmpscr->secretcombo[sXSWORDBEAM],tmpscr->secretcset[sXSWORDBEAM]);
						}
						else
						{
							tempitem=getItemID(itemsbuf,itype_sword,4);
							
							if(tempitem<0) break;
							
							if((!(get_debug() && zc_getkey(KEY_N)) && (lensclk&blink_rate))
									|| ((get_debug() && zc_getkey(KEY_N)) && (frame&blink_rate)))
							{
								tempitemx=x;
								tempitemy=y;
							}
							
							putitem2(dest,tempitemx,tempitemy,tempitem, lens_hint_item[tempitem][0], lens_hint_item[tempitem][1], 4);
						}
						
						break;
						
					case mfHOOKSHOT:
						if(!hints)
						{
							if(!(itemsbuf[Hero.getLastLensID()].flags & ITEM_FLAG2))putcombo(dest,x,y,tmpscr->secretcombo[sHOOKSHOT],tmpscr->secretcset[sHOOKSHOT]);
						}
						else
						{
							tempitem=getItemID(itemsbuf,itype_hookshot,1);
							
							if(tempitem<0) break;
							
							if((!(get_debug() && zc_getkey(KEY_N)) && (lensclk&blink_rate))
									|| ((get_debug() && zc_getkey(KEY_N)) && (frame&blink_rate)))
							{
								tempitemx=x;
								tempitemy=y;
							}
							
							putitem2(dest,tempitemx,tempitemy,tempitem, lens_hint_item[tempitem][0], lens_hint_item[tempitem][1], 0);
						}
						
						break;
						
					case mfWAND:
						if(!hints)
						{
							if(!(itemsbuf[Hero.getLastLensID()].flags & ITEM_FLAG2))putcombo(dest,x,y,tmpscr->secretcombo[sWAND],tmpscr->secretcset[sWAND]);
						}
						else
						{
							tempitem=getItemID(itemsbuf,itype_wand,1);
							
							if(tempitem<0) break;
							
							if((!(get_debug() && zc_getkey(KEY_N)) && (lensclk&blink_rate))
									|| ((get_debug() && zc_getkey(KEY_N)) && (frame&blink_rate)))
							{
								tempitemx=x;
								tempitemy=y;
							}
							
							putitem2(dest,tempitemx,tempitemy,tempitem, lens_hint_item[tempitem][0], lens_hint_item[tempitem][1], 0);
						}
						
						break;
						
					case mfHAMMER:
						if(!hints)
						{
							if(!(itemsbuf[Hero.getLastLensID()].flags & ITEM_FLAG2))putcombo(dest,x,y,tmpscr->secretcombo[sHAMMER],tmpscr->secretcset[sHAMMER]);
						}
						else
						{
							tempitem=getItemID(itemsbuf,itype_hammer,1);
							
							if(tempitem<0) break;
							
							if((!(get_debug() && zc_getkey(KEY_N)) && (lensclk&blink_rate))
									|| ((get_debug() && zc_getkey(KEY_N)) && (frame&blink_rate)))
							{
								tempitemx=x;
								tempitemy=y;
							}
							
							putitem2(dest,tempitemx,tempitemy,tempitem, lens_hint_item[tempitem][0], lens_hint_item[tempitem][1], 0);
						}
						
						break;
						
					case mfARMOS_ITEM:
					case mfDIVE_ITEM:
						if((!getmapflag() || (tmpscr->flags9&fBELOWRETURN)) && !(itemsbuf[Hero.getLastLensID()].flags & ITEM_FLAG3))
						{
							putitem2(dest,x,y,tmpscr->catchall, lens_hint_item[tmpscr->catchall][0], lens_hint_item[tmpscr->catchall][1], 0);
						}
						break;
						
					case 16:
					case 17:
					case 18:
					case 19:
					case 20:
					case 21:
					case 22:
					case 23:
					case 24:
					case 25:
					case 26:
					case 27:
					case 28:
					case 29:
					case 30:
					case 31:
						if(!hints)
							if(!(itemsbuf[Hero.getLastLensID()].flags & ITEM_FLAG2))
								putcombo(dest,x,y,tmpscr->secretcombo[checkflag-16+4],tmpscr->secretcset[checkflag-16+4]);
									 
						break;
					case mfSECRETSNEXT:
						if(!hints)
							if(!(itemsbuf[Hero.getLastLensID()].flags & ITEM_FLAG2))
								putcombo(dest,x,y,tmpscr->data[i]+1,tmpscr->cset[i]);
									 
						break;
					
					case mfSTRIKE:
						if(!(itemsbuf[Hero.getLastLensID()].flags & ITEM_FLAG2))
						{
							goto special;
						}
						else
						{
							break;
						}
						
					default: goto special;
					
					special:
						if(layer && ((checkflag!=mfRAFT && checkflag!=mfRAFT_BRANCH&& checkflag!=mfRAFT_BOUNCE) ||(itemsbuf[Hero.getLastLensID()].flags & ITEM_FLAG4)))
						{
							if((!(get_debug() && zc_getkey(KEY_N)) && (lensclk&blink_rate)) || ((get_debug() && zc_getkey(KEY_N)) && (frame&blink_rate)))
							{
								rectfill(dest,x,y,x+15,y+15,WHITE);
							}
						}
						
						break;
				}
			}
		}
		
		if(layer)
		{
			if(tmpscr->door[0]==dWALK)
				rectfill(dest, 120, 16+playing_field_offset, 135, 31+playing_field_offset, WHITE);
				
			if(tmpscr->door[1]==dWALK)
				rectfill(dest, 120, 144+playing_field_offset, 135, 159+playing_field_offset, WHITE);
				
			if(tmpscr->door[2]==dWALK)
				rectfill(dest, 16, 80+playing_field_offset, 31, 95+playing_field_offset, WHITE);
				
			if(tmpscr->door[3]==dWALK)
				rectfill(dest, 224, 80+playing_field_offset, 239, 95+playing_field_offset, WHITE);
				
			if(tmpscr->door[0]==dBOMB)
			{
				showbombeddoor(dest, 0);
			}
			
			if(tmpscr->door[1]==dBOMB)
			{
				showbombeddoor(dest, 1);
			}
			
			if(tmpscr->door[2]==dBOMB)
			{
				showbombeddoor(dest, 2);
			}
			
			if(tmpscr->door[3]==dBOMB)
			{
				showbombeddoor(dest, 3);
			}
		}
		
		if(tmpscr->stairx + tmpscr->stairy)
		{
			if(!hints)
				if(!(itemsbuf[Hero.getLastLensID()].flags & ITEM_FLAG2))putcombo(dest,tmpscr->stairx,tmpscr->stairy+playing_field_offset,tmpscr->secretcombo[sSTAIRS],tmpscr->secretcset[sSTAIRS]);
			else
			{
				if(tmpscr->flags&fWHISTLE)
				{
					tempitem=getItemID(itemsbuf,itype_whistle,1);
					int32_t tempitemx=-16;
					int32_t tempitemy=-16;
					
					if((!(get_debug() && zc_getkey(KEY_N)) && (lensclk&(blink_rate/4)))
							|| ((get_debug() && zc_getkey(KEY_N)) && (frame&(blink_rate/4))))
					{
						tempitemx=tmpscr->stairx;
						tempitemy=tmpscr->stairy+playing_field_offset;
					}
					
					putitem2(dest,tempitemx,tempitemy,tempitem, lens_hint_item[tempitem][0], lens_hint_item[tempitem][1], 0);
				}
			}
		}
	}
}

BITMAP *lens_scr_d; // The "d" is for "destructible"!

void draw_lens_over()
{
	// Oh, what the heck.
	static BITMAP *lens_scr = NULL;
	static int32_t last_width = -1;
	int32_t width = itemsbuf[current_item_id(itype_lens,true)].misc1;
	
	// Only redraw the circle if the size has changed
	if(width != last_width)
	{
		if(lens_scr == NULL)
		{
			lens_scr = create_bitmap_ex(8,2*288,2*(240-playing_field_offset));
		}
		
		clear_to_color(lens_scr, BLACK);
		circlefill(lens_scr, 288, 240-playing_field_offset, width, 0);
		circle(lens_scr, 288, 240-playing_field_offset, width+2, 0);
		circle(lens_scr, 288, 240-playing_field_offset, width+5, 0);
		last_width=width;
	}
	
	masked_blit(lens_scr, framebuf, 288-(HeroX()+8), 240-playing_field_offset-(HeroY()+8), 0, playing_field_offset, 256, 168);
}

//----------------------------------------------------------------

void draw_wavy(BITMAP *source, BITMAP *target, int32_t amplitude, bool interpol)
{
	//recreating a big bitmap every frame is highly sluggish.
	static BITMAP *wavebuf = create_bitmap_ex(8,288,240-original_playing_field_offset);
	clear_to_color(wavebuf, BLACK);
	blit(source,wavebuf,0,original_playing_field_offset,16,0,256,224-original_playing_field_offset);
	
	int32_t ofs;
	//  int32_t amplitude=8;
	//  int32_t wavelength=4;
	amplitude = zc_min(2048,amplitude); // some arbitrary limit to prevent crashing
	if((epilepsyFlashReduction || get_bit(quest_rules,qr_EPILEPSY)) && !get_bit(quest_rules, qr_WAVY_NO_EPILEPSY)) amplitude = zc_min(16,amplitude);
	int32_t amp2=168;
	if((epilepsyFlashReduction || get_bit(quest_rules,qr_EPILEPSY)) && !get_bit(quest_rules, qr_WAVY_NO_EPILEPSY_2)) amp2*=2;
	int32_t i=frame%amp2;
	
	for(int32_t j=0; j<168; j++)
	{
		if(j&1 && interpol)
		{
			// Add 288*2048 to ensure it's never negative. It'll get modded out.
			ofs=288*2048+int32_t(zc::math::Sin((double(i+j)*2*PI/amp2))*amplitude);
		}
		else
		{
			ofs=288*2048-int32_t(zc::math::Sin((double(i+j)*2*PI/amp2))*amplitude);
		}
		
		if(ofs)
		{
			for(int32_t k=0; k<256; k++)
			{
				target->line[j+original_playing_field_offset][k]=wavebuf->line[j][(k+ofs+16)%288];
			}
		}
	}
}

void draw_fuzzy(int32_t fuzz)
// draws from right half of scrollbuf to framebuf
{
	int32_t firstx, firsty, xstep, ystep, i, y, dx, dy;
	byte *start, *si, *di;
	
	if(fuzz<1)
		fuzz = 1;
		
	xstep = 128%fuzz;
	
	if(xstep > 0)
		xstep = fuzz-xstep;
		
	ystep = 112%fuzz;
	
	if(ystep > 0)
		ystep = fuzz-ystep;
		
	firsty = 1;
	
	for(y=0; y<224;)
	{
		start = &(scrollbuf->line[y][256]);
		
		for(dy=0; dy<ystep && dy+y<224; dy++)
		{
			si = start;
			di = &(framebuf->line[y+dy][0]);
			i = xstep;
			firstx = 1;
			
			for(dx=0; dx<256; dx++)
			{
				*(di++) = *si;
				
				if(++i >= fuzz)
				{
					if(!firstx)
						si += fuzz;
					else
					{
						si += fuzz-xstep;
						firstx = 0;
					}
					
					i = 0;
				}
			}
		}
		
		if(!firsty)
			y += fuzz;
		else
		{
			y += ystep;
			ystep = fuzz;
			firsty = 0;
		}
	}
}

void updatescr(bool allowwavy)
{
	static BITMAP *wavybuf = create_bitmap_ex(8,256,224);
	static BITMAP *panorama = create_bitmap_ex(8,256,224);
		
	if(toogam)
	{
		textout_ex(framebuf,font,"no walls",8,216,1,-1);
	}
	
	if(Showpal)
		dump_pal(framebuf);
		
	if(!Playing)
		black_opening_count=0;
		
	if(black_opening_count<0) //shape is opening up
	{
		black_opening(framebuf,black_opening_x,black_opening_y,(66+black_opening_count),66);
		
		if(Advance||(!Paused))
		{
			++black_opening_count;
		}
	}
	else if(black_opening_count>0) //shape is closing
	{
		black_opening(framebuf,black_opening_x,black_opening_y,black_opening_count,66);
		
		if(Advance||(!Paused))
		{
			--black_opening_count;
		}
	}

	if (replay_is_debug() && replay_get_mode() != ReplayMode::Replay)
	{
		int depth = bitmap_color_depth(framebuf);
		size_t len = framebuf->w * framebuf->h * BYTES_PER_PIXEL(depth);
		uint32_t hash = XXH32(framebuf->dat, len, 0);
		replay_step_gfx(hash);
	}

	if(black_opening_count==0&&black_opening_shape==bosFADEBLACK)
	{
		black_opening_shape = bosCIRCLE;
		memcpy(RAMpal, tempblackpal, PAL_SIZE*sizeof(RGB));
		refreshTints();
		refreshpal=true;
	}
	
	if(refreshpal)
	{
		refreshpal=false;
		RAMpal[253] = _RGB(0,0,0);
		RAMpal[254] = _RGB(63,63,63);
		hw_palette = &RAMpal;
		update_hw_pal = true;
		
		create_rgb_table(&rgb_table, RAMpal, NULL);
		create_zc_trans_table(&trans_table, RAMpal, 128, 128, 128);
		memcpy(&trans_table2, &trans_table, sizeof(COLOR_MAP));
		
		for(int32_t q=0; q<PAL_SIZE; q++)
		{
			trans_table2.data[0][q] = q;
			trans_table2.data[q][q] = q;
		}
	}
	
	if(details)
		show_details();
	if(show_ff_scripts)
		show_ffscript_names();
	
	bool clearwavy = (wavy <= 0);
	
	if(wavy <= 0)
	{
		// So far one thing can alter wavy apart from scripts: Wavy DMaps.
		wavy = (DMaps[currdmap].flags&dmfWAVY ? 4 : 0);
	}
	
	blit(framebuf, wavybuf, 0, 0, 0, 0, 256, 224);
	
	if(wavy && Playing && allowwavy)
	{
		draw_wavy(framebuf, wavybuf, wavy,false);
	}
	
	if(clearwavy)
		wavy = 0; // Wavy was set by a DMap flag. Clear it.
	else if(Playing && !Paused)
		wavy--; // Wavy was set by a script. Decrement it.
		
	if(Playing && msgpos && !screenscrolling)
	{
		if(!(msg_bg_display_buf->clip))
			blit_msgstr_bg(framebuf,0,0,0,playing_field_offset,256,168);
		if(!(msg_portrait_display_buf->clip))
			blit_msgstr_prt(framebuf,0,0,0,playing_field_offset,256,168);
		if(!(msg_txt_display_buf->clip))
			blit_msgstr_fg(framebuf,0,0,0,playing_field_offset,256,168);
	}
	
	/*
	if(!(msg_txt_display_buf->clip) && Playing && msgpos && !screenscrolling)
	{
		BITMAP* subBmp = 0;
		masked_blit(msg_txt_display_buf,subBmp,0,0,0,playing_field_offset,256,168);
		// masked_blit(msg_txt_display_buf,subBmp,0,playing_field_offset,256,168);
		 draw_trans_sprite(framebuf, subBmp, 0, playing_field_offset);
		destroy_bitmap(subBmp);
		//void draw_sprite_ex(BITMAP *bmp, BITMAP *sprite, int32_t x, int32_t y, int32_t mode, int32_t flip);
	   // masked_blit(msg_txt_display_buf,framebuf,0,0,0,playing_field_offset,256,168);
		//void masked_blit(BITMAP *source, BITMAP *dest, int32_t source_x, int32_t source_y, int32_t dest_x, int32_t dest_y, int32_t width, int32_t height);
	}
	*/
	
	bool nosubscr = (tmpscr->flags3&fNOSUBSCR && !(tmpscr->flags3&fNOSUBSCROFFSET));
	
	if(nosubscr)
	{
		rectfill(panorama,0,0,255,passive_subscreen_height/2,0);
		rectfill(panorama,0,168+passive_subscreen_height/2,255,168+passive_subscreen_height-1,0);
		blit(wavybuf,panorama,0,playing_field_offset,0,passive_subscreen_height/2,256,224-passive_subscreen_height);
	}
	
	//TODO: Optimize blit 'overcalls' -Gleeok
	BITMAP *source = nosubscr ? panorama : wavybuf;
		
	static BITMAP *scanlinesbmp=NULL;
	
	if(resx != SCREEN_W || resy != SCREEN_H)
	{
		Z_message("Conflicting variables warning: screen_scale %i, resx %i, resy %i, w %i, h %i\n", screen_scale, resx, resy, SCREEN_W, SCREEN_H);
		resx = SCREEN_W;
		resy = SCREEN_H;
		screen_scale = zc_max(zc_min(resx / 320, resy / 240), 1);
	}
	
	if(!sbig && screen_scale > 1)
		sbig = true;
		
	const int32_t sx = 256 * screen_scale;
	const int32_t sy = 224 * screen_scale;
	const int32_t scale_mul = screen_scale - 1;
	const int32_t mx = scale_mul * 128;
	const int32_t my = scale_mul * 112;
	
	if(sbig)
	{
		if(scanlines)
		{
			if(!scanlinesbmp)
				scanlinesbmp = create_bitmap_ex(8, sx, sy);
				
			stretch_blit(source, scanlinesbmp, 0, 0, 256, 224, 0, 0, sx, sy);
			
			for(int32_t i=0; i<224; ++i)
				_allegro_hline(scanlinesbmp, 0, (i*screen_scale)+1, sx, BLACK);
				
			blit(scanlinesbmp, screen, 0, 0, scrx+32-mx, scry+8-my, sx, sy);
		}
		else
		{
			stretch_blit(source, screen, 0, 0, 256, 224, scrx+32-mx, scry+8-my, sx, sy);
		}
		
		if(quakeclk>0)
			rectfill(screen, // I don't know if these are right...
					 scrx+32 - mx, //x1
					 scry+8 - my + sy, //y1
					 scrx+32 - mx + sx, //x2
					 scry+8 - my + sy + (16 * scale_mul), //y2
					 BLACK);
					 
		//stretch_blit(nosubscr?panorama:wavybuf,screen,0,0,256,224,scrx+32-128,scry+8-112,512,448);
		//if(quakeclk>0) rectfill(screen,scrx+32-128,scry+8-112+448,scrx+32-128+512,scry+8-112+456,0);
	}
	else
	{
		blit(source,screen,0,0,scrx+32,scry+8,256,224);
		
		if(quakeclk>0) rectfill(screen,scrx+32,scry+8+224,scrx+32+256,scry+8+232,BLACK);
	}
	
	if(ShowFPS)// &&(frame&1))
		show_fps(screen);
	
	show_replay_controls(screen);
		
	if(Paused)
		show_paused(screen);
		
	if(details)
	{
		textprintf_ex(screen,font,0,SCREEN_H-8,254,BLACK,"%-6d (%s)", idle_count, time_str_long(idle_count));
	}
	
	//if(panorama!=NULL) destroy_bitmap(panorama);
	
	++framecnt;
	
	update_hw_screen();
}

//----------------------------------------------------------------

PALETTE sys_pal;

int32_t onGUISnapshot()
{
	char buf[200];
	int32_t num=0;
	bool realpal=(key[KEY_ZC_LCONTROL] || key[KEY_ZC_RCONTROL]);
	do
	{
		sprintf(buf, "%szc_screen%05d.%s", get_snap_str(), ++num, snapshotformat_str[SnapshotFormat][1]);
	}
	while(num<99999 && exists(buf));
	
	BITMAP *b = create_bitmap_ex(8,resx,resy);
	
	if(b)
	{
		if(MenuOpen)
	{
		//Cannot load game's palette while GUI elements are in focus. -Z
		//If there is a way to do this, then I have missed it.
		/*
		game_pal();
		RAMpal[253] = _RGB(0,0,0);
		RAMpal[254] = _RGB(63,63,63);
		set_palette_range(RAMpal,0,255,false);
		memcpy(RAMpal, snappal, sizeof(snappal));
		create_rgb_table(&rgb_table, RAMpal, NULL);
		create_zc_trans_table(&trans_table, RAMpal, 128, 128, 128);
		memcpy(&trans_table2, &trans_table, sizeof(COLOR_MAP));
		
		for(int32_t q=0; q<PAL_SIZE; q++)
		{
			trans_table2.data[0][q] = q;
			trans_table2.data[q][q] = q;
		}
		*/
		//ringcolor(false);
		//get_palette(RAMpal);
		blit(screen,b,0,0,0,0,resx,resy);
		//al_trace("Menu Open\n");
		//game_pal();
		//PALETTE temppal;
		//get_palette(temppal);
		//system_pal();
		save_bitmap(buf,b,sys_pal);
		//save_bitmap(buf,b,RAMpal);
		//save_bitmap(buf,b,snappal);
	}	
	else 
	{
		blit(screen,b,0,0,0,0,resx,resy);
		save_bitmap(buf,b,realpal?sys_pal:RAMpal);
	}
		destroy_bitmap(b);
	}
	
	return D_O_K;
}

int32_t onNonGUISnapshot()
{
	PALETTE temppal;
	get_palette(temppal);
	bool realpal=(zc_getkey(KEY_ZC_LCONTROL, true) || zc_getkey(KEY_ZC_RCONTROL, true));
	
	char buf[200];
	int32_t num=0;
	
	do
	{
		sprintf(buf, "%szc_screen%05d.%s", get_snap_str(), ++num, snapshotformat_str[SnapshotFormat][1]);
	}
	while(num<99999 && exists(buf));
	
	BITMAP *panorama = create_bitmap_ex(8,256,168);
	/*
	PALETTE tempRAMpal;
	get_palette(tempRAMpal);
	
	if(tmpscr->flags3&fNOSUBSCR)
	{
		clear_to_color(panorama,0);
		blit(framebuf,panorama,0,playing_field_offset,0,0,256,168);
		save_bitmap(buf,panorama,realpal?temppal:tempRAMpal);
	}
	else
	{
		save_bitmap(buf,framebuf,realpal?temppal:tempRAMpal);
	}
	
	destroy_bitmap(panorama);
	return D_O_K;
	*/
	if(tmpscr->flags3&fNOSUBSCR && !(key[KEY_ALT]))
	{
		clear_to_color(panorama,0);
		blit(framebuf,panorama,0,playing_field_offset,0,0,256,168);
		save_bitmap(buf,panorama,realpal?temppal:RAMpal);
	}
	else
	{
		save_bitmap(buf,framebuf,realpal?temppal:RAMpal);
	}
	
	destroy_bitmap(panorama);
	return D_O_K;
}

int32_t onSnapshot()
{
	if(zc_getkey(KEY_LSHIFT, true)||zc_getkey(KEY_RSHIFT, true))
	{
		onGUISnapshot();
	}
	else
	{
		onNonGUISnapshot();
	}
	
	return D_O_K;
}

int32_t onSaveMapPic()
{
	int32_t mapres2 = 0;
	char buf[200];
	int32_t num=0;
	mapscr tmpscr_b[2];
	mapscr tmpscr_c[6];
	BITMAP* _screen_draw_buffer = NULL;
	_screen_draw_buffer = create_bitmap_ex(8,256,224);
	set_clip_state(_screen_draw_buffer,1);
	
	for(int32_t i=0; i<6; ++i)
	{
		tmpscr_c[i] = tmpscr2[i];
		tmpscr2[i].zero_memory();
		
		if(i>=2)
		{
			continue;
		}
		
		tmpscr_b[i] = tmpscr[i];
		tmpscr[i].zero_memory();
	}
	
	do
	{
		sprintf(buf, "%szc_screen%05d.png", get_snap_str(), ++num);
	}
	while(num<99999 && exists(buf));
	
	BITMAP* mappic = NULL;
	
	
	bool done=false, redraw=true;
	
	mappic = create_bitmap_ex(8,(256*16)>>mapres,(176*8)>>mapres);
	
	if(!mappic)
	{
		system_pal();
		jwin_alert("View Map","Not enough memory.",NULL,NULL,"OK",NULL,13,27,lfont);
		game_pal();
		return D_O_K;;
	}
	
	// draw the map
	set_clip_rect(_screen_draw_buffer, 0, 0, _screen_draw_buffer->w, _screen_draw_buffer->h);
	
	for(int32_t y=0; y<8; y++)
	{
		for(int32_t x=0; x<16; x++)
		{
			if(!displayOnMap(x, y))
			{
				rectfill(_screen_draw_buffer, 0, 0, 255, 223, WHITE);
			}
			else
			{
				int32_t s = (y<<4) + x;
				loadscr2(1,s,-1);
				
				for(int32_t i=0; i<6; i++)
				{
					if(tmpscr[1].layermap[i]<=0)
						continue;
					
					if((ZCMaps[tmpscr[1].layermap[i]-1].tileWidth==ZCMaps[currmap].tileWidth) &&
					   (ZCMaps[tmpscr[1].layermap[i]-1].tileHeight==ZCMaps[currmap].tileHeight))
					{
						const int32_t _mapsSize = (ZCMaps[currmap].tileWidth)*(ZCMaps[currmap].tileHeight);
						
						tmpscr2[i]=TheMaps[(tmpscr[1].layermap[i]-1)*MAPSCRS+tmpscr[1].layerscreen[i]];
						
						tmpscr2[i].data.resize(_mapsSize, 0);
						tmpscr2[i].sflag.resize(_mapsSize, 0);
						tmpscr2[i].cset.resize(_mapsSize, 0);
					}
				}
				
				if(XOR((tmpscr+1)->flags7&fLAYER2BG, DMaps[currdmap].flags&dmfLAYER2BG)) do_layer(_screen_draw_buffer, 0, 2, tmpscr+1, -256, playing_field_offset, 2);
				
				if(XOR((tmpscr+1)->flags7&fLAYER3BG, DMaps[currdmap].flags&dmfLAYER3BG)) do_layer(_screen_draw_buffer, 0, 3, tmpscr+1, -256, playing_field_offset, 2);
				
				putscr(_screen_draw_buffer,256,0,tmpscr+1);
				do_layer(_screen_draw_buffer, 0, 1, tmpscr+1, -256, playing_field_offset, 2);
				
				if(!XOR((tmpscr+1)->flags7&fLAYER2BG, DMaps[currdmap].flags&dmfLAYER2BG)) do_layer(_screen_draw_buffer, 0, 2, tmpscr+1, -256, playing_field_offset, 2);
				
				putscrdoors(_screen_draw_buffer,256,0,tmpscr+1);
				do_layer(_screen_draw_buffer, -2, 0, tmpscr+1, -256, playing_field_offset, 2);
				if(get_bit(quest_rules, qr_PUSHBLOCK_LAYER_1_2))
				{
					do_layer(_screen_draw_buffer, -2, 1, tmpscr+1, -256, playing_field_offset, 2);
					do_layer(_screen_draw_buffer, -2, 2, tmpscr+1, -256, playing_field_offset, 2);
				}
				do_layer(_screen_draw_buffer, -3, 0, tmpscr+1, -256, playing_field_offset, 2); // Freeform combos!
				
				if(!XOR((tmpscr+1)->flags7&fLAYER3BG, DMaps[currdmap].flags&dmfLAYER3BG)) do_layer(_screen_draw_buffer, 0, 3, tmpscr+1, -256, playing_field_offset, 2);
				
				do_layer(_screen_draw_buffer, 0, 4, tmpscr+1, -256, playing_field_offset, 2);
				do_layer(_screen_draw_buffer, -1, 0, tmpscr+1, -256, playing_field_offset, 2);
				if(get_bit(quest_rules, qr_OVERHEAD_COMBOS_L1_L2))
				{
					do_layer(_screen_draw_buffer, -1, 1, tmpscr+1, -256, playing_field_offset, 2);
					do_layer(_screen_draw_buffer, -1, 2, tmpscr+1, -256, playing_field_offset, 2);
				}
				do_layer(_screen_draw_buffer, 0, 5, tmpscr+1, -256, playing_field_offset, 2);
				do_layer(_screen_draw_buffer, 0, 6, tmpscr+1, -256, playing_field_offset, 2);
				
			}
			
			stretch_blit(_screen_draw_buffer, mappic, 256, 0, 256, 176, x<<(8-mapres), (y*176)>>mapres, 256>>mapres, 176>>mapres);
		}
	}
	
	for(int32_t i=0; i<6; ++i)
	{
		tmpscr2[i]=tmpscr_c[i];
		
		if(i>=2)
		{
			continue;
		}
		
		tmpscr[i]=tmpscr_b[i];
	}
	
	save_bitmap(buf,mappic,RAMpal);
	destroy_bitmap(mappic);
	destroy_bitmap(_screen_draw_buffer);
	return D_O_K;
}

/*
int32_t onSaveMapPic()
{
	BITMAP* mappic = NULL;
	BITMAP* _screen_draw_buffer = NULL;
	_screen_draw_buffer = create_bitmap_ex(8,256,224);
	int32_t mapres2 = 0;
	char buf[20];
	int32_t num=0;
	set_clip_state(_screen_draw_buffer,1);
	set_clip_rect(_screen_draw_buffer,0,0,_screen_draw_buffer->w, _screen_draw_buffer->h);
	
	do
	{
		sprintf(buf, "zelda%03d.png", ++num);
	}
	while(num<999 && exists(buf));
	
	//  if(!mappic) {
	mappic = create_bitmap_ex(8,(256*16)>>mapres2,(176*8)>>mapres2);
	
	if(!mappic)
	{
		system_pal();
		jwin_alert("Save Map Picture","Not enough memory.",NULL,NULL,"OK",NULL,13,27,lfont);
		game_pal();
		return D_O_K;
	}
	
	//  }
	
	int32_t layermap, layerscreen;
	int32_t x2=0;
	
	// draw the map
	for(int32_t y=0; y<8; y++)
	{
		for(int32_t x=0; x<16; x++)
		{
			int32_t s = (y<<4) + x;
			
			if(!displayOnMap(x, y))
			{
				rectfill(_screen_draw_buffer, 0, 0, 255, 223, WHITE);
			}
			else
			{
				loadscr(TEMPSCR_FUNCTION_SWAP_SPACE,currdmap,s,-1,false);
				putscr(_screen_draw_buffer, 0, 0, tmpscr+1);
				
				for(int32_t k=0; k<4; k++)
				{
					if(k==2)
					{
						putscrdoors(_screen_draw_buffer, 0, 0, tmpscr+1);
					}
					
					layermap=TheMaps[currmap*MAPSCRS+s].layermap[k]-1;
					
					if(layermap>-1)
					{
						layerscreen=layermap*MAPSCRS+TheMaps[currmap*MAPSCRS+s].layerscreen[k];
						
						if(TheMaps[currmap*MAPSCRS+s].layeropacity[k]==255)
						{
							for(int32_t i=0; i<176; i++)
							{
								overcombo(_screen_draw_buffer,((i&15)<<4)+x2,(i&0xF0),TheMaps[layerscreen].data[i],TheMaps[layerscreen].cset[i]);
							}
						}
						else
						{
							for(int32_t i=0; i<176; i++)
							{
								overcombotranslucent(_screen_draw_buffer,((i&15)<<4)+x2,(i&0xF0),TheMaps[layerscreen].data[i],TheMaps[layerscreen].cset[i],TheMaps[currmap*MAPSCRS+s].layeropacity[k]);
							}
						}
					}
				}
				
				for(int32_t i=0; i<176; i++)
				{
//		  if (COMBOTYPE((i&15)<<4,i&0xF0)==cOLD_OVERHEAD)
					if(combo_class_buf[COMBOTYPE((i&15)<<4,i&0xF0)].overhead)
					{
						overcombo(_screen_draw_buffer,((i&15)<<4)+x2,(i&0xF0),MAPCOMBO((i&15)<<4,i&0xF0),MAPCSET((i&15)<<4,i&0xF0));
					}
				}
				
				for(int32_t k=4; k<6; k++)
				{
					layermap=TheMaps[currmap*MAPSCRS+s].layermap[k]-1;
					
					if(layermap>-1)
					{
						layerscreen=layermap*MAPSCRS+TheMaps[currmap*MAPSCRS+s].layerscreen[k];
						
						if(TheMaps[currmap*MAPSCRS+s].layeropacity[k]==255)
						{
							for(int32_t i=0; i<176; i++)
							{
								overcombo(_screen_draw_buffer,((i&15)<<4)+x2,(i&0xF0),TheMaps[layerscreen].data[i],TheMaps[layerscreen].cset[i]);
							}
						}
						else
						{
							for(int32_t i=0; i<176; i++)
							{
								overcombotranslucent(_screen_draw_buffer,((i&15)<<4)+x2,(i&0xF0),TheMaps[layerscreen].data[i],TheMaps[layerscreen].cset[i],TheMaps[currmap*MAPSCRS+s].layeropacity[k]);
							}
						}
					}
				}
			}
			
			stretch_blit(_screen_draw_buffer, mappic, 0, 0, 256, 176,
						 x<<(8-mapres2), (y*176)>>mapres2, 256>>mapres2, 176>>mapres2);
		}
		
	}
	
	save_bitmap(buf,mappic,RAMpal);
	destroy_bitmap(mappic);
	destroy_bitmap(_screen_draw_buffer);
	return D_O_K;
}
*/

void f_Quit(int32_t type)
{
	if(type==qQUIT && !Playing)
		return;
		
	music_pause();
	pause_all_sfx();
	system_pal();
	clear_keybuf();
	
	switch(type)
	{
	case qQUIT:
		if (replay_is_replaying())
		{
			disableClickToFreeze=false;
			Quit=qQUIT;
			
			// Trying to evade a door repair charge?
			if(repaircharge)
			{
				game->change_drupy(-repaircharge);
				repaircharge=0;
			}
		}
		else
		{
			onQuit();
		}
		break;
		
	case qRESET:
		onReset();
		break;
		
	case qEXIT:
		onExit();
		break;
	}
	
	if(Quit)
	{
		kill_sfx();
		music_stop();
		game_pal();
		clear_to_color(screen,BLACK);
		update_hw_screen();
	}
	else
	{
		game_pal();
		music_resume();
		resume_all_sfx();
	}
	
	show_mouse(NULL);
	eat_buttons();
	
	zc_readrawkey(KEY_ESC);
		
	zc_readrawkey(KEY_ENTER);
}

//----------------------------------------------------------------

int32_t onNoWalls()
{
	cheats_enqueue(Cheat::Walls);
	return D_O_K;
}

int32_t onIgnoreSideview()
{
	cheats_enqueue(Cheat::IgnoreSideView);
	return D_O_K;
}

int32_t input_idle(bool checkmouse)
{
	static int32_t mx, my, mz, mb;
	
	if(keypressed() || zc_key_pressed() ||
	   (checkmouse && (mx != gui_mouse_x() || my != gui_mouse_y() || mz != gui_mouse_z() || mb != gui_mouse_b())))
	{
		idle_count = 0;
		
		if(active_count < MAX_ACTIVE)
		{
			++active_count;
		}
	}
	else if(idle_count < MAX_IDLE)
	{
		++idle_count;
		active_count = 0;
	}
	
	mx = gui_mouse_x();
	my = gui_mouse_y();
	mz = gui_mouse_z();
	mb = gui_mouse_b();
	
	return idle_count;
}

int32_t onGoFast()
{
	cheats_enqueue(Cheat::Fast);
	return D_O_K;
}

int32_t onKillCheat()
{
	cheats_enqueue(Cheat::Kill);
	return D_O_K;
}

int32_t onShowLayer0()
{
	show_layer_0 = !show_layer_0;
	return D_O_K;
}
int32_t onShowLayer1()
{
	show_layer_1 = !show_layer_1;
	return D_O_K;
}
int32_t onShowLayer2()
{
	show_layer_2 = !show_layer_2;
	return D_O_K;
}
int32_t onShowLayer3()
{
	show_layer_3 = !show_layer_3;
	return D_O_K;
}
int32_t onShowLayer4()
{
	show_layer_4 = !show_layer_4;
	return D_O_K;
}
int32_t onShowLayer5()
{
	show_layer_5 = !show_layer_5;
	return D_O_K;
}
int32_t onShowLayer6()
{
	show_layer_6 = !show_layer_6;
	return D_O_K;
}
int32_t onShowLayerO()
{
	show_layer_over=!show_layer_over;
	return D_O_K;
}
int32_t onShowLayerP()
{
	show_layer_push=!show_layer_push;
	return D_O_K;
}
int32_t onShowLayerS()
{
	show_sprites=!show_sprites;
	return D_O_K;
}
int32_t onShowLayerF()
{
	show_ffcs=!show_ffcs;
	return D_O_K;
}
int32_t onShowLayerW()
{
	show_walkflags=!show_walkflags;
	return D_O_K;
}
int32_t onShowLayerE()
{
	show_effectflags=!show_effectflags;
	return D_O_K;
}
int32_t onShowFFScripts()
{
	show_ff_scripts=!show_ff_scripts;
	return D_O_K;
}
int32_t onShowHitboxes()
{
	show_hitboxes=!show_hitboxes;
	return D_O_K;
}

int32_t onLightSwitch()
{
	cheats_enqueue(Cheat::Light);
	return D_O_K;
}

int32_t onGoTo();
int32_t onGoToComplete();

// Used in syskeys() to prevent keys from being read as both game and system input
/*static int32_t storedInput[14];
static void backupAndClearInput()
{
	storedInput[0]=key[DUkey];
	key[DUkey]=false;
	storedInput[1]=key[DDkey];
	key[DDkey]=false;
	storedInput[2]=key[DLkey];
	key[DLkey]=false;
	storedInput[3]=key[DRkey];
	key[DRkey]=false;
	storedInput[4]=key[Akey];
	key[Akey]=false;
	storedInput[5]=key[Bkey];
	key[Bkey]=false;
	storedInput[6]=key[Skey];
	key[Skey]=false;
	storedInput[7]=key[Lkey];
	key[Lkey]=false;
	storedInput[8]=key[Rkey];
	key[Rkey]=false;
	storedInput[9]=key[Pkey];
	key[Pkey]=false;
	storedInput[10]=key[Exkey1];
	key[Exkey1]=false;
	storedInput[11]=key[Exkey2];
	key[Exkey2]=false;
	storedInput[12]=key[Exkey3];
	key[Exkey3]=false;
	storedInput[13]=key[Exkey4];
	key[Exkey4]=false;
}

static void restoreInput()
{
	key[DUkey]=storedInput[0];
	key[DDkey]=storedInput[1];
	key[DLkey]=storedInput[2];
	key[DRkey]=storedInput[3];
	key[Akey]=storedInput[4];
	key[Bkey]=storedInput[5];
	key[Skey]=storedInput[6];
	key[Lkey]=storedInput[7];
	key[Rkey]=storedInput[8];
	key[Pkey]=storedInput[9];
	key[Exkey1]=storedInput[10];
	key[Exkey2]=storedInput[11];
	key[Exkey3]=storedInput[12];
	key[Exkey4]=storedInput[13];
}
*/
void syskeys()
{
	  //Saffith's method of separating system and game key bindings. Can't do this!!
	//backupAndClearInput(); //This caused input to become randomly 'stuck'. -Z
	
	int32_t oldtitle_version;
	
	if(close_button_quit)
	{
		close_button_quit=false;
		f_Quit(qEXIT);
	}
	
	poll_joystick();
	
	if(rMbtn() || (gui_mouse_b() && !mouse_down && ClickToFreeze &&!disableClickToFreeze))
	{
		oldtitle_version=title_version;
		System();
	}
	
	mouse_down=gui_mouse_b();
	
	if(zc_readkey(KEY_F1))
	{
		if(zc_getkey(KEY_ZC_LCONTROL) || zc_getkey(KEY_ZC_RCONTROL))
		{
			halt=!halt;
			//zinit.subscreen=(zinit.subscreen+1)%ssdtMAX;
		}
		else
		{
			Throttlefps=!Throttlefps;
			logic_counter=0;
		}
	}
	
	//  if(zc_readkey(KEY_F1))	Vsync=!Vsync;
	/*
	  if(zc_readkey(KEY_F1))	set_bit(QHeader.rules4,qr4_NEWENEMYTILES,
	  1-((get_bit(QHeader.rules4,qr4_NEWENEMYTILES))));
	  */
	
	if(zc_readkey(KEY_OPENBRACE))	if(frame_rest_suggest > 0) frame_rest_suggest--;
	
	if(zc_readkey(KEY_CLOSEBRACE))	if(frame_rest_suggest <= 2) frame_rest_suggest++;
	
	if(zc_readkey(KEY_F2))	ShowFPS=!ShowFPS;
	
	if(zc_readrawkey(KEY_F3) && Playing)	Paused=!Paused;
	
	if(zc_readrawkey(KEY_F4) && Playing)
	{
		Paused=true;
		Advance=true;
	}
	
	if(zc_readrawkey(KEY_F6)) onTryQuit();
	
#ifndef ALLEGRO_MACOSX
	if(zc_readrawkey(KEY_F9))	f_Quit(qRESET);
	
	if(zc_readrawkey(KEY_F10))   f_Quit(qEXIT);
#else
	if(zc_readrawkey(KEY_F7))	f_Quit(qRESET);
	
	if(zc_readrawkey(KEY_F8))   f_Quit(qEXIT);
#endif
	if(rF5()&&(Playing && currscr<128 && DMaps[currdmap].flags&dmfVIEWMAP))	onSaveMapPic();
	
	if(rF12())
	{
		onSnapshot();
	}
	
	if(debug_enabled && zc_readkey(KEY_TAB))
		set_debug(!get_debug());
		
	if(get_debug() || cheat>=1)
	{
	if( CheatModifierKeys() )
	{
			if(zc_readkey(KEY_ASTERISK) || zc_readkey(KEY_H))   cheats_enqueue(Cheat::Life, game->get_maxlife());
			
			if(zc_readkey(KEY_SLASH_PAD) || zc_readkey(KEY_M))  cheats_enqueue(Cheat::Magic, game->get_maxmagic());
			
			if(zc_readkey(KEY_R))		  cheats_enqueue(Cheat::Rupies, game->get_maxcounter(1));
			
			if(zc_readkey(KEY_B))
			{
				cheats_enqueue(Cheat::Bombs, game->get_maxbombs(), game->get_maxcounter(6));
			}
			
			if(zc_readkey(KEY_A))
			{
				cheats_enqueue(Cheat::Arrows, game->get_maxarrows());
			}
	}
	}
	
	if(get_debug() || cheat>=2)
	{
		if( CheatModifierKeys() )
		{
			if(rI())
			{
				cheats_enqueue(Cheat::Clock);
			}
		}
	}
	
	if(get_debug() || cheat>=4)
	{
	if( CheatModifierKeys() )
	{
			if(rF11())
			{
				cheats_enqueue(Cheat::Walls);
			}
			
			if(rQ())
			{
				cheats_enqueue(Cheat::Fast);
			}
			
			if(zc_readkey(KEY_F))
			{
				cheats_enqueue(Cheat::Freeze);
			}
			
			if(zc_readkey(KEY_G))   onGoToComplete();
			
			if(zc_readkey(KEY_0))   onShowLayer0();
			
			if(zc_readkey(KEY_1))   onShowLayer1();
			
			if(zc_readkey(KEY_2))   onShowLayer2();
			
			if(zc_readkey(KEY_3))   onShowLayer3();
			
			if(zc_readkey(KEY_4))   onShowLayer4();
			
			if(zc_readkey(KEY_5))   onShowLayer5();
			
			if(zc_readkey(KEY_6))   onShowLayer6();
			
			//if(zc_readkey(KEY_7))   onShowLayerO();
			if(zc_readkey(KEY_7))   onShowLayerF();
			
			if(zc_readkey(KEY_8))   onShowLayerS();
			
			if(zc_readkey(KEY_W))   onShowLayerW();
			
			if(zc_readkey(KEY_L))   cheats_enqueue(Cheat::Light);
			
			if(zc_readkey(KEY_V))   cheats_enqueue(Cheat::IgnoreSideView);
			
			if(zc_readkey(KEY_K))   cheats_enqueue(Cheat::Kill);
			if(zc_readkey(KEY_O))   onShowLayerO();
			if(zc_readkey(KEY_P))   onShowLayerP();
			if(zc_readkey(KEY_C))   onShowHitboxes();
			if(zc_readkey(KEY_F))   onShowFFScripts();
	}
	}
	
	if(volkeys)
	{
		if(zc_readkey(KEY_PGUP)) master_volume(-1,midi_volume+8);
		
		if(zc_readkey(KEY_PGDN)) master_volume(-1,midi_volume==255?248:midi_volume-8);
		
		if(zc_readkey(KEY_HOME)) master_volume(digi_volume+8,-1);
		
		if(zc_readkey(KEY_END))  master_volume(digi_volume==255?248:digi_volume-8,-1);
	}
	
	if(!get_debug() || !SystemKeys)
		goto bottom;
		
	if(zc_readkey(KEY_D))
	{
		details = !details;
		rectfill(screen,0,0,319,7,BLACK);
		rectfill(screen,0,8,31,239,BLACK);
		rectfill(screen,288,8,319,239,BLACK);
		rectfill(screen,32,232,287,239,BLACK);
	}
	
	if(zc_readkey(KEY_P))   Paused=!Paused;
	
	//if(zc_readkey(KEY_P))   centerHero();
	if(zc_readkey(KEY_A))
	{
		Paused=true;
		Advance=true;
	}
	
	if(zc_readkey(KEY_G))   db=(db==999)?0:999;
#ifndef ALLEGRO_MACOSX
	if(zc_readkey(KEY_F8))  Showpal=!Showpal;
	
	if(zc_readkey(KEY_F7))
	{
		Matrix(ss_speed, ss_density, 0);
		game_pal();
	}
#else
	// The reason these are different on Mac in the first place is that
	// the OS doesn't let us use F9 and F10...
	if(zc_readkey(KEY_F10))  Showpal=!Showpal;
	
	if(zc_readkey(KEY_F9))
	{
		Matrix(ss_speed, ss_density, 0);
		game_pal();
	}
#endif
	if(zc_readkey(KEY_PLUS_PAD) || zc_readkey(KEY_EQUALS))
	{
		//change containers
		if(zc_getkey(KEY_ZC_LCONTROL) || zc_getkey(KEY_ZC_RCONTROL))
		{
			//magic containers
			if(zc_getkey(KEY_LSHIFT) || zc_getkey(KEY_RSHIFT))
			{
				game->set_maxmagic(zc_min(game->get_maxmagic()+game->get_mp_per_block(),game->get_mp_per_block()*8));
			}
			else
			{
				game->set_maxlife(zc_min(game->get_maxlife()+game->get_hp_per_heart(),game->get_hp_per_heart()*24));
			}
		}
		else
		{
			if(zc_getkey(KEY_LSHIFT) || zc_getkey(KEY_RSHIFT))
			{
				game->set_magic(zc_min(game->get_magic()+1,game->get_maxmagic()));
			}
			else
			{
				game->set_life(zc_min(game->get_life()+1,game->get_maxlife()));
			}
		}
	}
	
	if(zc_readkey(KEY_MINUS_PAD) || zc_readkey(KEY_MINUS))
	{
		//change containers
		if(zc_getkey(KEY_ZC_LCONTROL) || zc_getkey(KEY_ZC_RCONTROL))
		{
			//magic containers
			if(zc_getkey(KEY_LSHIFT) || zc_getkey(KEY_RSHIFT))
			{
				game->set_maxmagic(zc_max(game->get_maxmagic()-game->get_mp_per_block(),0));
				game->set_magic(zc_min(game->get_maxmagic(), game->get_magic()));
				//heart containers
			}
			else
			{
				game->set_maxlife(zc_max(game->get_maxlife()-game->get_hp_per_heart(),game->get_hp_per_heart()));
				game->set_life(zc_min(game->get_maxlife(), game->get_life()));
			}
		}
		else
		{
			if(zc_getkey(KEY_LSHIFT) || zc_getkey(KEY_RSHIFT))
			{
				game->set_magic(zc_max(game->get_magic()-1,0));
			}
			else
			{
				game->set_life(zc_max(game->get_life()-1,0));
			}
		}
	}
	
	if(zc_readkey(KEY_COMMA))  jukebox(currmidi-1);
	
	if(zc_readkey(KEY_STOP))   jukebox(currmidi+1);
	
	/*
	  if(zc_readkey(KEY_TILDE)) {
	  wavyout();
	  zinit.subscreen=(zinit.subscreen+1)%3;
	  wavyin();
	  }
	  */
	
	verifyBothWeapons();
	
bottom:

	if(input_idle(true) > after_time())
	{
		Matrix(ss_speed, ss_density, 0);
		game_pal();
	}
	//Saffith's method of separating system and game key bindings. Can't do this!!
	//restoreInput(); //This caused input to become randomly 'stuck'. -Z
	
	//while(Playing && keypressed())
	//readkey();
	// What's the Playing check for?
	clear_keybuf();
}

void checkQuitKeys()
{
#ifndef ALLEGRO_MACOSX
	if(zc_readrawkey(KEY_F9))	f_Quit(qRESET);
	
	if(zc_readrawkey(KEY_F10))   f_Quit(qEXIT);
#else
	if(zc_readrawkey(KEY_F7))	f_Quit(qRESET);
	
	if(zc_readrawkey(KEY_F8))   f_Quit(qEXIT);
#endif
}

bool CheatModifierKeys()
{
	if ( ( cheat_modifier_keys[0] > 0 && key[cheat_modifier_keys[0]] ) ||
		( cheat_modifier_keys[1] > 0 && key[cheat_modifier_keys[1]] ) ||
		(cheat_modifier_keys[0] <= 0 && cheat_modifier_keys[1] <= 0))
	{
		if ( ( cheat_modifier_keys[2] <= 0 || key[cheat_modifier_keys[2]] ) ||
			( cheat_modifier_keys[3] > 0 && key[cheat_modifier_keys[3]] )  ||
			(cheat_modifier_keys[2] <= 0 && cheat_modifier_keys[3] <= 0))
		{
			return true;
		}
	}
	return false;
}

//99:05:54, for some reason?
#define OLDMAXTIME  21405240
//9000:00:00, the highest even-thousand hour fitting within 32b signed. This is 375 *DAYS*.
#define MAXTIME	 1944000000

void advanceframe(bool allowwavy, bool sfxcleanup, bool allowF6Script)
{
	if(zcmusic!=NULL)
	{
		zcmusic_poll();
	}
	
	while(Paused && !Advance && !Quit)
	{
		// have to call this, otherwise we'll get an infinite loop
		syskeys();
		if(allowF6Script)
		{
			FFCore.runF6Engine();
		}
		// to keep fps constant
		updatescr(allowwavy);
		throttleFPS();
		
#ifdef _WIN32
		
		if(use_dwm_flush)
		{
			do_DwmFlush();
		}
		
#endif
		
		// to keep music playing
		if(zcmusic!=NULL)
		{
			zcmusic_poll();
		}
	}
	
	if(Quit)
		return;
		
	if(Playing && game->get_time()<unsigned(get_bit(quest_rules,qr_GREATER_MAX_TIME) ? MAXTIME : OLDMAXTIME))
		game->change_time(1);
		
	Advance=false;

	locking_keys = true;
	++frame;
	update_keys(); //Update ZScript key arrays
	locking_keys = false;
	
	syskeys();
	if (replay_is_replaying())
		replay_peek_quit();
	if (Quit)
		replay_step_quit(Quit);
	if (GameFlags & GAMEFLAG_TRYQUIT)
		replay_step_quit(0);
	if(allowF6Script)
	{
		FFCore.runF6Engine();
	}
	// Someday... maybe install a Turbo button here?
	updatescr(allowwavy);
	throttleFPS();
	
#ifdef _WIN32
	
	if(use_dwm_flush)
	{
		do_DwmFlush();
	}
	
#endif
	
	//textprintf_ex(screen,font,0,72,254,BLACK,"%d %d", lastentrance, lastentrance_dmap);
	if(sfxcleanup)
		sfx_cleanup();
}

void zapout()
{
	set_clip_rect(scrollbuf, 0, 0, scrollbuf->w, scrollbuf->h);
	blit(framebuf,scrollbuf,0,0,256,0,256,224);
	
	FFCore.runGenericPassiveEngine(SCR_TIMING_END_FRAME);
	script_drawing_commands.Clear();
	
	// zap out
	for(int32_t i=1; i<=24; i++)
	{
		draw_fuzzy(i);
		syskeys();
		advanceframe(true);
		
		if(Quit)
		{
			break;
		}
	}
}

void zapin()
{
	FFCore.warpScriptCheck();
	draw_screen(tmpscr);
	set_clip_rect(scrollbuf, 0, 0, scrollbuf->w, scrollbuf->h);
	//put_passive_subscr(framebuf,&QMisc,0,passive_subscreen_offset,false,sspUP);
	blit(framebuf,scrollbuf,0,0,256,0,256,224);
	
	// zap out
	FFCore.runGenericPassiveEngine(SCR_TIMING_END_FRAME);
	for(int32_t i=24; i>=1; i--)
	{
		draw_fuzzy(i);
		syskeys();
		advanceframe(true);
		
		if(Quit)
		{
			break;
		}
	}
}


void wavyout(bool showhero)
{
	draw_screen(tmpscr, showhero);
	//put_passive_subscr(framebuf,&QMisc,0,passive_subscreen_offset,false,sspUP);
	
	BITMAP *wavebuf = create_bitmap_ex(8,288,224);
	clear_to_color(wavebuf,0);
	blit(framebuf,wavebuf,0,0,16,0,256,224);
	
	static PALETTE wavepal;
	
	int32_t ofs;
	int32_t amplitude=8;
	
	int32_t wavelength=4;
	double palpos=0, palstep=4, palstop=126;
	
	FFCore.runGenericPassiveEngine(SCR_TIMING_END_FRAME);
	for(int32_t i=0; i<168; i+=wavelength)
	{
		for(int32_t l=0; l<256; l++)
		{
			wavepal[l].r=vbound(int32_t(RAMpal[l].r+((palpos/palstop)*(63-RAMpal[l].r))),0,63);
			wavepal[l].g=vbound(int32_t(RAMpal[l].g+((palpos/palstop)*(63-RAMpal[l].g))),0,63);
			wavepal[l].b=vbound(int32_t(RAMpal[l].b+((palpos/palstop)*(63-RAMpal[l].b))),0,63);
		}
		
		palpos+=palstep;
		
		if(palpos>=0)
		{
			hw_palette = &wavepal;
			update_hw_pal = true;
		}
		else
		{
			hw_palette = &RAMpal;
			update_hw_pal = true;
		}
		
		for(int32_t j=0; j+playing_field_offset<224; j++)
		{
			for(int32_t k=0; k<256; k++)
			{
				ofs=0;
				
				if((j<i)&&(j&1))
				{
					ofs=int32_t(zc::math::Sin((double(i+j)*2*PI/168.0))*amplitude);
				}
				
				framebuf->line[j+playing_field_offset][k]=wavebuf->line[j+playing_field_offset][k+ofs+16];
			}
		}
		
		syskeys();
		advanceframe(true);
		
		//	animate_combos();
		if(Quit)
			break;
	}
	
	destroy_bitmap(wavebuf);
}

void wavyin()
{
	draw_screen(tmpscr);
	//put_passive_subscr(framebuf,&QMisc,0,passive_subscreen_offset,false,sspUP);
	
	BITMAP *wavebuf = create_bitmap_ex(8,288,224);
	clear_to_color(wavebuf,0);
	blit(framebuf,wavebuf,0,0,16,0,256,224);
	
	static PALETTE wavepal;
	
	//Breaks dark rooms.
	//In any case I don't think we need this, since palette is already loaded in doWarp() (famous last words...) -DD
	/*
	  loadfullpal();
	  loadlvlpal(DMaps[currdmap].color);
	  ringcolor(false);
	*/
	refreshpal=false;
	int32_t ofs;
	int32_t amplitude=8;
	int32_t wavelength=4;
	double palpos=168, palstep=4, palstop=126;
	
	FFCore.runGenericPassiveEngine(SCR_TIMING_END_FRAME);
	for(int32_t i=0; i<168; i+=wavelength)
	{
		for(int32_t l=0; l<256; l++)
		{
			wavepal[l].r=vbound(int32_t(RAMpal[l].r+((palpos/palstop)*(63-RAMpal[l].r))),0,63);
			wavepal[l].g=vbound(int32_t(RAMpal[l].g+((palpos/palstop)*(63-RAMpal[l].g))),0,63);
			wavepal[l].b=vbound(int32_t(RAMpal[l].b+((palpos/palstop)*(63-RAMpal[l].b))),0,63);
		}
		
		palpos-=palstep;
		
		if(palpos>=0)
		{
			hw_palette = &wavepal;
			update_hw_pal = true;
		}
		else
		{
			hw_palette = &RAMpal;
			update_hw_pal = true;
		}
		
		for(int32_t j=0; j+playing_field_offset<224; j++)
		{
			for(int32_t k=0; k<256; k++)
			{
				ofs=0;
				
				if((j<(167-i))&&(j&1))
				{
					ofs=int32_t(zc::math::Sin((double(i+j)*2*PI/168.0))*amplitude);
				}
				
				framebuf->line[j+playing_field_offset][k]=wavebuf->line[j+playing_field_offset][k+ofs+16];
			}
		}
		
		syskeys();
		advanceframe(true);
		//	animate_combos();
		
		if(Quit)
			break;
	}
	
	destroy_bitmap(wavebuf);
}

void blackscr(int32_t fcnt,bool showsubscr)
{
	reset_pal_cycling();
	script_drawing_commands.Clear();
	
	FFCore.warpScriptCheck();
	bool showtime = game->get_timevalid() && !game->did_cheat() && get_bit(quest_rules,qr_TIME);
	while(fcnt>0)
	{
		clear_bitmap(framebuf);
		
		if(showsubscr)
		{
			put_passive_subscr(framebuf,&QMisc,0,passive_subscreen_offset,showtime,sspUP);
			if(get_bit(quest_rules, qr_SCRIPTDRAWSINWARPS) || (get_bit(quest_rules, qr_PASSIVE_SUBSCRIPT_RUNS_WHEN_GAME_IS_FROZEN)))
			{
				do_script_draws(framebuf, tmpscr, 0, playing_field_offset);
			}
		}
		
		syskeys();
		advanceframe(true);
		
		if(Quit)
			break;
			
		--fcnt;
	}
}

void openscreen(int32_t shape)
{
	reset_pal_cycling();
	black_opening_count=0;
	
	if(COOLSCROLL || shape>-1)
	{
		open_black_opening(HeroX()+8, (HeroY()-HeroZ()-HeroFakeZ())+8+playing_field_offset, true, shape);
		return;
	}
	else
	{
		Hero.setDontDraw(true);
		show_subscreen_dmap_dots=false;
		show_subscreen_numbers=false;
		//	show_subscreen_items=false;
		show_subscreen_life=false;
	}
	
	int32_t x=128;
	
	FFCore.warpScriptCheck();
	for(int32_t i=0; i<80; i++)
	{
		draw_screen(tmpscr);
		//? draw_screen already draws the subscreen -DD
		//put_passive_subscr(framebuf,&QMisc,0,passive_subscreen_offset,false,sspUP);
		x=128-(((i*128/80)/8)*8);
		
		if(x>0)
		{
			rectfill(framebuf,0,playing_field_offset,x,167+playing_field_offset,0);
			rectfill(framebuf,256-x,playing_field_offset,255,167+playing_field_offset,0);
		}
		
		//	x=((80-i)/2)*4;
		/*
		  --x;
		  switch(++c)
		  {
		  case 5: c=0;
		  case 0:
		  case 2:
		  case 3: --x; break;
		  }
		  */
		syskeys();
		advanceframe(true);
		
		if(Quit)
		{
			break;
		}
	}
	
	Hero.setDontDraw(false);
	show_subscreen_items=true;
	show_subscreen_dmap_dots=true;
}

void closescreen(int32_t shape)
{
	reset_pal_cycling();
	black_opening_count=0;
	
	if(COOLSCROLL || shape>-1)
	{
		close_black_opening(HeroX()+8, (HeroY()-HeroZ()-HeroFakeZ())+8+playing_field_offset, true, shape);
		return;
	}
	else
	{
		Hero.setDontDraw(true);
		show_subscreen_dmap_dots=false;
		show_subscreen_numbers=false;
		//	show_subscreen_items=false;
		show_subscreen_life=false;
	}
	
	int32_t x=128;
	
	FFCore.warpScriptCheck();
	for(int32_t i=79; i>=0; --i)
	{
		draw_screen(tmpscr);
		//? draw_screen already draws the subscreen -DD
		//put_passive_subscr(framebuf,&QMisc,0,passive_subscreen_offset,false,sspUP);
		x=128-(((i*128/80)/8)*8);
		
		if(x>0)
		{
			rectfill(framebuf,0,playing_field_offset,x,167+playing_field_offset,0);
			rectfill(framebuf,256-x,playing_field_offset,255,167+playing_field_offset,0);
		}
		
		//	x=((80-i)/2)*4;
		/*
		  --x;
		  switch(++c)
		  {
		  case 5: c=0;
		  case 0:
		  case 2:
		  case 3: --x; break;
		  }
		  */
		syskeys();
		advanceframe(true);
		
		if(Quit)
		{
			break;
		}
	}
	
	Hero.setDontDraw(false);
	show_subscreen_items=true;
	show_subscreen_dmap_dots=true;
}

int32_t TriforceCount()
{
	int32_t c=0;
	
	for(int32_t i=1; i<=8; i++)
		if(game->lvlitems[i]&liTRIFORCE)
			++c;
			
	return c;
}

int32_t onCustomGame()
{
	int32_t file =  getsaveslot();
	
	if(file < 0)
		return D_O_K;
		
	bool ret = (custom_game(file)!=0);
	return ret ? D_CLOSE : D_O_K;
}

int32_t onContinue()
{
	return D_CLOSE;
}

int32_t onEsc() // Unused?? -L
{
	return zc_getrawkey(KEY_ESC, true)?D_CLOSE:D_O_K;
}

int32_t onVsync()
{
	Throttlefps = !Throttlefps;
	return D_O_K;
}

int32_t onClickToFreeze()
{
	ClickToFreeze = !ClickToFreeze;
	return D_O_K;
}

int32_t OnSaveZCConfig()
{
	if(jwin_alert3(
			"Save Configuration", 
			"Are you sure that you wish to save your present configuration settings?", 
			"This will overwrite your prior settings!",
			NULL,
		 "&Yes", 
		"&No", 
		NULL, 
		'y', 
		'n', 
		0, 
		lfont) == 1)	
	{
		save_game_configs();
		return D_O_K;
	}
	else return D_O_K;
}

int32_t OnnClearQuestDir()
{
	if(jwin_alert3(
			"Clear Current Directory Cache", 
			"Are you sure that you wish to clear the current cached directory?", 
			"This will default the current directory to the ROOT for this instance of ZC Player!",
			NULL,
		 "&Yes", 
		"&No", 
		NULL, 
		'y', 
		'n', 
		0, 
		lfont) == 1)	
	{
		set_config_string("zeldadx","win_qst_dir","");
		flush_config_file();
		strcpy(qstdir,get_config_string("zeldadx","win_qst_dir",""));
		//strcpy(filepath,get_config_string("zeldadx","win_qst_dir",""));
		//save_game_configs();
		return D_O_K;
	}
	else return D_O_K;
}


int32_t onConsoleZASM()
{
	if ( !zasm_debugger )
	{
		AlertDialog("WARNING: ZASM Debugger",
			"Enabling this will open the ZASM Debugger Console" 
			"\nThis will likely grind ZC to a halt with lag."
			"\nTo make any use of this, it is suggested that you read"
			"\nthe documentation for 'void Breakpoint(char[] string);'"
			" in 'ZScript_Additions.txt'"
			"\nThis is not recommended for normal users,"
			" and is only intended for ZC developers,"
			"\nor quest developers coding directly in ZASM"
			"\nAre you sure that you wish to open the ZASM Debugger?",
			[&](bool ret,bool)
			{
				if(ret)
				{
					FFCore.ZASMPrint(true);
					zasm_debugger = 1;
					save_game_configs();
				}
			}).show();
		return D_O_K;
	}
	else
	{
		zasm_debugger = 0;
		save_game_configs();
		FFCore.ZASMPrint(false);
		return D_O_K;
	}
}


int32_t onConsoleZScript()
{
	if ( !zscript_debugger )
	{
		AlertDialog("ZScript Debugger",
			"Enabling this will open the ZScript Debugger Console" 
			"\nThis will display any messages logged by scripts,"
			" including script errors."
			"\nAre you sure that you wish to open the ZScript Debugger?",
			[&](bool ret,bool)
			{
				if(ret)
				{
					FFCore.ZScriptConsole(true);
					zscript_debugger = 1;
					save_game_configs();
				}
			}).show();
		return D_O_K;
	}
	else
	{
		zscript_debugger = 0;
		save_game_configs();
		FFCore.ZScriptConsole(false);
		return D_O_K;
	}
}


int32_t onFrameSkip()
{
	FrameSkip = !FrameSkip;
	return D_O_K;
}

int32_t onTransLayers()
{
	TransLayers = !TransLayers;
	return D_O_K;
}

int32_t onNESquit()
{
	NESquit = !NESquit;
	return D_O_K;
}

int32_t onVolKeys()
{
	volkeys = !volkeys;
	return D_O_K;
}

int32_t onShowFPS()
{
	ShowFPS = !ShowFPS;
	scare_mouse();
	
	if(ShowFPS)
		show_fps(screen);
		
	if(sbig)
		stretch_blit(fps_undo,screen,0,0,64,16,scrx+40-120,scry+216+96,128,32);
	else
		blit(fps_undo,screen,0,0,scrx+40,scry+216,64,16);
		
	if(Paused)
		show_paused(screen);
		
	unscare_mouse();
	return D_O_K;
}

bool is_Fkey(int32_t k)
{
	switch(k)
	{
	case KEY_F1:
	case KEY_F2:
	case KEY_F3:
	case KEY_F4:
	case KEY_F5:
	case KEY_F6:
	case KEY_F7:
	case KEY_F8:
	case KEY_F9:
	case KEY_F10:
	case KEY_F11:
	case KEY_F12:
		return true;
	}
	
	return false;
}

void kb_getkey(DIALOG *d)
{
	d->flags|=D_SELECTED;
	
	scare_mouse();
	jwin_button_proc(MSG_DRAW,d,0);
	jwin_draw_win(screen, (resx-160)/2, (resy-48)/2, 160, 48, FR_WIN);
	//  text_mode(vc(11));
	textout_centre_ex(screen, font, "Press a key", resx/2, resy/2 - 8, jwin_pal[jcBOXFG],jwin_pal[jcBOX]);
	textout_centre_ex(screen, font, "ESC to cancel", resx/2, resy/2, jwin_pal[jcBOXFG],jwin_pal[jcBOX]);
	unscare_mouse();
	
	update_hw_screen(true);
	
	clear_keybuf();
	int32_t k = next_press_key();
	clear_keybuf();
	
	//shnarf
	//47=f1
	//59=esc
	if(k>0 && k<123 && !((k>46)&&(k<60)))
		*((int32_t*)d->dp3) = k;
		
		
	d->flags&=~D_SELECTED;
}


//Used by all keyboard key settings dialogues.
void kb_clearjoystick(DIALOG *d)
{
	d->flags|=D_SELECTED;
	
	scare_mouse();
	jwin_button_proc(MSG_DRAW,d,0);
	jwin_draw_win(screen, (resx-160)/2, (resy-48)/2, 168, 48, FR_WIN);
	//  text_mode(vc(11));
	textout_centre_ex(screen, font, "Press any key to clear", resx/2, resy/2 - 8, jwin_pal[jcBOXFG],jwin_pal[jcBOX]);
	textout_centre_ex(screen, font, "ESC to cancel", resx/2, resy/2, jwin_pal[jcBOXFG],jwin_pal[jcBOX]);
	unscare_mouse();
	
	update_hw_screen();
	
	clear_keybuf();
	int32_t k = next_press_key();
	clear_keybuf();
	
	//shnarf
	//47=f1
	//59=esc
//	if(k>0 && k<123 && !((k>46)&&(k<60)))
//		*((int32_t*)d->dp3) = k;
	if ( k != 59 ) *((int32_t*)d->dp3) = 0;
		
		
	d->flags&=~D_SELECTED;
}

//Clears key to 0. 
//Used by all keyboard key settings dialogues.
void kb_clearkey(DIALOG *d)
{
	d->flags|=D_SELECTED;
	
	scare_mouse();
	jwin_button_proc(MSG_DRAW,d,0);
	jwin_draw_win(screen, (resx-160)/2, (resy-48)/2, 160, 48, FR_WIN);
	//  text_mode(vc(11));
	textout_centre_ex(screen, font, "Press any key to clear", resx/2, resy/2 - 8, jwin_pal[jcBOXFG],jwin_pal[jcBOX]);
	textout_centre_ex(screen, font, "ESC to cancel", resx/2, resy/2, jwin_pal[jcBOXFG],jwin_pal[jcBOX]);
	unscare_mouse();
	
	update_hw_screen();
	
	clear_keybuf();
	int32_t k = next_press_key();
	clear_keybuf();
	
	//shnarf
	//47=f1
	//59=esc
//	if(k>0 && k<123 && !((k>46)&&(k<60)))
//		*((int32_t*)d->dp3) = k;
	if ( k != 59 ) *((int32_t*)d->dp3) = 0;
		
		
	d->flags&=~D_SELECTED;
}


int32_t d_j_clearbutton_proc(int32_t msg,DIALOG *d,int32_t c)
{
	switch(msg)
	{
	case MSG_KEY:
	case MSG_CLICK:
	
		kb_clearjoystick(d);
		
		while(gui_mouse_b())
			clear_keybuf();
			
		return D_REDRAW;
	}
	
	return jwin_button_proc(msg,d,c);
}

int32_t d_kbutton_proc(int32_t msg,DIALOG *d,int32_t c)
{
	switch(msg)
	{
	case MSG_KEY:
	case MSG_CLICK:
	
		kb_getkey(d);
		
		while(gui_mouse_b())
			clear_keybuf();
			
		return D_REDRAW;
	}
	
	return jwin_button_proc(msg,d,c);
}

//Only used in keyboard settings dialogues to clear keys. 
int32_t d_k_clearbutton_proc(int32_t msg,DIALOG *d,int32_t c)
{
	switch(msg)
	{
	case MSG_KEY:
	case MSG_CLICK:
	
		kb_clearkey(d);
		
		while(gui_mouse_b())
			clear_keybuf();
			
		return D_REDRAW;
	}
	
	return jwin_button_proc(msg,d,c);
}

void j_getbtn(DIALOG *d)
{
	d->flags|=D_SELECTED;
	scare_mouse();
	jwin_button_proc(MSG_DRAW,d,0);
	jwin_draw_win(screen, (resx-160)/2, (resy-48)/2, 160, 48, FR_WIN);
	//  text_mode(vc(11));
	int32_t y = resy/2 - 12;
	textout_centre_ex(screen, font, "Press a button", resx/2, y, jwin_pal[jcBOXFG],jwin_pal[jcBOX]);
	textout_centre_ex(screen, font, "ESC to cancel", resx/2, y+8, jwin_pal[jcBOXFG],jwin_pal[jcBOX]);
	textout_centre_ex(screen, font, "SPACE to disable", resx/2, y+16, jwin_pal[jcBOXFG],jwin_pal[jcBOX]);
	unscare_mouse();
	
	update_hw_screen();
	
	int32_t b = next_press_btn();
	
	if(b>=0)
		*((int32_t*)d->dp3) = b;
		
	d->flags&=~D_SELECTED;
	
	if(!player) //safety first...
		player = init_dialog(d,-1);
		
	player->joy_on = TRUE;
}

int32_t d_jbutton_proc(int32_t msg,DIALOG *d,int32_t c)
{
	switch(msg)
	{
	case MSG_KEY:
	case MSG_CLICK:
	
		j_getbtn(d);
		
		while(gui_mouse_b())
			clear_keybuf();
			
		return D_REDRAW;
	}
	
	return jwin_button_proc(msg,d,c);
}

//shnarf
const char *key_str[] =
{
    "(none)       ",              "a            ",              "b            ",              "c            ",
    "d            ",              "e            ",              "f            ",              "g            ",
    "h            ",              "i            ",              "j            ",              "k            ",
    "l            ",              "m            ",              "n            ",              "o            ",
    "p            ",              "q            ",              "r            ",              "s            ",
    "t            ",              "u            ",              "v            ",              "w            ",
    "x            ",              "y            ",              "z            ",              "0            ",
    "1            ",              "2            ",              "3            ",              "4            ",
    "5            ",              "6            ",              "7            ",              "8            ",
    "9            ",              "num 0        ",              "num 1        ",              "num 2        ",
    "num 3        ",              "num 4        ",              "num 5        ",              "num 6        ",
    "num 7        ",              "num 8        ",              "num 9        ",              "f1           ",
    "f2           ",              "f3           ",              "f4           ",              "f5           ",
    "f6           ",              "f7           ",              "f8           ",              "f9           ",
    "f10          ",              "f11          ",              "f12          ",              "esc          ",
    "~            ",              "-            ",              "=            ",              "backspace    ",
    "tab          ",              "{            ",              "}            ",              "enter        ",
    ":            ",              "quote        ",              "\\           ",              "\\ (2)       ",
    ",            ",              ".            ",              "/            ",              "space        ",
    "insert       ",              "delete       ",              "home         ",              "end          ",
    "page up      ",              "page down    ",              "left         ",              "right        ",
    "up           ",              "down         ",              "num /        ",              "num *        ",
    "num -        ",              "num +        ",              "num delete   ",              "num enter    ",
    "print screen ",              "pause        ",              "abnt c1      ",              "yen          ",
    "kana         ",              "convert      ",              "no convert   ",              "at           ",
    "circumflex   ",              ": (2)        ",              "kanji        ",              "num =        ",
    "back quote   ",              ";            ",              "command      ",              "unknown (0)  ",
    "unknown (1)  ",              "unknown (2)  ",              "unknown (3)  ",              "unknown (4)  ",
    "unknown (5)  ",              "unknown (6)  ",              "unknown (7)  ",              "left shift   ",
    "right shift  ",              "left control ",              "right control",              "alt          ",
    "alt gr       ",              "left win     ",              "right win    ",              "menu         ",
    "scroll lock  ",              "number lock  ",              "caps lock    ",      "MAX"
};


const char *pan_str[4] = { "MONO", " 1/2", " 3/4", "FULL" };
//extern int32_t zcmusic_bufsz;

static char str_a[80],str_b[80],str_s[80],str_m[80],str_l[80],str_r[80],str_p[80],str_ex1[80],str_ex2[80],str_ex3[80],str_ex4[80],
	str_leftmod1[80],str_leftmod2[80],str_rightmod1[80],str_rightmod2[80], str_left[80], str_right[80], str_up[80], str_down[80];

int32_t d_stringloader(int32_t msg,DIALOG *d,int32_t c)
{
	//these are here to bypass compiler warnings about unused arguments
	c=c;
	
	if(msg==MSG_DRAW)
	{
		switch(d->w)
		{
			case 0:
				sprintf(str_a,"%03d\n%s",Akey,key_str[Akey]);
				sprintf(str_b,"%03d\n%s",Bkey,key_str[Bkey]);
				sprintf(str_s,"%03d\n%s",Skey,key_str[Skey]);
				sprintf(str_l,"%03d\n%s",Lkey,key_str[Lkey]);
				sprintf(str_r,"%03d\n%s",Rkey,key_str[Rkey]);
				sprintf(str_p,"%03d\n%s",Pkey,key_str[Pkey]);
				sprintf(str_ex1,"%03d\n%s",Exkey1,key_str[Exkey1]);
				sprintf(str_ex2,"%03d\n%s",Exkey2,key_str[Exkey2]);
				sprintf(str_ex3,"%03d\n%s",Exkey3,key_str[Exkey3]);
				sprintf(str_ex4,"%03d\n%s",Exkey4,key_str[Exkey4]);
				sprintf(str_up,"%03d\n%s",DUkey,key_str[DUkey]);
				sprintf(str_down,"%03d\n%s",DDkey,key_str[DDkey]);
				sprintf(str_left,"%03d\n%s",DLkey,key_str[DLkey]);
				sprintf(str_right,"%03d\n%s",DRkey,key_str[DRkey]);
				sprintf(str_leftmod1,"%03d\n%s",cheat_modifier_keys[0],key_str[cheat_modifier_keys[0]]);
				sprintf(str_leftmod2,"%03d\n%s",cheat_modifier_keys[1],key_str[cheat_modifier_keys[1]]);
				sprintf(str_rightmod1,"%03d\n%s",cheat_modifier_keys[2],key_str[cheat_modifier_keys[2]]);
				sprintf(str_rightmod2,"%03d\n%s",cheat_modifier_keys[3],key_str[cheat_modifier_keys[3]]);
				break;
				
			case 1:
				sprintf(str_a,"%03d\n%s",Abtn,joybtn_name(Abtn));
				sprintf(str_b,"%03d\n%s",Bbtn,joybtn_name(Bbtn));
				sprintf(str_s,"%03d\n%s",Sbtn,joybtn_name(Sbtn));
				sprintf(str_l,"%03d\n%s",Lbtn,joybtn_name(Lbtn));
				sprintf(str_r,"%03d\n%s",Rbtn,joybtn_name(Rbtn));
				sprintf(str_m,"%03d\n%s",Mbtn,joybtn_name(Mbtn));
				sprintf(str_p,"%03d\n%s",Pbtn,joybtn_name(Pbtn));
				sprintf(str_ex1,"%03d\n%s",Exbtn1,joybtn_name(Exbtn1));
				sprintf(str_ex2,"%03d\n%s",Exbtn2,joybtn_name(Exbtn2));
				sprintf(str_ex3,"%03d\n%s",Exbtn3,joybtn_name(Exbtn3));
				sprintf(str_ex4,"%03d\n%s",Exbtn4,joybtn_name(Exbtn4));
				sprintf(str_up,"%03d\n%s",DUbtn,joybtn_name(DUbtn));
				sprintf(str_down,"%03d\n%s",DDbtn,joybtn_name(DDbtn));
				sprintf(str_left,"%03d\n%s",DLbtn,joybtn_name(DLbtn));
				sprintf(str_right,"%03d\n%s",DRbtn,joybtn_name(DRbtn));
				sprintf(str_leftmod1,"%03d\n%s",cheat_modifier_keys[0],key_str[cheat_modifier_keys[0]]);
				sprintf(str_leftmod2,"%03d\n%s",cheat_modifier_keys[1],key_str[cheat_modifier_keys[1]]);
				sprintf(str_rightmod1,"%03d\n%s",cheat_modifier_keys[2],key_str[cheat_modifier_keys[2]]);
				sprintf(str_rightmod2,"%03d\n%s",cheat_modifier_keys[3],key_str[cheat_modifier_keys[3]]);
				break;
				
			case 2:
				sprintf(str_a,"%3d",midi_volume);
				sprintf(str_b,"%3d",digi_volume);
				sprintf(str_l,"%3d",emusic_volume);
				sprintf(str_m,"%3dKB",zcmusic_bufsz);
				sprintf(str_r,"%3d",sfx_volume);
				strcpy(str_s,pan_str[pan_style]);
				sprintf(str_leftmod1,"%3d\n%s",cheat_modifier_keys[0],key_str[cheat_modifier_keys[0]]);
				sprintf(str_leftmod2,"%3d\n%s",cheat_modifier_keys[1],key_str[cheat_modifier_keys[1]]);
				sprintf(str_rightmod1,"%3d\n%s",cheat_modifier_keys[2],key_str[cheat_modifier_keys[2]]);
				sprintf(str_rightmod2,"%3d\n%s",cheat_modifier_keys[3],key_str[cheat_modifier_keys[3]]);
				break;
		}
	}
	
	return D_O_K;
}

int32_t set_vol(void *dp3, int32_t d2)
{
	switch(((int32_t*)dp3)[0])
	{
	case 0:
		midi_volume   = zc_min(d2<<3,255);
		break;
		
	case 1:
		digi_volume   = zc_min(d2<<3,255);
		break;
		
	case 2:
		emusic_volume = zc_min(d2<<3,255);
		break;
		
	case 3:
		sfx_volume	= zc_min(d2<<3,255);
		break;
	}
	
	scare_mouse();
	// text_mode(vc(11));
	textprintf_right_ex(screen,is_large ? lfont_l : font, ((int32_t*)dp3)[1],((int32_t*)dp3)[2],jwin_pal[jcBOXFG],jwin_pal[jcBOX],"%3d",zc_min(d2<<3,255));
	unscare_mouse();
	return D_O_K;
}

int32_t set_pan(void *dp3, int32_t d2)
{
	pan_style = vbound(d2,0,3);
	scare_mouse();
	// text_mode(vc(11));
	textout_right_ex(screen,is_large ? lfont_l : font, pan_str[pan_style],((int32_t*)dp3)[1],((int32_t*)dp3)[2],jwin_pal[jcBOXFG],jwin_pal[jcBOX]);
	unscare_mouse();
	return D_O_K;
}

int32_t set_buf(void *dp3, int32_t d2)
{
	scare_mouse();
	// text_mode(vc(11));
	zcmusic_bufsz = d2 + 1;
	textprintf_right_ex(screen,is_large ? lfont_l : font, ((int32_t*)dp3)[1],((int32_t*)dp3)[2],jwin_pal[jcBOXFG],jwin_pal[jcBOX],"%3dKB",zcmusic_bufsz);
	unscare_mouse();
	return D_O_K;
}

static int32_t gamepad_btn_list[] =
{
	6,
	7,8,9,10,11,12,13,14,15,16,17,
	18,19,20,21,22,23,24,25,26,27,28,
	29,30,31,32,33,34,35,36,37,38,39,
	-1
};

static int32_t gamepad_dirs_list[] =
{
	40,41,42,43,
	44,45,46,47,
	48,49,50,51,
	52,53,54,55,
	56,
	-1
};

static TABPANEL gamepad_tabs[] =
{
	// (text)
	{ (char *)"Buttons",		D_SELECTED,  gamepad_btn_list, 0, NULL },
	{ (char *)"Directions",  0,		   gamepad_dirs_list, 0, NULL },
	{ NULL,				  0,		   NULL,			   0, NULL }
};

static DIALOG gamepad_dlg[] =
{
	// (dialog proc)	   (x)   (y)   (w)   (h)   (fg)	 (bg)	 (key)	(flags)	(d1)	  (d2)	 (dp)	 (dp2) (dp3)
	{ jwin_win_proc,	   8,	24,   304,  256,  0,	   0,	   0,	   D_EXIT,	0,		0, (void *) "Gamepad Controls", NULL,  NULL },
	{ jwin_tab_proc,	   8+4, 24+23,304-8,256-52,vc(0),   vc(15),  0,	   0,		 0,		0, (void *) gamepad_tabs, NULL, (void *)gamepad_dlg },
	{ d_stringloader,	  0,	0,	1,	0,	0,	   0,	   0,	   0,		 0,		0,	   NULL, NULL,  NULL },
	{ d_timer_proc,		 0,	0,	 0,	0,	0,	   0,	   0,	   0,		  0,		  0,		 NULL, NULL, NULL },
	{ jwin_button_proc,	90,   254,  61,   21,   0,	   0,	   0,	   D_EXIT,	0,		0, (void *) "OK", NULL,  NULL },
	{ jwin_button_proc,	170,  254,  61,   21,   0,	   0,	   0,	   D_EXIT,	0,		0, (void *) "Cancel", NULL,  NULL },
	// 6
	{ d_dummy_proc,	 14,   61,   294,  192,  0,	   0,	   0,	   0,		 FR_ETCHED,0,	   NULL, NULL,  NULL },
	// 7
	{ jwin_ctext_proc,	  92,   92-20,   60,   8,	vc(7),   vc(11),  0,	   0,		 0,		0,	   str_a, NULL,  NULL },
	{ jwin_ctext_proc,	  92,   120-20,  60,   8,	vc(7),   vc(11),  0,	   0,		 0,		0,	   str_b, NULL,  NULL },
	{ jwin_ctext_proc,	  92,   148-20,  60,   8,	vc(7),   vc(11),  0,	   0,		 0,		0,	   str_s, NULL,  NULL },
	{ jwin_ctext_proc,	  92,   180-20,  60,   8,	vc(7),   vc(11),  0,	   0,		 0,		0,	   str_ex1, NULL,  NULL },
	{ jwin_ctext_proc,	  92,   212-20,  60,   8,	vc(7),   vc(11),  0,	   0,		 0,		0,	   str_ex3, NULL,  NULL },
	{ jwin_ctext_proc,	  237,  92-20,   60,   8,	vc(7),   vc(11),  0,	   0,		 0,		0,	   str_l, NULL,  NULL },
	{ jwin_ctext_proc,	  237,  120-20,  60,   8,	vc(7),   vc(11),  0,	   0,		 0,		0,	   str_r, NULL,  NULL },
	{ jwin_ctext_proc,	  237,  148-20,  60,   8,	vc(7),   vc(11),  0,	   0,		 0,		0,	   str_p, NULL,  NULL },
	{ jwin_ctext_proc,	  237,  180-20,  60,   8,	vc(7),   vc(11),  0,	   0,		 0,		0,	   str_ex2, NULL,  NULL },
	{ jwin_ctext_proc,	  237,  212-20,  60,   8,	vc(7),   vc(11),  0,	   0,		 0,		0,	   str_ex4, NULL,  NULL },
	{ jwin_ctext_proc,	  92,   244-20,  60,   8,	vc(7),   vc(11),  0,	   0,		 0,		0,	   str_m, NULL,  NULL },
	// 18
	{ d_jbutton_proc,	  22,   90-20,   61,   21,   vc(14),  vc(1),   0,	   0,		 0,		0, (void *) "A",	 NULL, &Abtn},
	{ d_jbutton_proc,	  22,   118-20,  61,   21,   vc(14),  vc(1),   0,	   0,		 0,		0, (void *) "B",	 NULL, &Bbtn},
	{ d_jbutton_proc,	  22,   146-20,  61,   21,   vc(14),  vc(1),   0,	   0,		 0,		0, (void *) "Start", NULL, &Sbtn},
	{ d_jbutton_proc,	  22,   178-20,  61,   21,   vc(14),  vc(1),   0,	   0,		 0,		0, (void *) "X (EX1)",	 NULL, &Exbtn1},
	{ d_jbutton_proc,	  22,   210-20,  61,   21,   vc(14),  vc(1),   0,	   0,		 0,		0, (void *) "EX3", NULL, &Exbtn3},
	{ d_jbutton_proc,	  167,  90-20,   61,   21,   vc(14),  vc(1),   0,	   0,		 0,		0, (void *) "L",	 NULL, &Lbtn},
	{ d_jbutton_proc,	  167,  118-20,  61,   21,   vc(14),  vc(1),   0,	   0,		 0,		0, (void *) "R",	 NULL, &Rbtn},
	{ d_jbutton_proc,	  167,  146-20,  61,   21,   vc(14),  vc(1),   0,	   0,		 0,		0, (void *) "Map",   NULL, &Pbtn},
	{ d_jbutton_proc,	  167,  178-20,  61,   21,   vc(14),  vc(1),   0,	   0,		 0,		0, (void *) "Y (EX2)",	 NULL, &Exbtn2},
	{ d_jbutton_proc,	  167,  210-20,  61,   21,   vc(14),  vc(1),   0,	   0,		 0,		0, (void *) "EX4",   NULL, &Exbtn4},
	{ d_jbutton_proc,	  22,   242-20,  61,   21,   vc(14),  vc(1),   0,	   0,		 0,		0, (void *) "Menu",  NULL, &Mbtn},
	// 29
	{ d_j_clearbutton_proc,	  22+91,  90-20,   40,   21,   vc(14),  vc(1),   0,	   0,		 0,		0, (void *) "Clear",	 NULL, &Abtn},
	{ d_j_clearbutton_proc,	  22+91,  118-20,   40,   21,   vc(14),  vc(1),   0,	   0,		 0,		0, (void *) "Clear",	 NULL, &Bbtn},
	{ d_j_clearbutton_proc,	  22+91,  146-20,   40,   21,   vc(14),  vc(1),   0,	   0,		 0,		0, (void *) "Clear",	 NULL, &Sbtn},
	{ d_j_clearbutton_proc,	  22+91,  178-20,   40,   21,   vc(14),  vc(1),   0,	   0,		 0,		0, (void *) "Clear",	 NULL, &Exbtn1},
	{ d_j_clearbutton_proc,	  22+91,  210-20,   40,   21,   vc(14),  vc(1),   0,	   0,		 0,		0, (void *) "Clear",	 NULL, &Exbtn3},
	{ d_j_clearbutton_proc,	  167+91,  90-20,   40,   21,   vc(14),  vc(1),   0,	   0,		 0,		0, (void *) "Clear",	 NULL, &Lbtn},
	{ d_j_clearbutton_proc,	  167+91,  118-20,   40,   21,   vc(14),  vc(1),   0,	   0,		 0,		0, (void *) "Clear",	 NULL, &Rbtn},
	{ d_j_clearbutton_proc,	  167+91,  146-20,   40,   21,   vc(14),  vc(1),   0,	   0,		 0,		0, (void *) "Clear",	 NULL, &Pbtn},
	{ d_j_clearbutton_proc,	  167+91,  178-20,   40,   21,   vc(14),  vc(1),   0,	   0,		 0,		0, (void *) "Clear",	 NULL, &Exbtn2},
	{ d_j_clearbutton_proc,	  167+91,  210-20,   40,   21,   vc(14),  vc(1),   0,	   0,		 0,		0, (void *) "Clear",	 NULL, &Exbtn4},
	{ d_j_clearbutton_proc,	  22+91,  242-20,   40,   21,   vc(14),  vc(1),   0,	   0,		 0,		0, (void *) "Clear",	 NULL, &Mbtn},
	// 40
	{ jwin_frame_proc,	 14,   62,   147,  80,  0,	   0,	   0,	   0,		 FR_ETCHED,0,	   NULL, NULL,  NULL },
	{ jwin_frame_proc,	 159,  62,   147,  80,  0,	   0,	   0,	   0,		 FR_ETCHED,0,	   NULL, NULL,  NULL },
	{ jwin_text_proc,		 30,   68,   160,  8,	vc(0),   vc(11),  0,	   0,		 0,		0, (void *) "Vertical", NULL,  NULL },
	{ jwin_text_proc,		 175,  68,   160,  8,	vc(0),   vc(11),  0,	   0,		 0,		0, (void *) "Horizontal", NULL,  NULL },
	// 44
	{ jwin_text_proc,	  92,   84,   60,   8,	vc(7),   vc(11),  0,	   0,		 0,		0,	   str_up, NULL,  NULL },
	{ jwin_text_proc,	  92,   112,  60,   8,	vc(7),   vc(11),  0,	   0,		 0,		0,	   str_down, NULL,  NULL },
	{ jwin_text_proc,	  237,  84,   60,   8,	vc(7),   vc(11),  0,	   0,		 0,		0,	   str_left, NULL,  NULL },
	{ jwin_text_proc,	  237,  112,  60,   8,	vc(7),   vc(11),  0,	   0,		 0,		0,	   str_right, NULL,  NULL },
	// 48
	{ d_jbutton_proc,	  22,   82,   61,   21,   vc(14),  vc(11),  0,	   0,		 0,		0, (void *) "Up",	 NULL, &DUbtn },
	{ d_jbutton_proc,	  22,   110,  61,   21,   vc(14),  vc(1),   0,	   0,		 0,		0, (void *) "Down",   NULL, &DDbtn },
	{ d_jbutton_proc,	  167,  82,   61,   21,   vc(14),  vc(1),   0,	   0,		 0,		0, (void *) "Left",   NULL, &DLbtn },
	{ d_jbutton_proc,	  167,  110,  61,   21,   vc(14),  vc(1),   0,	   0,		 0,		0, (void *) "Right",  NULL, &DRbtn },
	// 52
	{ d_j_clearbutton_proc,	  22+91,  82,   40,   21,   vc(14),  vc(1),   0,	   0,		 0,		0, (void *) "Clear",	 NULL, &DUbtn},
	{ d_j_clearbutton_proc,	  22+91,  110,  40,   21,   vc(14),  vc(1),   0,	   0,		 0,		0, (void *) "Clear",	 NULL, &DDbtn},
	{ d_j_clearbutton_proc,	  167+91, 82,   40,   21,   vc(14),  vc(1),   0,	   0,		 0,		0, (void *) "Clear",   NULL, &DLbtn},
	{ d_j_clearbutton_proc,	  167+91, 110,  40,   21,   vc(14),  vc(1),   0,	   0,		 0,		0, (void *) "Clear",	 NULL, &DRbtn},
	// 56
	{ jwin_check_proc,	 22,   150,  147,  8,	vc(14),  vc(1),   0,	   0,		 1,		0, (void *) "Use Analog Stick/DPad", NULL, NULL },
	{ NULL,				 0,	0,	0,	0,   0,	   0,	   0,	   0,		  0,			 0,	   NULL,						   NULL,  NULL }
};

static int32_t keyboard_keys_list[] =
{
	6,7,8,9,10,
	11,12,13,14,15,16,17,18,19,20,
	21,22,23,24,25,26,27,28,29,30,
	31,32,33,34,35,36,37,38,39,40,
	-1
};

static int32_t keyboard_dirs_list[] =
{
	41,42,43,44,
	45,46,47,48,
	49,50,51,52,
	53,54,55,56,
	-1
};

static int32_t keyboard_mods_list[] =
{
	57,58,59,60,
	61,62,63,64,
	65,66,67,68,
	69,70,71,72,
	-1
};

static TABPANEL keyboard_control_tabs[] =
{
	// (text)
	{ (char *)"Keys",		D_SELECTED,  keyboard_keys_list, 0, NULL },
	{ (char *)"Directions",  0,		   keyboard_dirs_list, 0, NULL },
	{ (char *)"Cheat Mods",  0,		   keyboard_mods_list, 0, NULL },
	{ NULL,				  0,		   NULL,			   0, NULL }
};

static DIALOG keyboard_control_dlg[] =
{
	// (dialog proc)	   (x)   (y)   (w)   (h)   (fg)	 (bg)	 (key)	(flags)	(d1)	  (d2)	 (dp)	 (dp2) (dp3)
	{ jwin_win_proc,	   8,	39,   304,  240,  0,	   0,	   0,	   D_EXIT,	0,		0, (void *) "Keyboard Controls", NULL,  NULL },
	{ jwin_tab_proc,	   8+4, 39+23,304-8,240-56,vc(0),   vc(15),  0,	   0,		 0,		0, (void *) keyboard_control_tabs, NULL, (void *)keyboard_control_dlg },
	{ d_stringloader,	  0,	0,	0,	0,	0,	   0,	   0,	   0,		 0,		0, NULL, NULL,  NULL },
	{ jwin_button_proc,	90,   254,  61,   21,   0,	   0,	   0,	   D_EXIT,	0,		0, (void *) "OK", NULL,  NULL },
	{ jwin_button_proc,	170,  254,  61,   21,   0,	   0,	   0,	   D_EXIT,	0,		0, (void *) "Cancel", NULL,  NULL },
	{ d_timer_proc,		0,	0,	 0,	0,	0,	   0,	   0,	   0,		  0,		  0,		 NULL, NULL, NULL },
	// Keys
	// 6
	{ jwin_frame_proc,	 14,   80,   148,  105,  0,	   0,	   0,	   0,		 FR_ETCHED,0, NULL, NULL,  NULL },
	{ jwin_frame_proc,	 158,  80,   148,  105,  0,	   0,	   0,	   0,		 FR_ETCHED,0, NULL, NULL,  NULL },
	{ jwin_frame_proc,	 14,   181,  292,  67,   0,	   0,	   0,	   0,		 FR_ETCHED,0, NULL, NULL,  NULL },
	{ jwin_text_proc,	  30,   86,   160,  8,	vc(0),   vc(11),  0,	   0,		 0,		0, (void *) "Standard", NULL,  NULL },
	{ jwin_text_proc,	  175,  86,   160,  8,	vc(0),   vc(11),  0,	   0,		 0,		0, (void *) "Extended", NULL,  NULL },
	// 11
	{ jwin_text_proc,	  92-4,   102,  60,   8,	vc(7),   vc(11),  0,	   0,		 0,		0,	   str_a, NULL,  NULL },
	{ jwin_text_proc,	  92-4,   130,  60,   8,	vc(7),   vc(11),  0,	   0,		 0,		0,	   str_b, NULL,  NULL },
	{ jwin_text_proc,	  92-4,   158,  60,   8,	vc(7),   vc(11),  0,	   0,		 0,		0,	   str_s, NULL,  NULL },
	{ jwin_text_proc,	  92-4,   190,  60,   8,	vc(7),   vc(11),  0,	   0,		 0,		0,	   str_ex1, NULL,  NULL },
	{ jwin_text_proc,	  92-4,   222,  60,   8,	vc(7),   vc(11),  0,	   0,		 0,		0,	   str_ex3, NULL,  NULL },
	{ jwin_text_proc,	  237-4,  102,  60,   8,	vc(7),   vc(11),  0,	   0,		 0,		0,	   str_l, NULL,  NULL },
	{ jwin_text_proc,	  237-4,  130,  60,   8,	vc(7),   vc(11),  0,	   0,		 0,		0,	   str_r, NULL,  NULL },
	{ jwin_text_proc,	  237-4,  158,  60,   8,	vc(7),   vc(11),  0,	   0,		 0,		0,	   str_p, NULL,  NULL },
	{ jwin_text_proc,	  237-4,  190,  60,   8,	vc(7),   vc(11),  0,	   0,		 0,		0,	   str_ex2, NULL,  NULL },
	{ jwin_text_proc,	  237-4,  222,  60,   8,	vc(7),   vc(11),  0,	   0,		 0,		0,	   str_ex4, NULL,  NULL },
	// 21
	{ d_kbutton_proc,	  22,   100,  61,   21,   vc(14),  vc(1),   0,	   0,		 0,		0, (void *) "A",	 NULL, &Akey},
	{ d_kbutton_proc,	  22,   128,  61,   21,   vc(14),  vc(1),   0,	   0,		 0,		0, (void *) "B",	 NULL, &Bkey},
	{ d_kbutton_proc,	  22,   156,  61,   21,   vc(14),  vc(1),   0,	   0,		 0,		0, (void *) "Start", NULL, &Skey},
	{ d_kbutton_proc,	  22,   188,  61,   21,   vc(14),  vc(1),   0,	   0,		 0,		0, (void *) "X (EX1)",	 NULL, &Exkey1},
	{ d_kbutton_proc,	  22,   220,  61,   21,   vc(14),  vc(1),   0,	   0,		 0,		0, (void *) "EX3", NULL, &Exkey3},
	{ d_kbutton_proc,	  167,  100,  61,   21,   vc(14),  vc(1),   0,	   0,		 0,		0, (void *) "L",	 NULL, &Lkey},
	{ d_kbutton_proc,	  167,  128,  61,   21,   vc(14),  vc(1),   0,	   0,		 0,		0, (void *) "R",	 NULL, &Rkey},
	{ d_kbutton_proc,	  167,  156,  61,   21,   vc(14),  vc(1),   0,	   0,		 0,		0, (void *) "Map",   NULL, &Pkey},
	{ d_kbutton_proc,	  167,  188,  61,   21,   vc(14),  vc(1),   0,	   0,		 0,		0, (void *) "Y (EX2)",	 NULL, &Exkey2},
	{ d_kbutton_proc,	  167,  220,  61,   21,   vc(14),  vc(1),   0,	   0,		 0,		0, (void *) "EX4",   NULL, &Exkey4},
	// 31
	{ d_k_clearbutton_proc,	  22+91,  100,  40,   21,   vc(14),  vc(1),   0,	   0,		 0,		0, (void *) "Clear",	 NULL, &Akey},
	{ d_k_clearbutton_proc,	  22+91,  128,   40,   21,   vc(14),  vc(1),   0,	   0,		 0,		0, (void *) "Clear",	 NULL, &Bkey},
	{ d_k_clearbutton_proc,	  22+91,  156,   40,   21,   vc(14),  vc(1),   0,	   0,		 0,		0, (void *) "Clear",	 NULL, &Skey},
	{ d_k_clearbutton_proc,	  22+91,  188,   40,   21,   vc(14),  vc(1),   0,	   0,		 0,		0, (void *) "Clear",	 NULL, &Exkey1},
	{ d_k_clearbutton_proc,	  22+91,  220,   40,   21,   vc(14),  vc(1),   0,	   0,		 0,		0, (void *) "Clear",	 NULL, &Exkey3},
	{ d_k_clearbutton_proc,	  167+91, 100,   40,   21,   vc(14),  vc(1),   0,	   0,		 0,		0, (void *) "Clear",	 NULL, &Lkey},
	{ d_k_clearbutton_proc,	  167+91, 128,   40,   21,   vc(14),  vc(1),   0,	   0,		 0,		0, (void *) "Clear",	 NULL, &Rkey},
	{ d_k_clearbutton_proc,	  167+91, 156,   40,   21,   vc(14),  vc(1),   0,	   0,		 0,		0, (void *) "Clear",	 NULL, &Pkey},
	{ d_k_clearbutton_proc,	  167+91, 188,   40,   21,   vc(14),  vc(1),   0,	   0,		 0,		0, (void *) "Clear",	 NULL, &Exkey2},
	{ d_k_clearbutton_proc,	  167+91, 220,   40,   21,   vc(14),  vc(1),   0,	   0,		 0,		0, (void *) "Clear",	 NULL, &Exkey4},
	// Dirs
	// 41
	{ jwin_frame_proc,	 14,   80,   147,  80,   0,	   0,	   0,	   0,		 FR_ETCHED,0,	   NULL, NULL,  NULL },
	{ jwin_frame_proc,	 159,  80,   147,  80,   0,	   0,	   0,	   0,		 FR_ETCHED,0,	   NULL, NULL,  NULL },
	{ jwin_text_proc,		 30,   86,   160,  8,	vc(0),   vc(11),  0,	   0,		 0,		0, (void *) "Vertical", NULL,  NULL },
	{ jwin_text_proc,		 175,  86,   160,  8,	vc(0),   vc(11),  0,	   0,		 0,		0, (void *) "Horizontal", NULL,  NULL },
	// 45
	{ jwin_text_proc,	  92-4,   102,  60,   8,	vc(7),   vc(11),  0,	   0,		 0,		0,	   str_up, NULL,  NULL },
	{ jwin_text_proc,	  92-4,   130,  60,   8,	vc(7),   vc(11),  0,	   0,		 0,		0,	   str_down, NULL,  NULL },
	{ jwin_text_proc,	  237-4,  102,  60,   8,	vc(7),   vc(11),  0,	   0,		 0,		0,	   str_left, NULL,  NULL },
	{ jwin_text_proc,	  237-4,  130,  60,   8,	vc(7),   vc(11),  0,	   0,		 0,		0,	   str_right, NULL,  NULL },
	// 49
	{ d_kbutton_proc,	  22,   100,  61,   21,   vc(14),  vc(11),  0,	   0,		 0,		0, (void *) "Up",	 NULL, &DUkey},
	{ d_kbutton_proc,	  22,   128,  61,   21,   vc(14),  vc(1),   0,	   0,		 0,		0, (void *) "Down",   NULL, &DDkey},
	{ d_kbutton_proc,	  167,  100,  61,   21,   vc(14),  vc(1),   0,	   0,		 0,		0, (void *) "Left",   NULL, &DLkey},
	{ d_kbutton_proc,	  167,  128,  61,   21,   vc(14),  vc(1),   0,	   0,		 0,		0, (void *) "Right",  NULL, &DRkey},
	// 53
	{ d_k_clearbutton_proc,	  22+91,  100,   40,   21,   vc(14),  vc(1),   0,	   0,		 0,		0, (void *) "Clear",	 NULL, &DUkey},
	{ d_k_clearbutton_proc,	  22+91,  128,   40,   21,   vc(14),  vc(1),   0,	   0,		 0,		0, (void *) "Clear",	 NULL, &DDkey},
	{ d_k_clearbutton_proc,	  167+91, 100,   40,   21,   vc(14),  vc(1),   0,	   0,		 0,		0, (void *) "Clear",	 NULL, &DLkey},
	{ d_k_clearbutton_proc,	  167+91, 128,   40,   21,   vc(14),  vc(1),   0,	   0,		 0,		0, (void *) "Clear",	 NULL, &DRkey},
	// Mods
	// 57
	{ jwin_frame_proc,	 14,   80,   148,  140-58,  0,	   0,	   0,	   0,		 FR_ETCHED,0,	   NULL, NULL,  NULL },
	{ jwin_frame_proc,	 158,  80,   148,  140-58,  0,	   0,	   0,	   0,		 FR_ETCHED,0,	   NULL, NULL,  NULL },
	{ jwin_text_proc,	  30,   86,   160,  8,	vc(0),   vc(11),  0,	   0,		 0,		0, (void *) "Left", NULL,  NULL },
	{ jwin_text_proc,	  175,  86,   160,  8,	vc(0),   vc(11),  0,	   0,		 0,		0, (void *) "Right", NULL,  NULL },
	// 61
	{ jwin_text_proc,	  92-26,   101,  60,   8,	vc(7),   vc(11),  0,	   0,		 0,		0,	   str_leftmod1, NULL,  NULL },
	{ jwin_text_proc,	  92-26,   129,  60,   8,	vc(7),   vc(11),  0,	   0,		 0,		0,	   str_rightmod1, NULL,  NULL },
	{ jwin_text_proc,	  237-4-22,101,  60,   8,	vc(7),   vc(11),  0,	   0,		 0,		0,	   str_leftmod2, NULL,  NULL },
	{ jwin_text_proc,	  237-4-22,129,  60,   8,	vc(7),   vc(11),  0,	   0,		 0,		0,	   str_rightmod2, NULL,  NULL },
	// 65
	{ d_kbutton_proc,	  22,   100,  40,   21,   vc(14),  vc(1),   0,	   0,		 0,		0, (void *) "Main",	 NULL, &cheat_modifier_keys[0]},
	{ d_kbutton_proc,	  22,   128,  40,   21,   vc(14),  vc(1),   0,	   0,		 0,		0, (void *) "Second",	 NULL, &cheat_modifier_keys[2]},
	{ d_kbutton_proc,	  167,  100,  40,   21,   vc(14),  vc(1),   0,	   0,		 0,		0, (void *) "Main", NULL, &cheat_modifier_keys[1]},
	{ d_kbutton_proc,	  167,  128,  40,   21,   vc(14),  vc(1),   0,	   0,		 0,		0, (void *) "Second",	 NULL, &cheat_modifier_keys[3]},
	// 69
	{ d_k_clearbutton_proc,	  22+91,  100,  40,   21,   vc(14),  vc(1),   0,	   0,		 0,		0, (void *) "Clear",	 NULL, &cheat_modifier_keys[0]},
	{ d_k_clearbutton_proc,	  22+91,  128,  40,   21,   vc(14),  vc(1),   0,	   0,		 0,		0, (void *) "Clear",	 NULL, &cheat_modifier_keys[2]},
	{ d_k_clearbutton_proc,	  167+91, 100,  40,   21,   vc(14),  vc(1),   0,	   0,		 0,		0, (void *) "Clear",   NULL, &cheat_modifier_keys[1]},
	{ d_k_clearbutton_proc,	  167+91, 128,  40,   21,   vc(14),  vc(1),   0,	   0,		 0,		0, (void *) "Clear",	 NULL, &cheat_modifier_keys[3]},
	// 73
	{ NULL,				 0,	0,	0,	0,   0,	   0,	   0,	   0,		  0,			 0,	   NULL,						   NULL,  NULL }
};

/*
int32_t midi_dp[3] = {0,147,104};
int32_t digi_dp[3] = {1,147,120};
int32_t pan_dp[3]  = {0,147,136};
int32_t buf_dp[3]  = {0,147,152};
*/
int32_t midi_dp[3] = {0,0,0};
int32_t digi_dp[3] = {1,0,0};
int32_t emus_dp[3] = {2,0,0};
int32_t buf_dp[3]  = {0,0,0};
int32_t sfx_dp[3]  = {3,0,0};
int32_t pan_dp[3]  = {0,0,0};

static DIALOG sound_dlg[] =
{
	//(dialog proc)		  (x)	 (y)	(w)	 (h)	(fg)	   (bg)				 (key) (flags)	   (d1)		   (d2) (dp)						   (dp2)			   (dp3)
	{ jwin_win_proc,		  0,	  0,	320,	178,	0,		 0,				   0,	D_EXIT,	   0,			 0, (void *) "Sound Settings",	  NULL,			   NULL	 },
	{ d_stringloader,		 0,	  0,	  2,	  0,	0,		 0,				   0,	0,			0,			 0,  NULL,						   NULL,			   NULL	 },
	{ jwin_button_proc,	  58,	148,	 61,	 21,	0,		 0,				   0,	D_EXIT,	   0,			 0, (void *) "OK",				  NULL,			   NULL	 },
	{ jwin_button_proc,	 138,	148,	 61,	 21,	0,		 0,				   0,	D_EXIT,	   0,			 0, (void *) "Cancel",			  NULL,			   NULL	 },
	{ d_timer_proc,		   0,	  0,	  0,	  0,	0,		 0,				   0,	0,			0,			 0,  NULL,						   NULL,			   NULL	 },
	{ jwin_frame_proc,	   10,	 28,	300,	112,	0,		 0,				   0,	0,			FR_ETCHED,	 0,  NULL,						   NULL,			   NULL	 },
	{ jwin_rtext_proc,	  190,	 40,	 40,	  8,	vc(7),	 vc(11),			  0,	0,			0,			 0, (void *) str_a,				 NULL,			   NULL	 },
	{ jwin_rtext_proc,	  190,	 56,	 40,	  8,	vc(7),	 vc(11),			  0,	0,			0,			 0, (void *) str_b,				 NULL,			   NULL	 },
	{ jwin_rtext_proc,	  190,	 72,	 40,	  8,	vc(7),	 vc(11),			  0,	0,			0,			 0, (void *) str_l,				 NULL,			   NULL	 },
	{ jwin_rtext_proc,	  190,	 88,	 40,	  8,	vc(7),	 vc(11),			  0,	0,			0,			 0, (void *) str_m,				 NULL,			   NULL	 },
	// 10
	{ jwin_rtext_proc,	  190,	104,	 40,	  8,	vc(7),	 vc(11),			  0,	0,			0,			 0, (void *) str_r,				 NULL,			   NULL	 },
	{ jwin_rtext_proc,	  190,	120,	 40,	  8,	vc(7),	 vc(11),			  0,	0,			0,			 0, (void *) str_s,				 NULL,			   NULL	 },
	{ d_dummy_proc,		   0,	  0,	  0,	  0,	0,		 0,				   0,	0,			0,			 0,  NULL,						   NULL,			   NULL	 },
	{ d_dummy_proc,		   0,	  0,	  0,	  0,	0,		 0,				   0,	0,			0,			 0,  NULL,						   NULL,			   NULL	 },
	{ d_dummy_proc,		   0,	  0,	  0,	  0,	0,		 0,				   0,	0,			0,			 0,  NULL,						   NULL,			   NULL	 },
	{ jwin_slider_proc,	 196,	 40,	 96,	  8,	vc(0),	 jwin_pal[jcBOX],	 0,	0,		   32,			 0,  NULL, (void *) set_vol,   midi_dp  },
	{ jwin_slider_proc,	 196,	 56,	 96,	  8,	vc(0),	 jwin_pal[jcBOX],	 0,	0,		   32,			 0,  NULL, (void *) set_vol,   digi_dp  },
	{ jwin_slider_proc,	 196,	 72,	 96,	  8,	vc(0),	 jwin_pal[jcBOX],	 0,	0,		   32,			 0,  NULL, (void *) set_vol,   emus_dp  },
	{ jwin_slider_proc,	 196,	 88,	 96,	  8,	vc(0),	 jwin_pal[jcBOX],	 0,	0,		  127,			 0,  NULL, (void *) set_buf,   buf_dp   },
	{ jwin_slider_proc,	 196,	104,	 96,	  8,	vc(0),	 jwin_pal[jcBOX],	 0,	0,		   32,			 0,  NULL, (void *) set_vol,   sfx_dp   },
	//20
	{ jwin_slider_proc,	 196,	120,	 96,	  8,	vc(0),	 jwin_pal[jcBOX],	 0,	0,			3,			 0,  NULL, (void *) set_pan,   pan_dp   },
	{ d_dummy_proc,		   0,	  0,	  0,	  0,	0,		 0,				   0,	0,			0,			 0,  NULL,						   NULL,			   NULL	 },
	{ d_dummy_proc,		   0,	  0,	  0,	  0,	0,		 0,				   0,	0,			0,			 0,  NULL,						   NULL,			   NULL	 },
	{ d_dummy_proc,		   0,	  0,	  0,	  0,	0,		 0,				   0,	0,			0,			 0,  NULL,						   NULL,			   NULL	 },
	{ jwin_text_proc,		17,	 40,	 48,	  8,	vc(0),	 vc(11),			  0,	0,			0,			 0, (void *) "Master MIDI Volume",  NULL,			   NULL	 },
	{ jwin_text_proc,		17,	 56,	 48,	  8,	vc(0),	 vc(11),			  0,	0,			0,			 0, (void *) "Master Digi Volume",  NULL,			   NULL	 },
	{ jwin_text_proc,		17,	 72,	 48,	  8,	vc(0),	 vc(11),			  0,	0,			0,			 0, (void *) "Enhanced Music Volume",	 NULL,			   NULL	 },
	{ jwin_text_proc,		17,	 88,	 48,	  8,	vc(0),	 vc(11),			  0,	0,			0,			 0, (void *) "Enhanced Music Buffer",	 NULL,			   NULL	 },
	{ jwin_text_proc,		17,	104,	 48,	  8,	vc(0),	 vc(11),			  0,	0,			0,			 0, (void *) "SFX Volume",		  NULL,			   NULL	 },
	{ jwin_text_proc,		17,	120,	 48,	  8,	vc(0),	 vc(11),			  0,	0,			0,			 0, (void *) "SFX Pan",			 NULL,			   NULL	 },
	//30
	{ d_dummy_proc,		   0,	  0,	  0,	  0,	0,		 0,				   0,	0,			0,			 0,  NULL,						   NULL,			   NULL	 },
	{ d_dummy_proc,		   0,	  0,	  0,	  0,	0,		 0,				   0,	0,			0,			 0,  NULL,						   NULL,			   NULL	 },
	{ d_dummy_proc,		   0,	  0,	  0,	  0,	0,		 0,				   0,	0,			0,			 0,  NULL,						   NULL,			   NULL	 },
	{ NULL,				   0,	  0,	  0,	  0,	0,		 0,				   0,	0,			0,			 0,  NULL,						   NULL,			   NULL	 },
};

char zc_builddate[80];
char zc_aboutstr[80];

static DIALOG about_dlg[] =
{
	/* (dialog proc)	   (x)   (y)   (w)   (h)   (fg)	 (bg)	 (key)	(flags)	(d1)	  (d2)	 (dp)	 (dp2) (dp3) */
	{ jwin_win_proc,	   68,   52,   184,  154,  0,	   0,	   0,	   D_EXIT,	0,		0, (void *) "About", NULL,  NULL },
	{ jwin_button_proc,	140,  176,  41,   21,   vc(14),  0,	   0,	   D_EXIT,	0,		0, (void *) "OK", NULL,  NULL },
	{ jwin_ctext_proc,		160,  84,   0,	8,	vc(0),   vc(11),  0,	   0,		 0,		0, zc_aboutstr, NULL,  NULL },
	{ jwin_ctext_proc,		160,  92,   0,	8,	vc(0) ,  vc(11),  0,	   0,		 0,		0, str_s, NULL,  NULL },
	{ jwin_ctext_proc,		160,  100,  0,	8,	vc(0) ,  vc(11),  0,	   0,		 0,		0, zc_builddate, NULL,  NULL },
	{ jwin_text_proc,		 88,   124,  140,  8,	vc(0),   vc(11),  0,	   0,		 0,		0, (void *) "Coded by:", NULL,  NULL },
	{ jwin_text_proc,		 88,   132,  140,  8,	vc(0),   vc(11),  0,	   0,		 0,		0, (void *) "  Phantom Menace", NULL,  NULL },
	{ jwin_text_proc,		 88,   144,  140,  8,	vc(0),   vc(11),  0,	   0,		 0,		0, (void *) "Produced by:", NULL,  NULL },
	{ jwin_text_proc,		 88,   152,  140,  8,	vc(0),   vc(11),  0,	   0,		 0,		0, (void *) "  Armageddon Games", NULL,  NULL },
	{ jwin_frame_proc,	 80,   117,  160,  50,   0,	   0,	   0,	   0,		 FR_ETCHED,0,	   NULL, NULL,  NULL },
	{ d_timer_proc,		 0,	0,	 0,	0,	0,	   0,	   0,	   0,		  0,		  0,		 NULL, NULL, NULL },
	{ NULL,				 0,	0,	0,	0,   0,	   0,	   0,	   0,		  0,			 0,	   NULL,						   NULL,  NULL }
};


static DIALOG quest_dlg[] =
{
	/* (dialog proc)	   (x)   (y)   (w)   (h)   (fg)	 (bg)	 (key)	(flags)	(d1)	  (d2)	 (dp)	 (dp2) (dp3) */
	{ jwin_win_proc,	   68,   25,   184,  190,  0,	   0,	   0,	   D_EXIT,	0,		0, (void *) "Quest Info", NULL,  NULL },
	{ jwin_edit_proc,	  84,   54,   152,  16,   0,	   0,	   0,	   D_READONLY, 100,	 0,	   NULL, NULL,  NULL },
	{ jwin_text_proc,		 89,   84,   141,  8,	vc(0),   vc(11),  0,	   0,		 0,		0, (void *) "Number:", NULL,  NULL },
	{ jwin_text_proc,		 152,  84,   24,   8,	vc(7),   vc(11),  0,	   0,		 0,		0,	   str_a, NULL,  NULL },
	{ jwin_text_proc,		 89,   94,   141,  8,	vc(0),   vc(11),  0,	   0,		 0,		0, (void *) "Version:", NULL,  NULL },
	{ jwin_text_proc,		 160,  94,   64,   8,	vc(7),   vc(11),  0,	   0,		 0,		0,	   QHeader.version, NULL,  NULL },
	{ jwin_text_proc,		 89,   104,  141,  8,	vc(0),   vc(11),  0,	   0,		 0,		0, (void *) "ZQ Version:", NULL,  NULL },
	{ jwin_text_proc,		 184,  104,  64,   8,	vc(7),   vc(11),  0,	   0,		 0,		0,	   str_s, NULL,  NULL },
	{ jwin_text_proc,		 84,   126,  80,   8,	vc(0),   vc(11),  0,	   0,		 0,		0, (void *) "Title:", NULL,  NULL },
	{ jwin_textbox_proc,   84,   136,  152,  24,   0,	   0,	   0,	   0,		 0,		0,	   QHeader.title, NULL,  NULL },
	{ jwin_text_proc,		 84,   168,  80,   8,	vc(0),   vc(11),  0,	   0,		 0,		0, (void *) "Author:", NULL,  NULL },
	{ jwin_textbox_proc,   84,   178,  152,  24,   0,	   0,	   0,	   0,		 0,		0,	   QHeader.author, NULL,  NULL },
	{ jwin_frame_proc,	 84,   79,   152,  38,   0,	   0,	   0,	   0,		 FR_ETCHED,0,	   NULL, NULL,  NULL },
	{ d_timer_proc,		 0,	0,	 0,	0,	0,	   0,	   0,	   0,		  0,		  0,		 NULL, NULL, NULL },
	{ NULL,				 0,	0,	0,	0,   0,	   0,	   0,	   0,		  0,			 0,	   NULL,						   NULL,  NULL }
};

static DIALOG triforce_dlg[] =
{
	/* (dialog proc)	 (x)   (y)   (w)   (h)   (fg)	 (bg)	(key)	(flags)	 (d1)		   (d2)	 (dp) */
	{ jwin_win_proc,	  72,	64,  177,  105,  vc(14),   vc(1),	0,  D_EXIT,		   0,		0, (void *) "Triforce Pieces", NULL,  NULL },
	// 1
	{ jwin_check_proc,   129,	94,   24,	9,   vc(0),  vc(11),	0,	   0,		   1,		0, (void *) "1", NULL,  NULL },
	{ jwin_check_proc,   129,   104,   24,	9,   vc(0),  vc(11),	0,	   0,		   1,		0, (void *) "2", NULL,  NULL },
	{ jwin_check_proc,   129,   114,   24,	9,   vc(0),  vc(11),	0,	   0,		   1,		0, (void *) "3", NULL,  NULL },
	{ jwin_check_proc,   129,   124,   24,	9,   vc(0),  vc(11),	0,	   0,		   1,		0, (void *) "4", NULL,  NULL },
	{ jwin_check_proc,   172,	94,   24,	9,   vc(0),  vc(11),	0,	   0,		   1,		0, (void *) "5", NULL,  NULL },
	{ jwin_check_proc,   172,   104,   24,	9,   vc(0),  vc(11),	0,	   0,		   1,		0, (void *) "6", NULL,  NULL },
	{ jwin_check_proc,   172,   114,   24,	9,   vc(0),  vc(11),	0,	   0,		   1,		0, (void *) "7", NULL,  NULL },
	{ jwin_check_proc,   172,   124,   24,	9,   vc(0),  vc(11),	0,	   0,		   1,		0, (void *) "8", NULL,  NULL },
	// 9
	{ jwin_button_proc,  90,	144,   61,   21,   vc(0),  vc(11),  'k',  D_EXIT,		   0,		0, (void *) "O&K", NULL,  NULL },
	{ jwin_button_proc,  170,   144,   61,   21,   vc(0),  vc(11),   27,  D_EXIT,		   0,		0, (void *) "Cancel", NULL,  NULL },
	{ d_timer_proc,		 0,	0,	 0,	0,	0,	   0,	   0,	   0,		  0,		  0,		 NULL, NULL, NULL },
	{ NULL,				 0,	0,	0,	0,   0,	   0,	   0,	   0,		  0,			 0,	   NULL,						   NULL,  NULL }
};

/*static DIALOG equip_dlg[] =
{
  // (dialog proc)	 (x)   (y)   (w)   (h)   (fg)	 (bg)	(key)	(flags)	 (d1)		   (d2)	 (dp)
  { jwin_win_proc,	  16,	18,  289,  215,  vc(14),   vc(1),	0,  D_EXIT,		   0,		0,	 (void *) "Equipment", NULL,  NULL },
  // 1
  { jwin_button_proc,   90,   206,   61,   21,   vc(0),  vc(11),  'k',  D_EXIT,		   0,		0,	 (void *) "O&K", NULL,  NULL },
  { jwin_button_proc,  170,   206,   61,   21,   vc(0),  vc(11),   27,  D_EXIT,		   0,		0,	 (void *) "Cancel", NULL,  NULL },
  // 3
  { jwin_frame_proc,	25,	45,   77,   50,	   0,	   0,	0,	   0,   FR_ETCHED,		0,	 NULL, NULL,  NULL },
  { jwin_text_proc,		29,	42,   40,	8,   vc(0),  vc(11),	0,	   0,		   0,		0,	 (void *) "Sword", NULL,  NULL },
  { jwin_check_proc,	33,	52,   64,	9,   vc(0),  vc(11),	0,	   0,		   1,		0,	 (void *) "Wooden", NULL,  NULL },
  { jwin_check_proc,	33,	62,   64,	9,   vc(0),  vc(11),	0,	   0,		   1,		0,	 (void *) "White", NULL,  NULL },
  { jwin_check_proc,	33,	72,   64,	9,   vc(0),  vc(11),	0,	   0,		   1,		0,	 (void *) "Magic", NULL,  NULL },
  { jwin_check_proc,	33,	82,   64,	9,   vc(0),  vc(11),	0,	   0,		   1,		0,	 (void *) "Master", NULL,  NULL },
  // 9
  { jwin_frame_proc,	25,	99,   77,   40,	   0,	   0,	0,	   0,   FR_ETCHED,		0,	 NULL, NULL,  NULL },
  { jwin_text_proc,		29,	96,   48,	8,   vc(0),  vc(11),	0,	   0,		   0,		0,	 (void *) "Shield", NULL,  NULL },
  { jwin_check_proc,	33,   106,   64,	9,   vc(0),  vc(11),	0,	   0,		   1,		0,	 (void *) "Small", NULL,  NULL },
  { jwin_check_proc,	33,   116,   64,	9,   vc(0),  vc(11),	0,	   0,		   1,		0,	 (void *) "Magic", NULL,  NULL },
  { jwin_check_proc,	33,   126,   64,	9,   vc(0),  vc(11),	0,	   0,		   1,		0,	 (void *) "Mirror", NULL,  NULL },
  // 14
  { jwin_frame_proc,	25,   143,   61,   40,	   0,	   0,	0,	   0,   FR_ETCHED,		0,	 NULL, NULL,  NULL },
  { jwin_text_proc,		29,   140,   48,	8,   vc(0),  vc(11),	0,	   0,		   0,		0,	 (void *) "Ring", NULL,  NULL },
  { jwin_check_proc,	33,   150,   48,	9,   vc(0),  vc(11),	0,	   0,		   1,		0,	 (void *) "Blue", NULL,  NULL },
  { jwin_check_proc,	33,   160,   48,	9,   vc(0),  vc(11),	0,	   0,		   1,		0,	 (void *) "Red", NULL,  NULL },
  { jwin_check_proc,	33,   170,   48,	9,   vc(0),  vc(11),	0,	   0,		   1,		0,	 (void *) "Gold", NULL,  NULL },
  // 19
  { jwin_frame_proc,   110,	45,   85,   30,	   0,	   0,	0,	   0,   FR_ETCHED,		0,	 NULL, NULL,  NULL },
  { jwin_text_proc,	   114,	42,   64,	8,   vc(0),  vc(11),	0,	   0,		   0,		0,	 (void *) "Bracelet", NULL,  NULL },
  { jwin_check_proc,   118,	52,   72,	9,   vc(0),  vc(11),	0,	   0,		   1,		0,	 (void *) "Level 1", NULL,  NULL },
  { jwin_check_proc,   118,	62,   72,	9,   vc(0),  vc(11),	0,	   0,		   1,		0,	 (void *) "Level 2", NULL,  NULL },
  // 23
  { jwin_frame_proc,   110,	79,   85,   30,	   0,	   0,	0,	   0,   FR_ETCHED,		0,	 NULL, NULL,  NULL },
  { jwin_text_proc,	   114,	76,   48,	8,   vc(0),  vc(11),	0,	   0,		   0,		0,	 (void *) "Amulet", NULL,  NULL },
  { jwin_check_proc,   118,	86,   72,	9,   vc(0),  vc(11),	0,	   0,		   1,		0,	 (void *) "Level 1", NULL,  NULL },
  { jwin_check_proc,   118,	96,   72,	9,   vc(0),  vc(11),	0,	   0,		   1,		0,	 (void *) "Level 2", NULL,  NULL },
  // 27
  { jwin_frame_proc,   110,   113,   69,   30,	   0,	   0,	0,	   0,   FR_ETCHED,		0,	 NULL, NULL,  NULL },
  { jwin_text_proc,	   114,   110,   48,	8,   vc(0),  vc(11),	0,	   0,		   0,		0,	 (void *) "Wallet", NULL,  NULL },
  { jwin_check_proc,   118,   120,   56,	9,   vc(0),  vc(11),	0,	   0,		   1,		0,	 (void *) "Small", NULL,  NULL },
  { jwin_check_proc,   118,   130,   56,	9,   vc(0),  vc(11),	0,	   0,		   1,		0,	 (void *) "Large", NULL,  NULL },
  // 31
  { jwin_frame_proc,   110,   147,   69,   30,	   0,	   0,	0,	   0,   FR_ETCHED,		0,	 NULL, NULL,  NULL },
  { jwin_text_proc,	   114,   144,   24,	8,   vc(0),  vc(11),	0,	   0,		   0,		0,	 (void *) "Bow", NULL,  NULL },
  { jwin_check_proc,   118,   154,   56,	9,   vc(0),  vc(11),	0,	   0,		   1,		0,	 (void *) "Small", NULL,  NULL },
  { jwin_check_proc,   118,   164,   56,	9,   vc(0),  vc(11),	0,	   0,		   1,		0,	 (void *) "Large", NULL,  NULL },
  // 35
  { jwin_frame_proc,   203,	45,   93,   70,	   0,	   0,	0,	   0,   FR_ETCHED,		0,	 NULL, NULL,  NULL },
  { jwin_text_proc,	   207,	42,   40,	8,   vc(0),  vc(11),	0,	   0,		   0,		0,	 (void *) "Other", NULL,  NULL },
  { jwin_check_proc,   211,	52,   80,	9,   vc(0),  vc(11),	0,	   0,		   1,		0,	 (void *) "Raft", NULL,  NULL },
  { jwin_check_proc,   211,	62,   80,	9,   vc(0),  vc(11),	0,	   0,		   1,		0,	 (void *) "Ladder", NULL,  NULL },
  { jwin_check_proc,   211,	72,   80,	9,   vc(0),  vc(11),	0,	   0,		   1,		0,	 (void *) "Book", NULL,  NULL },
  { jwin_check_proc,   211,	82,   80,	9,   vc(0),  vc(11),	0,	   0,		   1,		0,	 (void *) "Magic Key", NULL,  NULL },
  { jwin_check_proc,   211,	92,   80,	9,   vc(0),  vc(11),	0,	   0,		   1,		0,	 (void *) "Flippers", NULL,  NULL },
  { jwin_check_proc,   211,   102,   80,	9,   vc(0),  vc(11),	0,	   0,		   1,		0,	 (void *) "Boots", NULL,  NULL },
  { d_timer_proc,		 0,	0,	 0,	0,	0,	   0,	   0,	   0,		  0,		  0,		 NULL, NULL, NULL },
  { NULL,				 0,	0,	0,	0,   0,	   0,	   0,	   0,		  0,			 0,	   NULL,						   NULL,  NULL }
};

static DIALOG items_dlg[] =
{
  // (dialog proc)	 (x)   (y)   (w)   (h)   (fg)	 (bg)	(key)	(flags)	 (d1)		   (d2)	 (dp)
  { jwin_win_proc,	  16,	18,  289,  215,  vc(14),   vc(1),	0,  D_EXIT,		   0,		0,	 (void *) "Items", NULL,  NULL },
  //1
  { jwin_button_proc,   90,   206,   61,   21,   vc(0),  vc(11),  'k',  D_EXIT,		   0,		0,	 (void *) "O&K", NULL,  NULL },
  { jwin_button_proc,  170,   206,   61,   21,   vc(0),  vc(11),   27,  D_EXIT,		   0,		0,	 (void *) "Cancel", NULL,  NULL },
  // 3
  { jwin_frame_proc,	27,	45,   77,   40,	   0,	   0,	0,	   0,   FR_ETCHED,		0,	 NULL, NULL,  NULL },
  { jwin_text_proc,		31,	42,   64,	9,   vc(0),  vc(11),	0,	   0,		   0,		0,	 (void *) "Boomerang", NULL,  NULL },
  { jwin_check_proc,	35,	52,   64,	9,   vc(0),  vc(11),	0,	   0,		   1,		0,	 (void *) "Wooden", NULL,  NULL },
  { jwin_check_proc,	35,	62,   64,	9,   vc(0),  vc(11),	0,	   0,		   1,		0,	 (void *) "Magic", NULL,  NULL },
  { jwin_check_proc,	35,	72,   64,	9,   vc(0),  vc(11),	0,	   0,		   1,		0,	 (void *) "Fire", NULL,  NULL },
  // 8
  { jwin_frame_proc,	27,	89,   77,   40,	   0,	   0,	0,	   0,   FR_ETCHED,		0,	 NULL, NULL,  NULL },
  { jwin_text_proc,		31,	86,   48,	9,   vc(0),  vc(11),	0,	   0,		   0,		0,	 (void *) "Arrow", NULL,  NULL },
  { jwin_check_proc,	35,	96,   64,	9,   vc(0),  vc(11),	0,	   0,		   1,		0,	 (void *) "Wooden", NULL,  NULL },
  { jwin_check_proc,	35,   106,   64,	9,   vc(0),  vc(11),	0,	   0,		   1,		0,	 (void *) "Silver", NULL,  NULL },
  { jwin_check_proc,	35,   116,   64,	9,   vc(0),  vc(11),	0,	   0,		   1,		0,	 (void *) "Golden", NULL,  NULL },
  // 13
  { jwin_frame_proc,	27,   133,   63,   40,	   0,	   0,	0,	   0,   FR_ETCHED,		0,	 NULL, NULL,  NULL },
  { jwin_text_proc,		31,   130,   48,	9,   vc(0),  vc(11),	0,	   0,		   0,		0,	 (void *) "Potion", NULL,  NULL },
  { jwin_radio_proc,	35,   140,   48,	9,   vc(0),  vc(11),	0,	   0,		   1,		0,	 (void *) "None", NULL,  NULL },
  { jwin_radio_proc,	35,   150,   48,	9,   vc(0),  vc(11),	0,	   0,		   1,		0,	 (void *) "Blue", NULL,  NULL },
  { jwin_radio_proc,	35,   160,   48,	9,   vc(0),  vc(11),	0,	   0,		   1,		0,	 (void *) "Red", NULL,  NULL },
  // 18
  { jwin_frame_proc,   114,	45,   93,   20,	   0,	   0,	0,	   0,   FR_ETCHED,		0,	 NULL, NULL,  NULL },
  { jwin_text_proc,	   118,	42,   48,	9,   vc(0),  vc(11),	0,	   0,		   0,		0,	 (void *) "Whistle", NULL,  NULL },
  { jwin_check_proc,   122,	52,   80,	9,   vc(0),  vc(11),	0,	   0,		   1,		0,	 (void *) "Recorder", NULL,  NULL },
  // 21
  { jwin_frame_proc,   114,	69,   86,   20,	   0,	   0,	0,	   0,   FR_ETCHED,		0,	 NULL, NULL,  NULL },
  { jwin_text_proc,	   118,	66,   48,	9,   vc(0),  vc(11),	0,	   0,		   0,		0,	 (void *) "Hammer", NULL,  NULL },
  { jwin_check_proc,   122,	76,   72,	9,   vc(0),  vc(11),	0,	   0,		   1,		0,	 (void *) "Level 1", NULL,  NULL },
  // 24
  { jwin_frame_proc,   114,	93,   69,   30,	   0,	   0,	0,	   0,   FR_ETCHED,		0,	 NULL, NULL,  NULL },
  { jwin_text_proc,	   118,	90,   48,	9,   vc(0),  vc(11),	0,	   0,		   0,		0,	 (void *) "Hookshot", NULL,  NULL },
  { jwin_check_proc,   122,   100,   56,	9,   vc(0),  vc(11),	0,	   0,		   1,		0,	 (void *) "Short", NULL,  NULL },
  { jwin_check_proc,   122,   110,   56,	9,   vc(0),  vc(11),	0,	   0,		   1,		0,	 (void *) "Long", NULL,  NULL },
  // 28
  { jwin_frame_proc,   114,   127,   60,   30,	   0,	   0,	0,	   0,   FR_ETCHED,		0,	 NULL, NULL,  NULL },
  { jwin_text_proc,	   118,   124,   48,	9,   vc(0),  vc(11),	0,	   0,		   0,		0,	 (void *) "Candle", NULL,  NULL },
  { jwin_check_proc,   122,   134,   48,	9,   vc(0),  vc(11),	0,	   0,		   1,		0,	 (void *) "Blue", NULL,  NULL },
  { jwin_check_proc,   122,   144,   48,	9,   vc(0),  vc(11),	0,	   0,		   1,		0,	 (void *) "Red", NULL,  NULL },
  // 32
  { jwin_frame_proc,   217,	45,   77,  138,	   0,	   0,	0,	   0,   FR_ETCHED,		0,	 NULL, NULL,  NULL },
  { jwin_text_proc,	   221,	42,   80,	9,   vc(0),  vc(11),	0,	   0,		   0,		0,	 (void *) "Other", NULL,  NULL },
  { jwin_check_proc,   225,	52,   64,	9,   vc(0),  vc(11),	0,	   0,		   1,		0,	 (void *) "Bait", NULL,  NULL },
  { jwin_check_proc,   225,	62,   64,	9,   vc(0),  vc(11),	0,	   0,		   1,		0,	 (void *) "Letter", NULL,  NULL },
  { jwin_check_proc,   225,	72,   64,	9,   vc(0),  vc(11),	0,	   0,		   1,		0,	 (void *) "Wand", NULL,  NULL },
  { jwin_check_proc,   225,	82,   64,	9,   vc(0),  vc(11),	0,	   0,		   1,		0,	 (void *) "Lens", NULL,  NULL },
  { jwin_check_proc,   225,	92,   64,	9,   vc(0),  vc(11),	0,	   0,		   1,		0,	 (void *) "Din's Fire", NULL,  NULL },
  { jwin_check_proc,   225,   102,   64,	9,   vc(0),  vc(11),	0,	   0,		   1,		0,	 (void *) "Farore's Wind", NULL,  NULL },
  { jwin_check_proc,   225,   112,   64,	9,   vc(0),  vc(11),	0,	   0,		   1,		0,	 (void *) "Nayru's Love", NULL,  NULL },
  { jwin_text_proc,	   225,   132,   48,	9,   vc(0),  vc(11),	0,	   0,		   0,		0,	 (void *) "Bombs:", NULL,  NULL },
  { jwin_edit_proc,	229,   142,   40,   16,	   0,	   0,	0,	   0,		   6,		0,	 NULL, NULL,  NULL },
  { jwin_text_proc,	   225,   162,   48,	9,   vc(0),  vc(11),	0,	   0,		   0,		0,	 (void *) "S-Bombs:", NULL,  NULL },
  { jwin_edit_proc,	229,   162,   40,   16,	   0,	   0,	0,	   0,		   6,		0,	 NULL, NULL,  NULL },
  { jwin_check_proc,   225,   122,   64,	9,   vc(0),  vc(11),	0,	   0,		   1,		0,	 (void *) "Cane of Byrna", NULL,  NULL },
  //45
  { d_timer_proc,		 0,	0,	 0,	0,	0,	   0,	   0,	   0,		  0,		  0,		 NULL, NULL, NULL },
  { NULL,				 0,	0,	0,	0,   0,	   0,	   0,	   0,		  0,			 0,	   NULL,						   NULL,  NULL }
};*/



bool zc_getname(const char *prompt,const char *ext,EXT_LIST *list,const char *def,bool usefilename)
{
	go();
	int32_t ret=0;
	ret = zc_getname_nogo(prompt,ext,list,def,usefilename);
	comeback();
	return ret != 0;
}


bool zc_getname_nogo(const char *prompt,const char *ext,EXT_LIST *list,const char *def,bool usefilename)
{
	if(def!=modulepath)
		strcpy(modulepath,def);
		
	if(!usefilename)
	{
		int32_t i=(int32_t)strlen(modulepath);
		
		while(i>=0 && modulepath[i]!='\\' && modulepath[i]!='/')
			modulepath[i--]=0;
	}
	
	//  int32_t ret = file_select_ex(prompt,modulepath,ext,255,-1,-1);
	int32_t ret=0;
	int32_t sel=0;
	
	if(list==NULL)
	{
		ret = jwin_file_select_ex(prompt,modulepath,ext,2048,-1,-1,lfont);
	}
	else
	{
		ret = jwin_file_browse_ex(prompt, modulepath, list, &sel, 2048, -1, -1, lfont);
	}
	
	return ret!=0;
}

//The Dialogue that loads a ZMOD Module File
int32_t zc_load_zmod_module_file()
{
	if ( Playing )
	{
	jwin_alert("Error","Cannot change module while playing a quest!",NULL,NULL,"O&K",NULL,'k',0,lfont);	
	return -1;
	}
	if(!zc_getname("Load Module (.zmod)","zmod",NULL,modulepath,false))
		return D_CLOSE;
	
	FILE *tempmodule = fopen(modulepath,"r");
			
			if(tempmodule == NULL)
			{
				jwin_alert("Error","Cannot open specified file!",NULL,NULL,"O&K",NULL,'k',0,lfont);
				return -1;
			}
		
		
		//Set the module path:
		memset(moduledata.module_name, 0, sizeof(moduledata.module_name));
		strcpy(moduledata.module_name, modulepath);
		al_trace("New Module Path is: %s \n", moduledata.module_name);
		set_config_string("ZCMODULE","current_module",moduledata.module_name);
		//save_game_configs();
		zcm.init(true); //Load the module values.
		moduledata.refresh_title_screen = 1;
//		refresh_select_screen = 1;
		build_biic_list();
		return D_O_K;
}

static DIALOG module_info_dlg[] =
{
	// (dialog proc)	 (x)   (y)   (w)   (h)   (fg)	 (bg)	(key)	(flags)	 (d1)		   (d2)	 (dp)


	{ jwin_win_proc,	  0,   0,   200,  200,  vc(14),  vc(1),  0,	   D_EXIT,		  0,			 0, (void *) "About Current Module", NULL, NULL },
	//1
	{  jwin_text_proc,		10,	20,	 20,	  8,	vc(11),	 vc(1),	  0,	0,		  0,	0, (void*)"Module:",			   NULL,   NULL  },
	//2
	{  jwin_text_proc,		50,	20,	 20,	  8,	vc(11),	 vc(1),	  0,	0,		  0,	0, (void*)"",			   NULL,   NULL  },
   {  jwin_text_proc,		10,	30,	 20,	  8,	vc(11),	 vc(1),	  0,	0,		  0,	0, (void*)"Author:",			   NULL,   NULL  },
	//4
	{  jwin_text_proc,		50,	30,	 20,	  8,	vc(11),	 vc(1),	  0,	0,		  0,	0, (void*)"",			   NULL,   NULL  },
	{  jwin_text_proc,		10,	40,	 20,	  8,	vc(11),	 vc(1),	  0,	0,		  0,	0, (void*)"",			   NULL,   NULL  },
	{  jwin_text_proc,		10,	50,	 20,	  8,	vc(11),	 vc(1),	  0,	0,		  0,	0, (void*)"Information:",			   NULL,   NULL  },
	//7
	
	{  jwin_text_proc,		10,	60,	 20,	  8,	vc(11),	 vc(1),	  0,	0,		  0,	0, (void*)"",			   NULL,   NULL  },
	{  jwin_text_proc,		10,	70,	 20,	  8,	vc(11),	 vc(1),	  0,	0,		  0,	0, (void*)"",			   NULL,   NULL  },
	{  jwin_text_proc,		10,	80,	 20,	  8,	vc(11),	 vc(1),	  0,	0,		  0,	0, (void*)"",			   NULL,   NULL  },
	{  jwin_text_proc,		10,	90,	 20,	  8,	vc(11),	 vc(1),	  0,	0,		  0,	0, (void*)"",			   NULL,   NULL  },
	{  jwin_text_proc,		10,	100,	 20,	  8,	vc(11),	 vc(1),	  0,	0,		  0,	0, (void*)"",			   NULL,   NULL  },
	{  jwin_text_proc,		10,	120,	 20,	  8,	vc(11),	 vc(1),	  0,	0,		  0,	0, (void*)"",			   NULL,   NULL  },
	{  jwin_text_proc,		10,	130,	 20,	  8,	vc(11),	 vc(1),	  0,	0,		  0,	0, (void*)"",			   NULL,   NULL  },
	{  jwin_text_proc,		10,	140,	 20,	  8,	vc(11),	 vc(1),	  0,	0,		  0,	0, (void*)"",			   NULL,   NULL  },
	{  jwin_text_proc,		10,	150,	 20,	  8,	vc(11),	 vc(1),	  0,	0,		  0,	0, (void*)"",			   NULL,   NULL  },
   
	{ jwin_button_proc,   40,   160,  50,   21,   vc(14),  vc(1),  13,	  D_EXIT,	 0,			 0, (void *) "OK", NULL, NULL },
	{ jwin_button_proc,   200-40-50,  160,  50,   21,   vc(14),  vc(1),  27,	  D_EXIT,	 0,			 0, (void *) "Cancel", NULL, NULL },
	{ NULL,				 0,	0,	0,	0,   0,	   0,	   0,	   0,		  0,			 0,	   NULL,						   NULL,  NULL }
};

void about_zcplayer_module(const char *prompt,int32_t initialval)
{	
	
	module_info_dlg[0].dp2 = lfont;
	if ( moduledata.moduletitle[0] != NULL )
		module_info_dlg[2].dp = (char*)moduledata.moduletitle;
	
	if ( moduledata.moduleauthor[0] != NULL )
		module_info_dlg[4].dp = (char*)moduledata.moduleauthor;
	
	if ( moduledata.moduleinfo0[0] != NULL )
		module_info_dlg[7].dp = (char*)moduledata.moduleinfo0;
	if ( moduledata.moduleinfo1[0] != NULL )
		module_info_dlg[8].dp = (char*)moduledata.moduleinfo1;
	if ( moduledata.moduleinfo2[0] != NULL )
		module_info_dlg[9].dp = (char*)moduledata.moduleinfo2;
	if ( moduledata.moduleinfo3[0] != NULL )
		module_info_dlg[10].dp = (char*)moduledata.moduleinfo3;
	if ( moduledata.moduleinfo4[0] != NULL )
		module_info_dlg[11].dp = (char*)moduledata.moduleinfo4;
	
	char module_date[255];
	memset(module_date, 0, sizeof(module_date));
	sprintf(module_date,"Build Date: %s %s, %d at @ %d:%d %s", dayextension(moduledata.modday).c_str(), 
			(char*)months[moduledata.modmonth], moduledata.modyear, moduledata.modhour, moduledata.modminute, moduledata.moduletimezone);
	
	
	
	char module_vers[255];
	memset(module_vers, 0, sizeof(module_vers));
	sprintf(module_vers, "Version: %d.%d.%d.%d", moduledata.modver_1, moduledata.modver_2, moduledata.modver_3, moduledata.modver_4);
	
	
	//sprintf(tilecount,"%d",1);
	
	char module_build[255];
	memset(module_build, 0, sizeof(module_build));
	if ( moduledata.modbeta )
		sprintf(module_build,"Module Build: %d, %s: %d", moduledata.modbuild, (moduledata.modbeta<0) ? "Alpha" : "Beta", moduledata.modbeta );
	else
		sprintf(module_build,"Module Build: %d", moduledata.modbuild);
	
	module_info_dlg[12].dp = (char*)module_date;
	module_info_dlg[13].dp = (char*)module_vers;
	module_info_dlg[14].dp = (char*)module_build;
	
	if(is_large)
		large_dialog(module_info_dlg);
	
	int32_t ret = zc_popup_dialog(module_info_dlg,-1);
	jwin_center_dialog(module_info_dlg);
	
	
}

int32_t onAbout_ZCP_Module()
{
	about_zcplayer_module("About Module (.zmod)", 0);
	return D_O_K;
}

//New Modules Menu for 2.55+
static MENU zcmodule_menu[] =
{
	{ (char *)"&Load Module...",		zc_load_zmod_module_file,		   NULL,					 0,			NULL   },
	{ (char *)"&About Module",		onAbout_ZCP_Module,		   NULL,					 0,			NULL   },
	
	{  NULL,								NULL,					  NULL,					 0,			NULL   }
};

int32_t onToggleRecordingNewSaves()
{
	if (zc_get_config("zeldadx", "replay_new_saves", false))
	{
		zc_set_config("zeldadx", "replay_new_saves", false);
	}
	else
	{
		zc_set_config("zeldadx", "replay_new_saves", true);
		jwin_alert("Recording", "Newly created saves will be recorded and written to a replay file.",
			NULL,NULL,"OK",NULL,13,27,lfont);
	}
	return D_O_K;
}

int32_t onStopReplayOrRecord()
{
	if (replay_is_replaying())
	{
		replay_quit();
	}
	else if (replay_get_mode() == ReplayMode::Record)
	{
		if (!replay_get_meta_bool("test_mode"))
		{
			jwin_alert("Recording", "You cannot stop recording a save file.",
				NULL,NULL,"OK",NULL,13,27,lfont);
			return D_CLOSE;
		}

		if (jwin_alert("Stop Recording",
			"Save replay to disk and stop recording?",
			"This will stop the recording.",
			NULL,
			"Yes","No",13,27,lfont) != 1)
		return D_CLOSE;

		replay_save();
		replay_stop();
	}
	return D_O_K;
}

int32_t onLoadReplay()
{
	if (Playing)
	{
		if (jwin_alert("Replay - Warning!",
			"Loading a replay will exit the current game.",
			"All unsaved progress will be lost.",
			"Do you wish to continue?",
			"Yes","No",13,27,lfont) != 1)
		return D_CLOSE;
	}

	if (jwin_alert("Replay",
		"Select a replay file to play back.",
		"You won't be able to save, and it won't effect existing saves.",
		"You can stop the replay and take over manually any time.",
		"OK","Nevermind",13,27,lfont) == 1)
	{
		char replay_path[2048];
		strcpy(replay_path, "replays/");
		if (jwin_file_select_ex(
				fmt::format("Load Replay (.{})", REPLAY_EXTENSION).c_str(),
				replay_path, REPLAY_EXTENSION.c_str(), 2048, -1, -1, lfont) == 0)
			return D_CLOSE;

		replay_quit();
		load_replay_file_deferred(ReplayMode::Replay, replay_path);
		Quit = qRESET;
		return D_CLOSE;
	}
	return D_O_K;
}

int32_t onSaveReplay()
{
	if (replay_get_mode() == ReplayMode::Record)
	{
		if (!replay_get_meta_bool("test_mode"))
		{
			if (jwin_alert("Save Replay",
				"This will save a copy of the replay up to this point.",
				"The official replay file will be untouched.",
				"Do you wish to continue?",
				"Yes","No",13,27,lfont) != 1)
			return D_CLOSE;

			char replay_path[2048];
			strcpy(replay_path, replay_get_filename().c_str());
			if (jwin_file_select_ex(
					fmt::format("Save Replay (.{})", REPLAY_EXTENSION).c_str(),
					replay_path, REPLAY_EXTENSION.c_str(), 2048, -1, -1, lfont) == 0)
				return D_CLOSE;

			if (fileexists(replay_path))
			{
				jwin_alert("Save Replay", "You cannot overwrite an existing file.",
					NULL,NULL,"OK",NULL,13,27,lfont);
				return D_CLOSE;
			}

			replay_save(replay_path);
		}
		else
		{
			replay_save();
		}
	}
	return D_O_K;
}

static MENU replay_menu[] =
{
	{ (char *)"Record new saves",		   onToggleRecordingNewSaves, NULL,					 0,			NULL   },
	{ (char *)"Stop replay",				onStopReplayOrRecord,	  NULL,					 0,			NULL   },
	{ (char *)"Load replay",				onLoadReplay,			  NULL,					 0,			NULL   },
	{ (char *)"Save replay",				onSaveReplay,			  NULL,					 0,			NULL   },
	
	{  NULL,								NULL,					  NULL,					 0,			NULL   }
};

static DIALOG credits_dlg[] =
{
	/* (dialog proc)	   (x)   (y)   (w)   (h)   (fg)	 (bg)	 (key)	(flags)	(d1)	  (d2)	 (dp)	 (dp2) (dp3) */
	{ jwin_win_proc,	   40,   38,   241,  173,  vc(14),  vc(1),   0,	   D_EXIT,	0,		0, (void *) "Zelda Classic Credits", NULL,  NULL },
	{ jwin_frame_proc,	 47,   65,   227,  115,  vc(15),  vc(1),   0,	   0,		 FR_DEEP,  0,	   NULL, NULL,  NULL },
	{ d_bitmap_proc,	   49,   67,   222,  110,  vc(15),  vc(1),   0,	   0,		 0,		0,	   NULL, NULL,  NULL },
	{ jwin_button_proc,	140,  184,  41,   21,   vc(14),  vc(1),   0,	   D_EXIT,	0,		0, (void *) "OK", NULL,  NULL },
	{ d_timer_proc,		 0,	0,	 0,	0,	0,	   0,	   0,	   0,		  0,		  0,		 NULL, NULL, NULL },
	{ NULL,				 0,	0,	0,	0,   0,	   0,	   0,	   0,		  0,			 0,	   NULL,						   NULL,  NULL }
};

static ListData dmap_list(dmaplist, &font);

static DIALOG goto_dlg[] =
{
	/* (dialog proc)	   (x)   (y)   (w)   (h)   (fg)	 (bg)	 (key)	(flags)	(d1)	  (d2)	 (dp)					 (dp2) (dp3) */
	{ jwin_win_proc,	   48,   25,   205,  100,  0,	   0,	   0,	   D_EXIT,	0,		0, (void *) "Goto Location", NULL,  NULL },
	{ jwin_button_proc,	90,   176-78,  61,   21,   vc(14),  0,	   13,	 D_EXIT,	0,		0, (void *) "OK", NULL,  NULL },
	{ jwin_button_proc,	170,  176-78,  61,   21,   vc(14),  0,	   27,	 D_EXIT,	0,		0, (void *) "Cancel", NULL,  NULL },
	{ jwin_text_proc,	  55,   129-75,  80,   8,	vc(0),   vc(11),  0,	   0,		 0,		0, (void *) "DMap:", NULL,  NULL },
	{ jwin_droplist_proc,	  88,  126-75,  160,  16,   0,	   0,	   0,	   0,		 0,		0, (void *) &dmap_list, NULL,  NULL },
	{ jwin_text_proc,	  55,   149-75,  80,   8,	vc(0),   vc(11),  0,	   0,		 0,		0, (void *) "Screen:", NULL,  NULL },
	{ jwin_edit_proc,	  132,  146-75,  91,   16,   0,	   0,	   0,	   0,		 2,		0,	   NULL, NULL,  NULL },
	{ d_timer_proc,		 0,	0,	 0,	0,	0,	   0,	   0,	   0,		  0,		  0,		 NULL, NULL, NULL },
	{ NULL,				 0,	0,	0,	0,   0,	   0,	   0,	   0,		  0,			 0,	   NULL,						   NULL,  NULL }
};

int32_t onGoTo()
{
	bool music = false;
	music = music;
	sprintf(cheat_goto_screen_str,"%X",cheat_goto_screen);
	
	goto_dlg[0].dp2=lfont;
	goto_dlg[4].d2=cheat_goto_dmap;
	goto_dlg[6].dp=cheat_goto_screen_str;
	
	clear_keybuf();
	
	if(is_large)
		large_dialog(goto_dlg);
		
	if(zc_popup_dialog(goto_dlg,4)==1)
	{
		// dmap, screen
		cheats_enqueue(Cheat::GoTo, goto_dlg[4].d2, zc_min(zc_xtoi(cheat_goto_screen_str),0x7F));
	};
	
	return D_O_K;
}

int32_t onGoToComplete()
{
	if(!Playing)
	{
		return D_O_K;
	}
	
	system_pal();
	music_pause();
	pause_all_sfx();
	show_mouse(screen);
	onGoTo();
	eat_buttons();
	
	zc_readrawkey(KEY_ESC);
		
	show_mouse(NULL);
	game_pal();
	music_resume();
	resume_all_sfx();
	return D_O_K;
}

int32_t onCredits()
{
	go();
	
	BITMAP *win = create_bitmap_ex(8,222,110);
	
	if(!win)
		return D_O_K;
		
	int32_t c=0;
	int32_t l=0;
	int32_t ol=-1;
	RLE_SPRITE *rle = (RLE_SPRITE*)(data[RLE_CREDITS].dat);
	RGB *pal = (RGB*)(data[PAL_CREDITS].dat);
	PALETTE tmppal;
	
	clear_bitmap(win);
	draw_rle_sprite(win,rle,0,0);
	credits_dlg[0].dp2=lfont;
	credits_dlg[1].fg = jwin_pal[jcDISABLED_FG];
	credits_dlg[2].dp = win;
	set_palette_range(black_palette,0,127,false);
	
	DIALOG_PLAYER *p = init_dialog(credits_dlg,3);
	
	while(update_dialog(p))
	{
		throttleFPS();
		++c;
		l = zc_max((c>>1)-30,0);
		
		if(l > rle->h)
			l = c = 0;
			
		if(l > rle->h - 112)
			l = rle->h - 112;
			
		clear_bitmap(win);
		draw_rle_sprite(win,rle,0,0-l);
		
		if(c<=64)
			fade_interpolate(black_palette,pal,tmppal,c,0,127);
			
		set_palette_range(tmppal,0,127,false);
		
		if(l!=ol)
		{
			scare_mouse();
			d_bitmap_proc(MSG_DRAW,credits_dlg+2,0);
			unscare_mouse();
			SCRFIX();
			ol=l;
		}
	}
	
	shutdown_dialog(p);
	destroy_bitmap(win);
	comeback();
	return D_O_K;
}

const char *midilist(int32_t index, int32_t *list_size)
{
	if(index<0)
	{
		*list_size=0;
		
		for(int32_t i=0; i<MAXMIDIS; i++)
			if(tunes[i].data)
				++(*list_size);
				
		return NULL;
	}
	
	int32_t i=0,m=0;
	
	while(m<=index && i<=MAXMIDIS)
	{
		if(tunes[i].data)
			++m;
			
		++i;
	}
	
	--i;
	
	if(i==MAXMIDIS && m<index)
		return "(null)";
		
	return tunes[i].title;
}

/*  ------- MIDI info stuff -------- */

char *text;
midi_info *zmi;
bool dialog_running;
bool listening;

void get_info(int32_t index);

int32_t d_midilist_proc(int32_t msg,DIALOG *d,int32_t c)
{
	int32_t d2 = d->d2;
	int32_t ret = jwin_droplist_proc(msg,d,c);
	
	if(d2!=d->d2)
	{
		get_info(d->d2);
	}
	
	return ret;
}

int32_t d_listen_proc(int32_t msg,DIALOG *d,int32_t c)
{
	/* 'd->d1' is offset from 'd' in DIALOG array to midilist proc */
	
	int32_t ret = jwin_button_proc(msg,d,c);
	
	if(ret == D_CLOSE)
	{
		// get current midi index
		int32_t index = (d+(d->d1))->d2;
		int32_t i=0, m=0;
		
		while(m<=index && i<=MAXMIDIS)
		{
			if(tunes[i].data)
				++m;
				
			++i;
		}
		
		--i;
		jukebox(i);
		listening = true;
		ret = D_O_K;
	}
	
	return ret;
}

int32_t d_savemidi_proc(int32_t msg,DIALOG *d,int32_t c)
{
	/* 'd->d1' is offset from 'd' in DIALOG array to midilist proc */
	
	int32_t ret = jwin_button_proc(msg,d,c);
	
	if(ret == D_CLOSE)
	{
		// get current midi index
		int32_t index = (d+(d->d1))->d2;
		int32_t i=0, m=0;
		
		while(m<=index && i<=MAXMIDIS)
		{
			if(tunes[i].data)
				++m;
				
			++i;
		}
		
		--i;
		
		// get file name
		
		int32_t  sel=0;
		//struct ffblk f;
		char title[40] = "Save MIDI: ";
		char fname[2048];
	memset(fname,0,2048);
		static EXT_LIST list[] =
		{
			{ (char *)"MIDI files (*.mid)", (char *)"mid" },
			{ (char *)"HTML files (*.html, *.html)", (char *)"htm html" },
			{ NULL,								  NULL }
		};
		
		strcpy(title+11, tunes[i].title);
	title[39] = '\0';
		
		if(jwin_file_browse_ex(title, fname, list, &sel, 2048, -1, -1, lfont)==0)
			goto done;
			
		if(exists(fname))
		{
			if(jwin_alert(title, fname, "already exists.", "Overwrite it?", "&Yes","&No",'y','n',lfont)==2)
				goto done;
		}
		
		// save midi i
		
		if(save_midi(fname, (MIDI*)tunes[i].data) != 0)
			jwin_alert(title, "Error saving MIDI to", fname, NULL, "Darn", NULL,13,27,lfont);
			
done:
		chop_path(fname);
		ret = D_REDRAW;
	}
	
	return ret;
}

static ListData midi_list(midilist, &font);

static DIALOG midi_dlg[] =
{
	/* (dialog proc)	   (x)   (y)   (w)   (h)   (fg)	 (bg)	 (key)	(flags)	(d1)	  (d2)	 (dp)	 (dp2) (dp3) */
	{ jwin_win_proc,	   8,	28,   304,  184,  0,	   0,		0,	   D_EXIT,	0,		0, (void *) "MIDI Info", NULL,  NULL },
	{ jwin_text_proc,		 32,   60,   40,   8,	vc(0),   vc(11),   0,	   0,		 0,		0, (void *) "Tune:", NULL,  NULL },
	{ d_midilist_proc,	 80,   56,   192,  16,   0,	   0,		0,	   0,		 0,		0, (void *) &midi_list, NULL,  NULL },
	{ jwin_textbox_proc,   15,   80,   290,  96,   0,	   0,		0,	   0,		 0,		0,	   NULL, NULL,  NULL },
	{ d_listen_proc,	   24,   183,  72,   21,   0,	   0,		'l',	 D_EXIT,	-2,	   0, (void *) "&Listen", NULL,  NULL },
	{ d_savemidi_proc,	 108,  183,  72,   21,   0,	   0,		's',	 D_EXIT,	-3,	   0, (void *) "&Save", NULL,  NULL },
	{ jwin_button_proc,	236,  183,  61,   21,   0,	   0,		'k',	 D_EXIT,	0,		0, (void *) "O&K", NULL,  NULL },
	{ d_timer_proc,		 0,	0,	 0,	0,	0,	   0,	   0,	   0,		  0,		  0,		 NULL, NULL, NULL },
	{ NULL,				 0,	0,	0,	0,   0,	   0,	   0,	   0,		  0,			 0,	   NULL,						   NULL,  NULL }
};

void get_info(int32_t index)
{
	int32_t i=0, m=0;
	
	while(m<=index && i<=MAXMIDIS)
	{
		if(tunes[i].data)
			++m;
			
		++i;
	}
	
	--i;
	
	if(i==MAXMIDIS && m<index)
		strcpy(text,"(null)");
	else
	{
		get_midi_info((MIDI*)tunes[i].data,zmi);
		get_midi_text((MIDI*)tunes[i].data,zmi,text);
	}
	
	midi_dlg[0].dp2=lfont;
	midi_dlg[3].dp = text;
	midi_dlg[3].d1 = midi_dlg[3].d2 = 0;
	midi_dlg[5].flags = (tunes[i].flags&tfDISABLESAVE) ? D_DISABLED : D_EXIT;
	
	if(dialog_running)
	{
		scare_mouse();
		jwin_textbox_proc(MSG_DRAW,midi_dlg+3,0);
		d_savemidi_proc(MSG_DRAW,midi_dlg+5,0);
		unscare_mouse();
	}
}

int32_t onMIDICredits()
{
	text = (char*)malloc(4096);
	zmi = (midi_info*)malloc(sizeof(midi_info));
	
	if(!text || !zmi)
	{
		jwin_alert(NULL,"Not enough memory",NULL,NULL,"OK",NULL,13,27,lfont);
		return D_O_K;
	}
	
	bool do_pause_midi = midi_pos >= 0 && currmidi;
	auto restore_midi = currmidi;
	if(do_pause_midi)
	{
		paused_midi_pos = midi_pos;
		stop_midi();
		midi_paused=true;
		midi_suspended = midissuspHALTED;
	}
	
	midi_dlg[0].dp2=lfont;
	midi_dlg[2].d1 = 0;
	midi_dlg[2].d2 = 0;
	midi_dlg[4].flags = D_EXIT;
	midi_dlg[5].flags = (tunes[midi_dlg[2].d1].flags&tfDISABLESAVE) ? D_DISABLED : D_EXIT;
	
	listening = false;
	dialog_running=false;
	get_info(0);
	
	dialog_running=true;
	
	if(is_large)
		large_dialog(midi_dlg);
		
	zc_popup_dialog(midi_dlg,0);
	dialog_running=false;
	
	if(listening)
		music_stop();
	
	if(do_pause_midi)
	{
		midi_suspended = midissuspRESUME;
		currmidi = restore_midi;
		midi_pos = paused_midi_pos;
	}
		
	if(text) free(text);
	if(zmi) free(zmi);
	return D_O_K;
}

int32_t onAbout()
{
	char buf1[80]={0};
	std::ostringstream oss;
	sprintf(buf1,"%s (%s), Version: %s", ZC_PLAYER_NAME,PROJECT_NAME,ZC_PLAYER_V);
	oss << buf1 << '\n';
	sprintf(buf1, "%s, Build %d", ALPHA_VER_STR, VERSION_BUILD);
	oss << buf1 << '\n';
	sprintf(buf1,"Build Date: %s %s, %d at @ %s %s", dayextension(BUILDTM_DAY).c_str(), (char*)months[BUILDTM_MONTH], BUILDTM_YEAR, __TIME__, __TIMEZONE__);
	oss << buf1 << '\n';
	sprintf(buf1, "Built By: %s", DEV_SIGNOFF);
	oss << buf1 << '\n';
	
	InfoDialog("About ZC", oss.str()).show();
	return D_O_K;
}

int32_t onQuest()
{
	char fname[100];
	strcpy(fname, get_filename(qstpath));
	quest_dlg[0].dp2=lfont;
	quest_dlg[1].dp = fname;
	
	if(QHeader.quest_number==0)
		sprintf(str_a,"Custom");
	else
		sprintf(str_a,"%d",QHeader.quest_number);
		
	sprintf(str_s,"%s",VerStr(QHeader.zelda_version));
	
	quest_dlg[11].d1 = quest_dlg[9].d1 = 0;
	quest_dlg[11].d2 = quest_dlg[9].d2 = 0;
	
	if(is_large)
		large_dialog(quest_dlg);
		
	zc_popup_dialog(quest_dlg, 0);
	return D_O_K;
}

void call_vidmode_dlg();
int32_t onVidMode()
{
	call_vidmode_dlg();
	return D_O_K;
}

#define addToHash(c,b,h) if(h->find(c ## key) == h->end()) \
{(*h)[c ## key]=true;} else { if ( c ## key != 0 ) b = false;}
//Added an extra statement, so that if the key is cleared to 0, the cleared
//keybinding status need not be unique. -Z ( 1st April, 2019 )

void load_ukeys(int32_t* arr)
{
	arr[ukey_a] = Akey;
	arr[ukey_b] = Bkey;
	arr[ukey_s] = Skey;
	arr[ukey_l] = Lkey;
	arr[ukey_r] = Rkey;
	arr[ukey_p] = Pkey;
	arr[ukey_ex1] = Exkey1;
	arr[ukey_ex2] = Exkey2;
	arr[ukey_ex3] = Exkey3;
	arr[ukey_ex4] = Exkey4;
	arr[ukey_du] = DUkey;
	arr[ukey_dd] = DDkey;
	arr[ukey_dl] = DLkey;
	arr[ukey_dr] = DRkey;
	arr[ukey_mod1a] = cheat_modifier_keys[0];
	arr[ukey_mod1b] = cheat_modifier_keys[1];
	arr[ukey_mod2a] = cheat_modifier_keys[2];
	arr[ukey_mod2b] = cheat_modifier_keys[3];
};

static const char* ukey_names[] = {
	"A", "B", "Start", "L", "R", "Map",
	"Ex1", "Ex2", "Ex3", "Ex4", "Up", "Down",
	"Left", "Right", "Cheat Mod L1", "Cheat Mod L2",
	"Cheat Mod R1", "Cheat Mod R2",
};
std::string get_ukey_name(int32_t k)
{
	if (k < num_ukey) return ukey_names[k];
	return "";
}

int32_t onKeyboard()
{
	int32_t a = Akey;
	int32_t b = Bkey;
	int32_t s = Skey;
	int32_t l = Lkey;
	int32_t r = Rkey;
	int32_t p = Pkey;
	int32_t ex1 = Exkey1;
	int32_t ex2 = Exkey2;
	int32_t ex3 = Exkey3;
	int32_t ex4 = Exkey4;
	int32_t du = DUkey;
	int32_t dd = DDkey;
	int32_t dl = DLkey;
	int32_t dr = DRkey;
	int32_t mod1a = cheat_modifier_keys[0];
	int32_t mod1b = cheat_modifier_keys[1];
	int32_t mod2a = cheat_modifier_keys[2];
	int32_t mod2b = cheat_modifier_keys[3];
	bool done=false;
	int32_t ret;
	
	keyboard_control_dlg[0].dp2=lfont;
	
	if(is_large)
		large_dialog(keyboard_control_dlg);
		
	while(!done)
	{
		ret = zc_popup_dialog(keyboard_control_dlg,3);
		
		if(ret==3) // OK
		{
			int32_t ukeys[num_ukey];
			load_ukeys(ukeys);
			std::vector<std::string> uniqueError;
			for(int32_t q = 0; q < num_ukey; ++q)
			{
				for(int32_t p = q+1; p < num_ukey; ++p)
				{
					if(ukeys[q] == ukeys[p] && ukeys[q] != 0)
					{
						char buf[64];
						sprintf(buf, "'%s' conflicts with '%s'", get_ukey_name(q).c_str(), get_ukey_name(p).c_str());
						std::string str(buf);
						uniqueError.push_back(str);
					}
				}
			}
			if(uniqueError.size() == 0)
				done = true;
			else
			{
				box_start(1, "Duplicate Keys", lfont, sfont, false, keyboard_control_dlg[0].w,keyboard_control_dlg[0].h, 2);
				box_out("Cannot have duplicate keybinds!"); box_eol();
				for(std::vector<std::string>::iterator it = uniqueError.begin();
					it != uniqueError.end(); ++it)
				{
					box_out((*it).c_str()); box_eol();
				}
				box_end(true);
			}
			/* Old uniqueness check
			std::map<int32_t,bool> *keyhash = new std::map<int32_t,bool>();
			bool unique = true;
			addToHash(A,unique,keyhash);
			addToHash(B,unique,keyhash);
			addToHash(S,unique,keyhash);
			addToHash(L,unique,keyhash);
			addToHash(R,unique,keyhash);
			addToHash(P,unique,keyhash);
			addToHash(DU,unique,keyhash);
			addToHash(DD,unique,keyhash);
			addToHash(DL,unique,keyhash);
			addToHash(DR,unique,keyhash);
			
			if(keyhash->find(Exkey1) == keyhash->end())
			{
				(*keyhash)[Exkey1]=true;
			}
			else
			{
				if ( Exkey1 != 0 ) unique = false;
			}
			
			if(keyhash->find(Exkey2) == keyhash->end())
			{
				(*keyhash)[Exkey2]=true;
			}
			else
			{
				if ( Exkey2 != 0 ) unique = false;
			}
			
			if(keyhash->find(Exkey3) == keyhash->end())
			{
				(*keyhash)[Exkey3]=true;
			}
			else
			{
				if ( Exkey3 != 0 ) unique = false;
			}
			
			if(keyhash->find(Exkey4) == keyhash->end())
			{
				(*keyhash)[Exkey4]=true;
			}
			else
			{
				if ( Exkey4 != 0 )unique = false;
			}
			//modifier keys
			if(keyhash->find(cheat_modifier_keys[0]) == keyhash->end())
			{
				(*keyhash)[cheat_modifier_keys[0]]=true;
			}
			else
			{
				if ( cheat_modifier_keys[0] != 0 ) unique = false;
			}
			if(keyhash->find(cheat_modifier_keys[1]) == keyhash->end())
			{
				(*keyhash)[cheat_modifier_keys[1]]=true;
			}
			else
			{
				if ( cheat_modifier_keys[1] != 0 ) unique = false;
			}
			if(keyhash->find(cheat_modifier_keys[2]) == keyhash->end())
			{
				(*keyhash)[cheat_modifier_keys[2]]=true;
			}
			else
			{
				if ( cheat_modifier_keys[2] != 0 ) unique = false;
			}
			if(keyhash->find(cheat_modifier_keys[3]) == keyhash->end())
			{
				(*keyhash)[cheat_modifier_keys[3]]=true;
			}
			else
			{
				if ( cheat_modifier_keys[3] != 0 ) unique = false;
			}
			
			delete keyhash;
			
			if(unique)
				done=true;
			else
				jwin_alert("Error", "Key bindings must be unique!", "", "", "OK",NULL,'o',0,lfont);
			*/
		}
		else // Cancel
		{
			Akey = a;
			Bkey = b;
			Skey = s;
			Lkey = l;
			Rkey = r;
			Pkey = p;
			Exkey1 = ex1;
			Exkey2 = ex2;
			Exkey3 = ex3;
			Exkey4 = ex4;
			DUkey = du;
			DDkey = dd;
			DLkey = dl;
			DRkey = dr;
			cheat_modifier_keys[0] = mod1a;
			cheat_modifier_keys[1] = mod1b;
			cheat_modifier_keys[2] = mod2a;
			cheat_modifier_keys[3] = mod2b;

			done=true;
		}
	}
	
	save_game_configs();
	return D_O_K;
}

int32_t onGamepad()
{
	int32_t a = Abtn;
	int32_t b = Bbtn;
	int32_t s = Sbtn;
	int32_t l = Lbtn;
	int32_t r = Rbtn;
	int32_t m = Mbtn;
	int32_t p = Pbtn;
	int32_t ex1 = Exbtn1;
	int32_t ex2 = Exbtn2;
	int32_t ex3 = Exbtn3;
	int32_t ex4 = Exbtn4;
	int32_t up = DUbtn;
	int32_t down = DDbtn;
	int32_t left = DLbtn;
	int32_t right = DRbtn;
	
	gamepad_dlg[0].dp2=lfont;
	if(analog_movement)
		gamepad_dlg[56].flags|=D_SELECTED;
	else
		gamepad_dlg[56].flags&=~D_SELECTED;
	
	if(is_large)
		large_dialog(gamepad_dlg);
		
	int32_t ret = zc_popup_dialog(gamepad_dlg,4);
	
	if(ret == 4) //OK
		analog_movement = gamepad_dlg[56].flags&D_SELECTED;
	else //Cancel
	{
		Abtn = a;
		Bbtn = b;
		Sbtn = s;
		Lbtn = l;
		Rbtn = r;
		Mbtn = m;
		Pbtn = p;
		Exbtn1 = ex1;
		Exbtn2 = ex2;
		Exbtn3 = ex3;
		Exbtn4 = ex4;
		DUbtn = up;
		DDbtn = down;
		DLbtn = left;
		DRbtn = right;
	}
	
	save_game_configs();
	return D_O_K;
}

int32_t onSound()
{
	//if out of beta, we cmight want to clear the settings from scripts:
	//#ifndef IS_BETA
	
	if ( FFCore.coreflags&FFCORE_SCRIPTED_MIDI_VOLUME )
	{
		master_volume(-1,((int32_t)FFCore.usr_midi_volume));
	}
	if ( FFCore.coreflags&FFCORE_SCRIPTED_DIGI_VOLUME )
	{
		master_volume((int32_t)(FFCore.usr_digi_volume),1);
	}
	if ( FFCore.coreflags&FFCORE_SCRIPTED_MUSIC_VOLUME )
	{
		emusic_volume = (int32_t)FFCore.usr_music_volume;
	}
	if ( FFCore.coreflags&FFCORE_SCRIPTED_SFX_VOLUME )
	{
		sfx_volume = (int32_t)FFCore.usr_sfx_volume;
	}
	if ( FFCore.coreflags&FFCORE_SCRIPTED_PANSTYLE )
	{
		pan_style = (int32_t)FFCore.usr_panstyle;
	}
	//#endif
	int32_t m = midi_volume;
	int32_t d = digi_volume;
	int32_t e = emusic_volume;
	int32_t b = zcmusic_bufsz;
	int32_t s = sfx_volume;
	int32_t p = pan_style;
	pan_style = vbound(pan_style,0,3);
	
	sound_dlg[0].dp2=lfont;
	
	if(is_large)
		large_dialog(sound_dlg);
		
	midi_dp[1] = sound_dlg[6].x;
	midi_dp[2] = sound_dlg[6].y;
	digi_dp[1] = sound_dlg[7].x;
	digi_dp[2] = sound_dlg[7].y;
	emus_dp[1] = sound_dlg[8].x;
	emus_dp[2] = sound_dlg[8].y;
	buf_dp[1]  = sound_dlg[9].x;
	buf_dp[2]  = sound_dlg[9].y;
	sfx_dp[1]  = sound_dlg[10].x;
	sfx_dp[2]  = sound_dlg[10].y;
	pan_dp[1]  = sound_dlg[11].x;
	pan_dp[2]  = sound_dlg[11].y;
	sound_dlg[15].d2 = (midi_volume==255) ? 32 : midi_volume>>3;
	sound_dlg[16].d2 = (digi_volume==255) ? 32 : digi_volume>>3;
	sound_dlg[17].d2 = (emusic_volume==255) ? 32 : emusic_volume>>3;
	sound_dlg[18].d2 = zcmusic_bufsz;
	sound_dlg[19].d2 = (sfx_volume==255) ? 32 : sfx_volume>>3;
	sound_dlg[20].d2 = pan_style;
	
	int32_t ret = zc_popup_dialog(sound_dlg,1);
	
	if(ret==2)
	{
		master_volume(digi_volume,midi_volume);
		
		for(int32_t i=0; i<WAV_COUNT; ++i)
		{
			//allegro assertion fails when passing in -1 as voice -DD
			if(sfx_voice[i] > 0)
				voice_set_volume(sfx_voice[i], sfx_volume);
		}
	}
	else
	{
		midi_volume   = m;
		digi_volume   = d;
		emusic_volume = e;
		zcmusic_bufsz = b;
		sfx_volume	= s;
		pan_style	 = p;
	}
	
	return D_O_K;
}

int32_t queding(char const* s1, char const* s2, char const* s3)
{
	return jwin_alert(ZC_str,s1,s2,s3,"&Yes","&No",'y','n',lfont);
}

int32_t onQuit()
{
	if(Playing)
	{
		int32_t ret=0;
		
		if(get_bit(quest_rules, qr_NOCONTINUE))
		{
			if(standalone_mode)
			{
				ret=queding("End current game?",
							"The continue screen is disabled; the game",
							"will be reloaded from the last save.");
			}
			else
			{
				ret=queding("End current game?",
							"The continue screen is disabled. You will",
							"be returned to the file select screen.");
			}
		}
		else
			ret=queding("End current game?",NULL,NULL);
			
		if(ret==1)
		{
			disableClickToFreeze=false;
			Quit=qQUIT;
			
			// Trying to evade a door repair charge?
			if(repaircharge)
			{
				game->change_drupy(-repaircharge);
				repaircharge=0;
			}
			
			return D_CLOSE;
		}
	}
	
	return D_O_K;
}

int32_t onTryQuitMenu()
{
	return onTryQuit(true);
}

int32_t onTryQuit(bool inMenu)
{
	if(Playing && !(GameFlags & GAMEFLAG_NO_F6))
	{
		if(get_bit(quest_rules,qr_OLD_F6))
		{
			if(inMenu) onQuit();
			else /*if(!get_bit(quest_rules, qr_NOCONTINUE))*/ f_Quit(qQUIT);
		}
		else
		{
			disableClickToFreeze=false;
			GameFlags |= GAMEFLAG_TRYQUIT;
		}
		return D_CLOSE;
	}
	
	return D_O_K;
}

int32_t onReset()
{
	if(queding("  Reset system?  ",NULL,NULL)==1)
	{
		disableClickToFreeze=false;
		Quit=qRESET;
		replay_quit();
		return D_CLOSE;
	}
	
	return D_O_K;
}

int32_t onExit()
{
	if(queding(" Quit Zelda Classic? ",NULL,NULL)==1)
	{
		Quit=qEXIT;
		return D_CLOSE;
	}
	
	return D_O_K;
}

int32_t onTitle_NES()
{
	title_version=0;
	return D_O_K;
}
int32_t onTitle_DX()
{
	title_version=1;
	return D_O_K;
}
int32_t onTitle_25()
{
	title_version=2;
	return D_O_K;
}

int32_t onDebug()
{
	if(debug_enabled)
		set_debug(!get_debug());
		
	return D_O_K;
}

int32_t onHeartBeep()
{
	heart_beep=!heart_beep;
	return D_O_K;
}

int32_t onSaveIndicator()
{
	use_save_indicator=!use_save_indicator;
	return D_O_K;
}

int32_t onEpilepsy()
{
	if(jwin_alert3(
			"Epilepsy Flash Reduction", 
			"Enabling this will reduce the intensity of flashing and screen wave effects.",
			"Disabling this will restore standard flash and wavy behaviour.",
			"Proceed?",
		 "&Yes", 
		"&No", 
		NULL, 
		'y', 
		'n', 
		NULL, 
		lfont) == 1)
	{
		if ( epilepsyFlashReduction ) epilepsyFlashReduction = 0;
		else epilepsyFlashReduction = 1;
		set_config_int("zeldadx","checked_epilepsy",1);
		save_game_configs();
	}
	return D_O_K;
}

int32_t onTriforce()
{
	for(int32_t i=0; i<MAXINITTABS; ++i)
	{
		init_tabs[i].flags&=~D_SELECTED;
	}
	
	init_tabs[3].flags=D_SELECTED;
	return onCheatConsole();
	/*triforce_dlg[0].dp2=lfont;
	for(int32_t i=1; i<=8; i++)
	  triforce_dlg[i].flags = (game->lvlitems[i] & liTRIFORCE) ? D_SELECTED : 0;
	
	if(zc_popup_dialog (triforce_dlg,-1)==9)
	{
	  for(int32_t i=1; i<=8; i++)
	  {
		game->lvlitems[i] &= ~liTRIFORCE;
		game->lvlitems[i] |= (triforce_dlg[i].flags & D_SELECTED) ? liTRIFORCE : 0;
	  }
	}
	return D_O_K;*/
}

bool rc = false;
/*
int32_t onEquipment()
{
  for (int32_t i=0; i<MAXINITTABS; ++i)
  {
	init_tabs[i].flags&=~D_SELECTED;
  }
  init_tabs[0].flags=D_SELECTED;
  return onCheatConsole();
}
*/

int32_t onItems()
{
	for(int32_t i=0; i<MAXINITTABS; ++i)
	{
		init_tabs[i].flags&=~D_SELECTED;
	}
	
	init_tabs[1].flags=D_SELECTED;
	return onCheatConsole();
}

static DIALOG getnum_dlg[] =
{
	// (dialog proc)	   (x)   (y)	(w)	 (h)   (fg)	 (bg)	(key)	(flags)	 (d1)		   (d2)	 (dp)
	{ jwin_win_proc,		80,   80,	 160,	72,   vc(0),  vc(11),  0,	   D_EXIT,	 0,			 0,	   NULL, NULL,  NULL },
	{ jwin_text_proc,		  104,  104+4,  48,	 8,	vc(0),  vc(11),  0,	   0,		  0,			 0, (void *) "Number:", NULL,  NULL },
	{ jwin_edit_proc,	   168,  104,	48,	 16,	0,	 0,	   0,	   0,		  6,			 0,	   NULL, NULL,  NULL },
	{ jwin_button_proc,	 90,   126,	61,	 21,   vc(0),  vc(11),  13,	  D_EXIT,	 0,			 0, (void *) "OK", NULL,  NULL },
	{ jwin_button_proc,	 170,  126,	61,	 21,   vc(0),  vc(11),  27,	  D_EXIT,	 0,			 0, (void *) "Cancel", NULL,  NULL },
	{ d_timer_proc,		 0,	0,	 0,	0,	0,	   0,	   0,	   0,		  0,		  0,		 NULL, NULL, NULL },
	{ NULL,				 0,	0,	0,	0,   0,	   0,	   0,	   0,		  0,			 0,	   NULL,						   NULL,  NULL }
};

int32_t getnumber(const char *prompt,int32_t initialval)
{
	char buf[20];
	sprintf(buf,"%d",initialval);
	getnum_dlg[0].dp=(void *)prompt;
	getnum_dlg[0].dp2=lfont;
	getnum_dlg[2].dp=buf;
	
	if(is_large)
		large_dialog(getnum_dlg);
		
	if(zc_popup_dialog(getnum_dlg,2)==3)
		return atoi(buf);
		
	return initialval;
}

int32_t onLife()
{
	int value = vbound(getnumber("Life",game->get_life()),1,game->get_maxlife());
	cheats_enqueue(Cheat::Life, value);
	return D_O_K;
}

int32_t onHeartC()
{
	int max_life = vbound(getnumber("Heart Containers",game->get_maxlife()/game->get_hp_per_heart()),1,4095) * game->get_hp_per_heart();
	int life = vbound(getnumber("Life",game->get_life()/game->get_hp_per_heart()),1,max_life/game->get_hp_per_heart())*game->get_hp_per_heart();
	cheats_enqueue(Cheat::MaxLife, max_life);
	cheats_enqueue(Cheat::Life, life);
	return D_O_K;
}

int32_t onMagicC()
{
	int max_magic = vbound(getnumber("Magic Containers",game->get_maxmagic()/game->get_mp_per_block()),0,2047) * game->get_mp_per_block();
	int magic = vbound(getnumber("Magic",game->get_magic()/game->get_mp_per_block()),0,game->get_maxmagic()/game->get_mp_per_block())*game->get_mp_per_block();
	cheats_enqueue(Cheat::MaxMagic, max_magic);
	cheats_enqueue(Cheat::Magic, magic);
	return D_O_K;
}

int32_t onRupies()
{
	int value = vbound(getnumber("Rupees",game->get_rupies()),0,game->get_maxcounter(1));
	cheats_enqueue(Cheat::Rupies, value);
	return D_O_K;
}

int32_t onMaxBombs()
{
	int value = vbound(getnumber("Max Bombs",game->get_maxbombs()),0,0xFFFF);
	cheats_enqueue(Cheat::MaxBombs, value);
	cheats_enqueue(Cheat::Bombs, value);
	return D_O_K;
}

int32_t onRefillLife()
{
	cheats_enqueue(Cheat::Life, game->get_maxlife());
	return D_O_K;
}
int32_t onRefillMagic()
{
	cheats_enqueue(Cheat::Magic, game->get_maxmagic());
	return D_O_K;
}
int32_t onClock()
{
	cheats_enqueue(Cheat::Clock);
	return D_O_K;
}

int32_t onQstPath()
{
	char path[2048];
	
	chop_path(qstdir);
	strcpy(path,qstdir);
	
	go();
	
	if(jwin_dfile_select_ex("Quest File Directory", path, "qst", 2048, -1, -1, lfont))
	{
		chop_path(path);
		fix_filename_case(path);
		fix_filename_slashes(path);
		strcpy(qstdir,path);
		strcpy(qstpath,qstdir);
	}
	
	comeback();
	return D_O_K;
}

#include "dialog/cheat_dialog.h"
int32_t onCheat()
{
	call_setcheat_dialog();
	game->set_cheat(maxcheat);
	if(cheat) game->did_cheat(true);
	return D_O_K;
}

int32_t onCheatRupies()
{
	cheats_enqueue(Cheat::Rupies, game->get_maxcounter(1));
	return D_O_K;
}

int32_t onCheatArrows()
{
	cheats_enqueue(Cheat::Arrows, game->get_maxarrows());
	return D_O_K;
}

int32_t onCheatBombs()
{
	cheats_enqueue(Cheat::Bombs, game->get_maxbombs(), game->get_maxcounter(6));
	return D_O_K;
}

// *** screen saver

int32_t after_time()
{
	if(ss_enable == 0)
		return INT_MAX;

	if(ss_after <= 0)
		return 5 * 60;
		
	if(ss_after <= 3)
		return ss_after * 15 * 60;
		
	if(ss_after <= 13)
		return (ss_after - 3) * 60 * 60;
		
	return MAX_IDLE + 1;
}

static const char *after_str[15] =
{
	" 5 sec", "15 sec", "30 sec", "45 sec", " 1 min", " 2 min", " 3 min",
	" 4 min", " 5 min", " 6 min", " 7 min", " 8 min", " 9 min", "10 min",
	"Never"
};

const char *after_list(int32_t index, int32_t *list_size)
{
	if(index < 0)
	{
		*list_size = 15;
		return NULL;
	}
	
	return after_str[index];
}

static ListData after__list(after_list, &font);

static DIALOG scrsaver_dlg[] =
{
	/* (dialog proc)	   (x)   (y)   (w)   (h)   (fg)	 (bg)	 (key)	(flags)	(d1)	  (d2)	 (dp)  (dp2)	(dp3) */
	{ jwin_win_proc,	   32,   64,   256,  136,  0,	   0,	   0,	   D_EXIT,	0,		0, (void *) "Screen Saver Settings", NULL,  NULL },
	{ jwin_frame_proc,	 42,   92,   236,  70,   0,	   0,	   0,	   0,		 FR_ETCHED,0,	   NULL, NULL,  NULL },
	{ jwin_text_proc,		 60,   104,  48,   8,	vc(0),   vc(11),  0,	   0,		 0,		0, (void *) "Run After", NULL,  NULL },
	{ jwin_text_proc,		 60,   128,  48,   8,	vc(0),   vc(11),  0,	   0,		 0,		0, (void *) "Speed", NULL,  NULL },
	{ jwin_text_proc,		 60,   144,  48,   8,	vc(0),   vc(11),  0,	   0,		 0,		0, (void *) "Density", NULL,  NULL },
	{ jwin_droplist_proc,  144,  100,  96,   16,   0,	   0,	   0,	   0,		 0,		0, (void *) &after__list, NULL,  NULL },
	{ jwin_slider_proc,	144,  128,  116,  8,	vc(0),   jwin_pal[jcBOX],  0,	   0,		 6,		0,	   NULL, NULL,  NULL },
	{ jwin_slider_proc,	144,  144,  116,  8,	vc(0),   jwin_pal[jcBOX],  0,	   0,		 6,		0,	   NULL, NULL,  NULL },
	{ jwin_button_proc,	42,   170,  61,   21,   0,	   0,	   0,	   D_EXIT,	0,		0, (void *) "OK", NULL,  NULL },
	{ jwin_button_proc,	124,  170,  72,   21,   0,	   0,	   0,	   D_EXIT,	0,		0, (void *) "Preview", NULL,  NULL },
	{ jwin_button_proc,	218,  170,  61,   21,   0,	   0,	   0,	   D_EXIT,	0,		0, (void *) "Cancel", NULL,  NULL },
	{ d_timer_proc,		 0,	0,	 0,	0,	0,	   0,	   0,	   0,		  0,		  0,		 NULL, NULL, NULL },
	{ NULL,				 0,	0,	0,	0,   0,	   0,	   0,	   0,		  0,			 0,	   NULL,						   NULL,  NULL }
};

int32_t onScreenSaver()
{
	scrsaver_dlg[0].dp2=lfont;
	scrsaver_dlg[5].d1 = scrsaver_dlg[5].d2 = ss_after;
	scrsaver_dlg[6].d2 = ss_speed;
	scrsaver_dlg[7].d2 = ss_density;
	
	if(is_large)
		large_dialog(scrsaver_dlg);
		
	int32_t ret = zc_popup_dialog(scrsaver_dlg,-1);
	
	if(ret == 8 || ret == 9)
	{
		ss_after   = scrsaver_dlg[5].d1;
		ss_speed   = scrsaver_dlg[6].d2;
		ss_density = scrsaver_dlg[7].d2;
	}
	
	if(ret == 9)
		// preview Screen Saver
	{
		clear_keybuf();
		scare_mouse();
		Matrix(ss_speed, ss_density, 30);
		system_pal();
		unscare_mouse();
	}
	
	return D_O_K;
}

/*****  Menus  *****/

static MENU game_menu[] =
{
	{ (char *)"&Continue\tESC",			onContinue,			   NULL,					  0, NULL },
	{ (char *)"",						  NULL,					 NULL,					  0, NULL },
	{ (char *)"L&oad Quest...",			onCustomGame,			 NULL,					  0, NULL },
	{ (char *)"&End Game\tF6",			 onTryQuitMenu,				NULL,					  0, NULL },
	{ (char *)"",						  NULL,					 NULL,					  0, NULL },
#ifndef ALLEGRO_MACOSX
	{ (char *)"&Reset\tF9",				onReset,				  NULL,					  0, NULL },
	{ (char *)"&Quit\tF10",				onExit,				   NULL,					  0, NULL },
#else
	{ (char *)"&Reset\tF7",				onReset,				  NULL,					  0, NULL },
	{ (char *)"&Quit\tF8",				onExit,				   NULL,					  0, NULL },
#endif
	{ NULL,								NULL,					 NULL,					  0, NULL }
};

static MENU title_menu[] =
{
	{ (char *)"&Original",				 onTitle_NES,			  NULL,					  0, NULL },
	{ (char *)"&Zelda Classic",			onTitle_DX,			   NULL,					  0, NULL },
	{ (char *)"Zelda Classic &2.50",	   onTitle_25,			   NULL,					  0, NULL },
	{ NULL,								NULL,					 NULL,					  0, NULL }
};

static MENU snapshot_format_menu[] =
{
	{ (char *)"&BMP",					  onSetSnapshotFormat,	  NULL,					  0, NULL },
	{ (char *)"&GIF",					  onSetSnapshotFormat,	  NULL,					  0, NULL },
	{ (char *)"&JPG",					  onSetSnapshotFormat,	  NULL,					  0, NULL },
	{ (char *)"&PNG",					  onSetSnapshotFormat,	  NULL,					  0, NULL },
	{ (char *)"PC&X",					  onSetSnapshotFormat,	  NULL,					  0, NULL },
	{ (char *)"&TGA",					  onSetSnapshotFormat,	  NULL,					  0, NULL },
	{ NULL,								NULL,					 NULL,					  0, NULL }
};

static MENU controls_menu[] =
{
	{ (char *)"Key&board...",			   onKeyboard,			  NULL,					  0, NULL },
	{ (char *)"&Gamepad...",				onGamepad,			   NULL,					  0, NULL },
	{ NULL,								 NULL,					NULL,					  0, NULL }
};

static MENU name_entry_mode_menu[] =
{
	{ (char *)"&Keyboard",				  onKeyboardEntry,		 NULL,					  0, NULL },
	{ (char *)"&Letter Grid",			   onLetterGridEntry,	   NULL,					  0, NULL },
	{ (char *)"&Extended Letter Grid",	  onExtLetterGridEntry,	NULL,					  0, NULL },
	{ NULL,								 NULL,					NULL,					  0, NULL }
};

static void set_controls_menu_active()
{
	
}

static MENU settings_menu[] =
{
	{ (char *)"C&ontrols",				  NULL,					controls_menu,			 0, NULL },
	{ (char *)"&Sound...",				  onSound,				 NULL,					  0, NULL },
	{ (char *)"&Title Screen",			  NULL,					title_menu,				0, NULL },
	{ (char *)"Name &Entry Mode",		   NULL,					name_entry_mode_menu,	  0, NULL },
	{ (char *)"",						   NULL,					NULL,					  0, NULL },
	{ (char *)"&Cap FPS\tF1",			   onVsync,				 NULL,					  0, NULL },
	{ (char *)"Show &FPS\tF2",			  onShowFPS,			   NULL,					  0, NULL },
	{ (char *)"Show Trans. &Layers",		onTransLayers,		   NULL,					  0, NULL },
	{ (char *)"Up+A+B To &Quit",			onNESquit,			   NULL,					  0, NULL },
	{ (char *)"Click to Freeze",			onClickToFreeze,		 NULL,					  0, NULL },
	{ (char *)"Volume &Keys",			   onVolKeys,			   NULL,					  0, NULL },
	{ (char *)"Cont. &Heart Beep",		  onHeartBeep,			 NULL,					  0, NULL },
	{ (char *)"Sa&ve Indicator",			onSaveIndicator,		 NULL,					  0, NULL },
	{ (char *)"Epilepsy Flash Reduction",					 onEpilepsy,				 NULL,					  0, NULL },
	{ (char *)"S&napshot Format",		   NULL,					snapshot_format_menu,	  0, NULL },
	{ (char *)"",						   NULL,					NULL,					  0, NULL },
	{ (char *)"Debu&g",					 onDebug,				 NULL,					  0, NULL },
	{ (char *)"",						   NULL,					NULL,					  0, NULL },
	{ NULL,								 NULL,					NULL,					  0, NULL }
};


static MENU misc_menu[] =
{
	{ (char *)"&About...",				  onAbout,				 NULL,					  0, NULL },
	{ (char *)"&Credits...",				onCredits,			   NULL,					  0, NULL },
	{ (char *)"&Fullscreen",				onFullscreenMenu,		NULL,					  0, NULL },
	{ (char *)"&Video Mode...",			 onVidMode,			   NULL,					  0, NULL },
	{ (char *)"",						   NULL,					NULL,					  0, NULL },
	{ (char *)"&Quest Info...",			 onQuest,				 NULL,					  0, NULL },
	{ (char *)"Quest &MIDI Info...",		onMIDICredits,		   NULL,					  0, NULL },
	{ (char *)"Quest &Directory...",		onQstPath,			   NULL,					  0, NULL },
	{ (char *)"",						   NULL,					NULL,					  0, NULL },
	{ (char *)"Take &Snapshot\tF12",		onSnapshot,			  NULL,					  0, NULL },
	{ (char *)"Sc&reen Saver...",		   onScreenSaver,		   NULL,					  0, NULL },
	{ (char *)"Save ZC Configuration",	  OnSaveZCConfig,		  NULL,					  0, NULL },
	{ (char *)"Show ZASM Debugger",		 onConsoleZASM,		   NULL,					  0, NULL },
	{ (char *)"Show ZScript Debugger",	  onConsoleZScript,		NULL,					  0, NULL },
	{ (char *)"Clear Directory Cache",	  OnnClearQuestDir,		NULL,					  0, NULL },
	{ (char *)"Modules",					NULL,					zcmodule_menu,			 0, NULL },
	{ (char *)"Replay",					 NULL,					replay_menu,			   0, NULL },

	{ NULL,								 NULL,					NULL,					  0, NULL }
};

static MENU refill_menu[] =
{
	{ (char *)"&Life\t*, H",				onRefillLife,			NULL,					  0, NULL },
	{ (char *)"&Magic\t/, M",			   onRefillMagic,		   NULL,					  0, NULL },
	{ (char *)"&Bombs\tB",				  onCheatBombs,			NULL,					  0, NULL },
	{ (char *)"&Rupees\tR",				 onCheatRupies,		   NULL,					  0, NULL },
	{ (char *)"&Arrows\tA",				 onCheatArrows,		   NULL,					  0, NULL },
	{ NULL,								 NULL,					NULL,					  0, NULL }
};

static MENU show_menu[] =
{
	{ (char *)"Combos\t0",			 onShowLayer0,				 NULL,					  0, NULL },
	{ (char *)"Layer 1\t1",			 onShowLayer1,				 NULL,					  0, NULL },
	{ (char *)"Layer 2\t2",			 onShowLayer2,				 NULL,					  0, NULL },
	{ (char *)"Layer 3\t3",			 onShowLayer3,				 NULL,					  0, NULL },
	{ (char *)"Layer 4\t4",			 onShowLayer4,				 NULL,					  0, NULL },
	{ (char *)"Layer 5\t5",			 onShowLayer5,				 NULL,					  0, NULL },
	{ (char *)"Layer 6\t6",			 onShowLayer6,				 NULL,					  0, NULL },
	{ (char *)"Overhead Combos\tO",		onShowLayerO,				 NULL,					  0, NULL },
	{ (char *)"Push Blocks\tP",			onShowLayerP,				 NULL,					  0, NULL },
	{ (char *)"Freeform Combos\t7",	  onShowLayerF,				 NULL,					  0, NULL },
	{ (char *)"Sprites\t8",			  onShowLayerS,				 NULL,					  0, NULL },
	{ (char *)"",						   NULL,					 NULL,					  0, NULL },
	{ (char *)"Walkability\tW",		  onShowLayerW,				 NULL,					  0, NULL },
	{ (char *)"Current FFC Scripts\tF",	  onShowFFScripts,			  NULL,					  0, NULL },
	{ (char *)"Hitboxes\tC",				  onShowHitboxes,			   NULL,					  0, NULL },
	{ (char *)"Effects\tE",		  onShowLayerE,				 NULL,					  0, NULL },
	{ NULL,								 NULL,					 NULL,					  0, NULL }
};

static MENU cheat_menu[] =
{
	{ (char *)"S&et Cheat",				  onCheat,				 NULL,					  0, NULL },
	{ (char *)"",							NULL,					NULL,					  0, NULL },
	{ (char *)"Re&fill",					 NULL,					refill_menu,			   0, NULL },
	{ (char *)"",							NULL,					NULL,					  0, NULL },
	{ (char *)"&Clock\tI",				   onClock,				 NULL,					  0, NULL },
	{ (char *)"Ma&x Bombs...",			   onMaxBombs,			  NULL,					  0, NULL },
	{ (char *)"&Heart Containers...",		onHeartC,				NULL,					  0, NULL },
	{ (char *)"&Magic Containers...",		onMagicC,				NULL,					  0, NULL },
	{ (char *)"",							NULL,					NULL,					  0, NULL },
	{ (char *)"&Player Data...",			 onCheatConsole,		  NULL,					  0, NULL },
	{ (char *)"",							NULL,					NULL,					  0, NULL },
	{ (char *)"Walk Through &Walls\tF11",	onNoWalls,			   NULL,					  0, NULL },
	{ (char *)"Player Ignores Side&view\tV", onIgnoreSideview,		NULL,					  0, NULL },
	{ (char *)"&Quick Movement\tQ",		  onGoFast,				NULL,					  0, NULL },
	{ (char *)"&Kill All Enemies\tK",		onKillCheat,			 NULL,					  0, NULL },
	{ (char *)"Show/Hide Layer",			 NULL,					show_menu,				 0, NULL },
	{ (char *)"Toggle Light\tL",			 onLightSwitch,		   NULL,					  0, NULL },
	{ (char *)"&Goto Location...\tG",		onGoTo,				  NULL,					  0, NULL },
	{ NULL,								  NULL,					NULL,					  0, NULL }
};

static MENU fixes_menu[] =
{
	{ (char *)"Windows MIDI Patch",		   onMIDIPatch,					NULL,	  0, NULL },
	{ NULL,								 NULL,					NULL,					  0, NULL }
};

#if DEVLEVEL > 0
int32_t devLogging();
int32_t devDebug();
#if DEVLEVEL > 1
int32_t setCheat();
#endif //DEVLEVEL > 1
static MENU dev_menu[] =
{
	{ (char *)"&Force Error Log",		   devLogging,			  NULL,			 D_SELECTED, NULL },
	{ (char *)"&Extra Debug Log",		   devDebug,				NULL,			 D_SELECTED, NULL },
	{ (char *)"&Timestamp Log",			 devTimestmp,			 NULL,			 D_SELECTED, NULL },
	#if DEVLEVEL > 1
	{ (char *)"",						   NULL,					NULL,			 0,		  NULL },
	{ (char *)"Set &Cheat",				 setCheat,				NULL,			 0,		  NULL },
	#endif //DEVLEVEL > 1
	{ NULL,								 NULL,					NULL,			 0,		  NULL }
};
int32_t devLogging()
{
	dev_logging = !dev_logging;
	dev_menu[0].flags = dev_logging ? D_SELECTED : 0;
	return D_O_K;
}
int32_t devDebug()
{
	dev_debug = !dev_debug;
	dev_menu[1].flags = dev_debug ? D_SELECTED : 0;
	return D_O_K;
}
int32_t devTimestmp()
{
	dev_timestmp = !dev_timestmp;
	dev_menu[2].flags = dev_timestmp ? D_SELECTED : 0;
	return D_O_K;
}
#if DEVLEVEL > 1
int32_t setCheat()
{
	cheat = (vbound(getnumber("Cheat Level",cheat), 0, 4));
	return D_O_K;
}
#endif //DEVLEVEL > 1
#endif //DEVLEVEL > 0

MENU the_player_menu[] =
{
	{ (char *)"&Game",					  NULL,					game_menu,				 0, NULL },
	{ (char *)"&Settings",				  NULL,					settings_menu,			 0, NULL },
	{ (char *)"&Cheat",					 NULL,					cheat_menu,				0, NULL },
	{ (char *)"&Fixes",					 NULL,					fixes_menu,				0, NULL },
	#if DEVLEVEL > 0
	{ (char *)"&ZC",					  NULL,					misc_menu,				 0, NULL },
	{ (char *)"&Dev",					   NULL,					dev_menu,				  0, NULL },
	{ NULL,								 NULL,					NULL,					  0, NULL }
	#else
	{ (char *)"&ZC",					  NULL,					misc_menu,				 0, NULL },
	{ NULL,								 NULL,					NULL,					  0, NULL }
	#endif
};

MENU the_player_menu2[] =
{
	{ (char *)"&Game",					  NULL,					game_menu,				 0, NULL },
	{ (char *)"&Settings",				  NULL,					settings_menu,			 0, NULL },
	{ (char *)"&Fixes",					 NULL,					fixes_menu,				0, NULL },
	
	#if DEVLEVEL > 0
	{ (char *)"&ZC",					  NULL,					misc_menu,				 0, NULL },
	{ (char *)"&Dev",					   NULL,					dev_menu,				  0, NULL },
	{ NULL,								 NULL,					NULL,					  0, NULL }
	#else
	{ (char *)"&ZC",					  NULL,					misc_menu,				 0, NULL },
	{ NULL,								 NULL,					NULL,					  0, NULL }
	#endif
	
};

int32_t onMIDIPatch()
{
	if(jwin_alert3(
			"Toggle Windows MIDI Fix", 
			"This action will change whether ZC Player auto-restarts a MIDI at its",
			"last index if you move ZC Player out of focus, then back into focus.",
			"Proceed?",
		 "&Yes", 
		"&No", 
		NULL, 
		'y', 
		'n', 
		NULL, 
		lfont) == 1)
	{
		if (midi_patch_fix) midi_patch_fix = 0;
		
		else midi_patch_fix = 1;	
		
	}
	fixes_menu[0].flags =(midi_patch_fix)?D_SELECTED:0;
	save_game_configs();
	return D_O_K;
}

int32_t onKeyboardEntry()
{
	NameEntryMode=0;
	return D_O_K;
}

int32_t onLetterGridEntry()
{
	NameEntryMode=1;
	return D_O_K;
}

int32_t onExtLetterGridEntry()
{
	NameEntryMode=2;
	return D_O_K;
}

int32_t onFullscreenMenu()
{
	onFullscreen();
	misc_menu[2].flags =(isFullScreen()==1)?D_SELECTED:0;
	return D_O_K;
}

void fix_menu()
{
	if(!debug_enabled)
		settings_menu[15].text = NULL;
}

static DIALOG system_dlg[] =
{
	/* (dialog proc)	 (x)   (y)   (w)   (h)   (fg)  (bg)  (key)	(flags)  (d1)	  (d2)	 (dp) */
	{ jwin_menu_proc,	0,	0,	0,	0,	0,	0,	0,	   D_USER,  0,		0, (void *) the_player_menu, NULL,  NULL },
	{ d_keyboard_proc,   0,	0,	0,	0,	0,	0,	0,	   0,	   KEY_F1,   0, (void *) onVsync, NULL,  NULL },
	{ d_keyboard_proc,   0,	0,	0,	0,	0,	0,	0,	   0,	   KEY_F2,   0, (void *) onShowFPS, NULL,  NULL },
	{ d_keyboard_proc,   0,	0,	0,	0,	0,	0,	0,	   0,	   KEY_F6,   0, (void *) onTryQuitMenu, NULL,  NULL },
#ifndef ALLEGRO_MACOSX
	{ d_keyboard_proc,   0,	0,	0,	0,	0,	0,	0,	   0,	   KEY_F9,   0, (void *) onReset, NULL,  NULL },
	{ d_keyboard_proc,   0,	0,	0,	0,	0,	0,	0,	   0,	   KEY_F10,  0, (void *) onExit, NULL,  NULL },
#else
	{ d_keyboard_proc,   0,	0,	0,	0,	0,	0,	0,	   0,	   KEY_F7,   0, (void *) onReset, NULL,  NULL },
	{ d_keyboard_proc,   0,	0,	0,	0,	0,	0,	0,	   0,	   KEY_F8,   0, (void *) onExit, NULL,  NULL },
#endif
	{ d_keyboard_proc,   0,	0,	0,	0,	0,	0,	0,	   0,	   KEY_F12,  0, (void *) onSnapshot, NULL,  NULL },
	{ d_keyboard_proc,   0,	0,	0,	0,	0,	0,	0,	   0,	   KEY_TAB,  0, (void *) onDebug, NULL,  NULL },
	{ d_timer_proc,	  0,	0,	0,	0,	0,	0,	0,	   0,	   0,		0,	   NULL,			 NULL, NULL },
	{ NULL,			  0,	0,	0,	0,	0,	0,	0,	   0,	   0,		0,	   NULL,						   NULL,  NULL }
};

static DIALOG system_dlg2[] =
{
	/* (dialog proc)	 (x)   (y)   (w)   (h)   (fg)  (bg)  (key)	(flags)  (d1)	  (d2)	 (dp) */
	{ jwin_menu_proc,	0,	0,	0,	0,	0,	0,	0,	   D_USER,  0,		0, (void *) the_player_menu2, NULL,  NULL },
	{ d_keyboard_proc,   0,	0,	0,	0,	0,	0,	0,	   0,	   KEY_F1,   0, (void *) onVsync, NULL,  NULL },
	{ d_keyboard_proc,   0,	0,	0,	0,	0,	0,	0,	   0,	   KEY_F2,   0, (void *) onShowFPS, NULL,  NULL },
	{ d_keyboard_proc,   0,	0,	0,	0,	0,	0,	0,	   0,	   KEY_F6,   0, (void *) onTryQuitMenu, NULL,  NULL },
#ifndef ALLEGRO_MACOSX
	{ d_keyboard_proc,   0,	0,	0,	0,	0,	0,	0,	   0,	   KEY_F9,   0, (void *) onReset, NULL,  NULL },
	{ d_keyboard_proc,   0,	0,	0,	0,	0,	0,	0,	   0,	   KEY_F10,  0, (void *) onExit, NULL,  NULL },
#else
	{ d_keyboard_proc,   0,	0,	0,	0,	0,	0,	0,	   0,	   KEY_F7,   0, (void *) onReset, NULL,  NULL },
	{ d_keyboard_proc,   0,	0,	0,	0,	0,	0,	0,	   0,	   KEY_F8,   0, (void *) onExit, NULL,  NULL },
#endif
	{ d_keyboard_proc,   0,	0,	0,	0,	0,	0,	0,	   0,	   KEY_F12,  0, (void *) onSnapshot, NULL,  NULL },
	{ d_keyboard_proc,   0,	0,	0,	0,	0,	0,	0,	   0,	   KEY_TAB,  0, (void *) onDebug, NULL,  NULL },
	{ d_timer_proc,	  0,	0,	0,	0,	0,	0,	0,	   0,	   0,		0,	   NULL,			 NULL, NULL },
	{ NULL,				 0,	0,	0,	0,   0,	   0,	   0,	   0,		  0,			 0,	   NULL,						   NULL,  NULL }
};

void reset_snapshot_format_menu()
{
	for(int32_t i=0; i<ssfmtMAX; ++i)
	{
		snapshot_format_menu[i].flags=0;
	}
}

int32_t onSetSnapshotFormat()
{
	switch(active_menu->text[1])
	{
	case 'B': //"&BMP"
		SnapshotFormat=0;
		break;
		
	case 'G': //"&GIF"
		SnapshotFormat=1;
		break;
		
	case 'J': //"&JPG"
		SnapshotFormat=2;
		break;
		
	case 'P': //"&PNG"
		SnapshotFormat=3;
		break;
		
	case 'C': //"PC&X"
		SnapshotFormat=4;
		break;
		
	case 'T': //"&TGA"
		SnapshotFormat=5;
		break;
		
	case 'L': //"&LBM"
		SnapshotFormat=6;
		break;
	}
	
	snapshot_format_menu[SnapshotFormat].flags=D_SELECTED;
	return D_O_K;
}


void color_layer(RGB *src,RGB *dest,char r,char g,char b,char pos,int32_t from,int32_t to)
{
	PALETTE tmp;
	
	for(int32_t i=0; i<256; i++)
	{
		tmp[i].r=r;
		tmp[i].g=g;
		tmp[i].b=b;
	}
	
	fade_interpolate(src,tmp,dest,pos,from,to);
}

void system_pal()
{
	is_sys_pal = true;
	static PALETTE pal;
	copy_pal((RGB*)data[PAL_GUI].dat, pal);
	
	// set up the grayscale palette
	for(int32_t i=128; i<192; i++)
	{
		pal[i].r = i-128;
		pal[i].g = i-128;
		pal[i].b = i-128;
	}
	load_colorset(gui_colorset, pal);
	
	color_layer(pal, pal, 24,16,16, 28, 128,191);
	
	for(int32_t i=0; i<256; i+=2)
	{
		int32_t v = (i>>3)+2;
		int32_t c = (i>>3)+192;
		pal[c] = _RGB(v,v,v+(v>>1));
		/*
		  if(i<240)
		  {
		  _allegro_hline(tmp_scr,0,i,319,c);
		  _allegro_hline(tmp_scr,0,i+1,319,c);
		  }
		  */
	}
	
	// draw the vertical screen gradient
	for(int32_t i=0; i<240; ++i)
	{
		_allegro_hline(tmp_scr,0,i,319,192+(i*31/239));
	}
	
	/*
	  palrstart= 10*63/255; palrend=166*63/255;
	  palgstart= 36*63/255; palgend=202*63/255;
	  palbstart=106*63/255; palbend=240*63/255;
	  paldivs=32;
	  for(int32_t i=0; i<paldivs; i++)
	  {
	  pal[223-paldivs+1+i].r = palrstart+((palrend-palrstart)*i/(paldivs-1));
	  pal[223-paldivs+1+i].g = palgstart+((palgend-palgstart)*i/(paldivs-1));
	  pal[223-paldivs+1+i].b = palbstart+((palbend-palbstart)*i/(paldivs-1));
	  }
	  */
	BITMAP *panorama = create_bitmap_ex(8,256,224);
	int32_t ts_height, ts_start;
	
	if(tmpscr->flags3&fNOSUBSCR && !(tmpscr->flags3&fNOSUBSCROFFSET))
	{
		clear_to_color(panorama,0);
		blit(framebuf,panorama,0,playing_field_offset,0,28,256,224-passive_subscreen_height);
		ts_height=224-passive_subscreen_height;
		ts_start=28;
	}
	else
	{
		blit(framebuf,panorama,0,0,0,0,256,224);
		ts_height=224;
		ts_start=0;
	}
	
	// gray scale the current frame
	for(int32_t y=0; y<ts_height; y++)
	{
		for(int32_t x=0; x<256; x++)
		{
			int32_t c = panorama->line[y+ts_start][x];
			int32_t gray = zc_min((RAMpal[c].r*42 + RAMpal[c].g*75 + RAMpal[c].b*14) >> 7, 63);
			tmp_scr->line[y+8+ts_start][x+32] = gray+128;
		}
	}
	
	destroy_bitmap(panorama);
	
	// save the fps_undo section
	blit(tmp_scr,fps_undo,40,216,0,0,64,16);
	
	// display everything
	vsync();
	hw_palette = &pal;
	update_hw_pal = true;
	
	if(sbig)
		stretch_blit(tmp_scr,screen,0,0,320,240,scrx-(160*(screen_scale-1)),scry-(120*(screen_scale-1)),screen_scale*320,screen_scale*240);
	else
		blit(tmp_scr,screen,0,0,scrx,scry,320,240);
		
	if(ShowFPS)
		show_fps(screen);
		
	if(Paused)
		show_paused(screen);
		
	//  sys_pal = pal;
	memcpy(sys_pal,pal,sizeof(pal));
}

void system_pal2()
{
	is_sys_pal = true;
	static PALETTE RAMpal2;
	copy_pal((RGB*)data[PAL_GUI].dat, RAMpal2);
	
	/* Windows 2000 colors
	  RAMpal2[dvc(1)] = _RGB(  0*63/255,   0*63/255,   0*63/255);
	  RAMpal2[dvc(2)] = _RGB( 66*63/255,  65*63/255,  66*63/255);
	  RAMpal2[dvc(3)] = _RGB(132*63/255, 130*63/255, 132*63/255);
	  RAMpal2[dvc(4)] = _RGB(212*63/255, 208*63/255, 200*63/255);
	  RAMpal2[dvc(5)] = _RGB(255*63/255, 255*63/255, 255*63/255);
	  RAMpal2[dvc(6)] = _RGB(255*63/255, 255*63/255, 225*63/255);
	  RAMpal2[dvc(7)] = _RGB(255*63/255, 225*63/255, 160*63/255);
	  RAMpal2[dvc(8)] = _RGB(  0*63/255,   0*63/255,  80*63/255);
	
	  byte palrstart= 10*63/255, palrend=166*63/255,
	  palgstart= 36*63/255, palgend=202*63/255,
	  palbstart=106*63/255, palbend=240*63/255,
	  paldivs=7;
	  for(int32_t i=0; i<paldivs; i++)
	  {
	  RAMpal2[dvc(15-paldivs+1)+i].r = palrstart+((palrend-palrstart)*i/(paldivs-1));
	  RAMpal2[dvc(15-paldivs+1)+i].g = palgstart+((palgend-palgstart)*i/(paldivs-1));
	  RAMpal2[dvc(15-paldivs+1)+i].b = palbstart+((palbend-palbstart)*i/(paldivs-1));
	  }
	  */
	
	/* Windows 98 colors
	  RAMpal2[dvc(1)] = _RGB(  0*63/255,   0*63/255,   0*63/255);
	  RAMpal2[dvc(2)] = _RGB(128*63/255, 128*63/255, 128*63/255);
	  RAMpal2[dvc(3)] = _RGB(192*63/255, 192*63/255, 192*63/255);
	  RAMpal2[dvc(4)] = _RGB(223*63/255, 223*63/255, 223*63/255);
	  RAMpal2[dvc(5)] = _RGB(255*63/255, 255*63/255, 255*63/255);
	  RAMpal2[dvc(6)] = _RGB(255*63/255, 255*63/255, 225*63/255);
	  RAMpal2[dvc(7)] = _RGB(255*63/255, 225*63/255, 160*63/255);
	  RAMpal2[dvc(8)] = _RGB(  0*63/255,   0*63/255,  80*63/255);
	
	  byte palrstart=  0*63/255, palrend=166*63/255,
	  palgstart=  0*63/255, palgend=202*63/255,
	  palbstart=128*63/255, palbend=240*63/255,
	  paldivs=7;
	  for(int32_t i=0; i<paldivs; i++)
	  {
	  RAMpal2[dvc(15-paldivs+1)+i].r = palrstart+((palrend-palrstart)*i/(paldivs-1));
	  RAMpal2[dvc(15-paldivs+1)+i].g = palgstart+((palgend-palgstart)*i/(paldivs-1));
	  RAMpal2[dvc(15-paldivs+1)+i].b = palbstart+((palbend-palbstart)*i/(paldivs-1));
	  }
	  */
	
	/* Windows 99 colors
	  RAMpal2[dvc(1)] = _RGB(  0*63/255,   0*63/255,   0*63/255);
	  RAMpal2[dvc(2)] = _RGB( 64*63/255,  64*63/255,  64*63/255);
	  RAMpal2[dvc(3)] = _RGB(128*63/255, 128*63/255, 128*63/255);
	  RAMpal2[dvc(4)] = _RGB(192*63/255, 192*63/255, 192*63/255);
	  RAMpal2[dvc(5)] = _RGB(223*63/255, 223*63/255, 223*63/255);
	  RAMpal2[dvc(6)] = _RGB(255*63/255, 255*63/255, 255*63/255);
	  RAMpal2[dvc(7)] = _RGB(255*63/255, 255*63/255, 225*63/255);
	  RAMpal2[dvc(8)] = _RGB(255*63/255, 225*63/255, 160*63/255);
	  RAMpal2[dvc(9)] = _RGB(  0*63/255,   0*63/255,  80*63/255);
	
	  byte palrstart=  0*63/255, palrend=166*63/255,
	  palgstart=  0*63/255, palgend=202*63/255,
	
	  palbstart=128*63/255, palbend=240*63/255,
	  paldivs=6;
	  for(int32_t i=0; i<paldivs; i++)
	  {
	  RAMpal2[dvc(15-paldivs+1)+i].r = palrstart+((palrend-palrstart)*i/(paldivs-1));
	  RAMpal2[dvc(15-paldivs+1)+i].g = palgstart+((palgend-palgstart)*i/(paldivs-1));
	  RAMpal2[dvc(15-paldivs+1)+i].b = palbstart+((palbend-palbstart)*i/(paldivs-1));
	  }
	  */
	
	
	
	RAMpal2[dvc(1)] = _RGB(0*63/255,   0*63/255,   0*63/255);
	RAMpal2[dvc(2)] = _RGB(64*63/255,  64*63/255,  64*63/255);
	RAMpal2[dvc(3)] = _RGB(128*63/255, 128*63/255, 128*63/255);
	RAMpal2[dvc(4)] = _RGB(192*63/255, 192*63/255, 192*63/255);
	RAMpal2[dvc(5)] = _RGB(223*63/255, 223*63/255, 223*63/255);
	RAMpal2[dvc(6)] = _RGB(255*63/255, 255*63/255, 255*63/255);
	RAMpal2[dvc(7)] = _RGB(255*63/255, 255*63/255, 225*63/255);
	RAMpal2[dvc(8)] = _RGB(255*63/255, 225*63/255, 160*63/255);
	RAMpal2[dvc(9)] = _RGB(0*63/255,   0*63/255,  80*63/255);
	
	byte palrstart=  0*63/255, palrend=166*63/255,
		 palgstart=  0*63/255, palgend=202*63/255,
		 palbstart=128*63/255, palbend=240*63/255,
		 paldivs=6;
		 
	for(int32_t i=0; i<paldivs; i++)
	{
		RAMpal2[dvc(15-paldivs+1)+i].r = palrstart+((palrend-palrstart)*i/(paldivs-1));
		RAMpal2[dvc(15-paldivs+1)+i].g = palgstart+((palgend-palgstart)*i/(paldivs-1));
		RAMpal2[dvc(15-paldivs+1)+i].b = palbstart+((palbend-palbstart)*i/(paldivs-1));
	}
	
	gui_bg_color=jwin_pal[jcBOX];
	gui_fg_color=jwin_pal[jcBOXFG];
	
	jwin_set_colors(jwin_pal);
	
	
	// set up the new palette
	for(int32_t i=128; i<192; i++)
	{
		RAMpal2[i].r = i-128;
		RAMpal2[i].g = i-128;
		RAMpal2[i].b = i-128;
	}
	
	/*
	  for(int32_t i=0; i<64; i++)
	  {
	  RAMpal2[128+i] = _RGB(i,i,i)1));
	  }
	  */
	
	/*
	
	  pal[vc(1)]  = _RGB(0x00,0x00,0x14);
	  pal[vc(4)]  = _RGB(0x36,0x36,0x36);
	  pal[vc(6)]  = _RGB(0x10,0x10,0x10);
	  pal[vc(7)]  = _RGB(0x20,0x20,0x20);
	  pal[vc(9)]  = _RGB(0x20,0x20,0x24);
	  pal[vc(11)] = _RGB(0x30,0x30,0x30);
	  pal[vc(14)] = _RGB(0x3F,0x38,0x28);
	
	  gui_fg_color=vc(14);
	  gui_bg_color=vc(1);
	
	  jwin_set_colors(jwin_pal);
	  */
	
	//  color_layer(RAMpal2, RAMpal2, 24,16,16, 28, 128,191);
	
	// set up the colors for the vertical screen gradient
	for(int32_t i=0; i<256; i+=2)
	{
		int32_t v = (i>>3)+2;
		int32_t c = (i>>3)+192;
		RAMpal2[c] = _RGB(v,v,v+(v>>1));
		
		/*
		  if(i<240)
		  {
		  _allegro_hline(tmp_scr,0,i,319,c);
		  _allegro_hline(tmp_scr,0,i+1,319,c);
		  }
		  */
	}
	
	// hw_palette = &RAMpal;
	// update_hw_pal = true;
	
	for(int32_t i=0; i<240; ++i)
	{
		_allegro_hline(tmp_scr,0,i,319,192+(i*31/239));
	}
	
	/*
	  byte palrstart= 10*63/255, palrend=166*63/255,
	  palgstart= 36*63/255, palgend=202*63/255,
	  palbstart=106*63/255, palbend=240*63/255,
	  paldivs=32;
	  for(int32_t i=0; i<paldivs; i++)
	  {
	  pal[223-paldivs+1+i].r = palrstart+((palrend-palrstart)*i/(paldivs-1));
	  pal[223-paldivs+1+i].g = palgstart+((palgend-palgstart)*i/(paldivs-1));
	  pal[223-paldivs+1+i].b = palbstart+((palbend-palbstart)*i/(paldivs-1));
	  }
	  */
	BITMAP *panorama = create_bitmap_ex(8,256,224);
	int32_t ts_height, ts_start;
	
	if(tmpscr->flags3&fNOSUBSCR && !(tmpscr->flags3&fNOSUBSCROFFSET))
	{
		clear_to_color(panorama,0);
		blit(framebuf,panorama,0,playing_field_offset,0,28,256,224-passive_subscreen_height);
		ts_height=224-passive_subscreen_height;
		ts_start=28;
	}
	else
	{
		blit(framebuf,panorama,0,0,0,0,256,224);
		ts_height=224;
		ts_start=0;
	}
	
	// gray scale the current frame
	for(int32_t y=0; y<ts_height; y++)
	{
		for(int32_t x=0; x<256; x++)
		{
			int32_t c = panorama->line[y+ts_start][x];
			int32_t gray = zc_min((RAMpal2[c].r*42 + RAMpal2[c].g*75 + RAMpal2[c].b*14) >> 7, 63);
			tmp_scr->line[y+8+ts_start][x+32] = gray+128;
		}
	}
	
	destroy_bitmap(panorama);
	
	// save the fps_undo section
	blit(tmp_scr,fps_undo,40,216,0,0,64,16);
	
	// display everything
	vsync();
	hw_palette = &RAMpal2;
	update_hw_pal = true;
	
	if(sbig)
		//stretch_blit(tmp_scr,screen,0,0,320,240,scrx-160,scry-120,640,480);
		stretch_blit(tmp_scr,screen,0,0,320,240,scrx-(160*(screen_scale-1)),scry-(120*(screen_scale-1)),screen_scale*320,screen_scale*240);
	else
		blit(tmp_scr,screen,0,0,scrx,scry,320,240);
		
	if(ShowFPS)
		show_fps(screen);
		
	if(Paused)
		show_paused(screen);
		
	//  sys_pal = pal;
	memcpy(sys_pal,RAMpal2,sizeof(RAMpal2));
}

static uint32_t entered_sys_pal = 0;
void enter_sys_pal()
{
	if(is_sys_pal)
	{
		if(entered_sys_pal)
			++entered_sys_pal;
		return;
	}
	system_pal();
	++entered_sys_pal;
}
void exit_sys_pal()
{
	if(entered_sys_pal)
	{
		if(!--entered_sys_pal)
		{
			game_pal();
		}
	}
}

void switch_out_callback()
{
	if (pause_in_background)
	{
		callback_switchin = 3;
		return;
	}

#ifdef _WIN32
	if(midi_patch_fix==0 || currmidi==0)
		return;

	
	paused_midi_pos = midi_pos;
	stop_midi();
	midi_paused=true;
	midi_suspended = midissuspHALTED;
#endif
}

void switch_in_callback()
{
	if(pause_in_background)
	{
		return;
	}

#ifdef _WIN32
	if(midi_patch_fix==0 || currmidi==0)
		return;
	
	else
	{
		callback_switchin = 1;
		midi_suspended = midissuspRESUME;
	}
#endif
}

void game_pal()
{
	is_sys_pal = false;
	entered_sys_pal = 0;
	clear_to_color(screen,BLACK);
	hw_palette = &RAMpal;
	update_hw_pal = true;
}

static char bar_str[] = "";

void music_pause()
{
	//al_pause_duh(tmplayer);
	zcmusic_pause(zcmusic, ZCM_PAUSE);
	midi_pause();
	midi_paused=true;
}

void music_resume()
{
	//al_resume_duh(tmplayer);
	zcmusic_pause(zcmusic, ZCM_RESUME);
	midi_resume();
	midi_paused=false;
}

void music_stop()
{
	//al_stop_duh(tmplayer);
	//unload_duh(tmusic);
	//tmusic=NULL;
	//tmplayer=NULL;
	zcmusic_stop(zcmusic);
	zcmusic_unload_file(zcmusic);
	stop_midi();
	midi_paused=false;
	currmidi=0;
}

void System()
{
	mouse_down=gui_mouse_b();
	music_pause();
	pause_all_sfx();
	MenuOpen = true;
	system_pal();
	//  FONT *oldfont=font;
	//  font=tfont;
	
	misc_menu[2].flags =(isFullScreen()==1)?D_SELECTED:0;
	
	game_menu[2].flags = getsaveslot() > -1 ? 0 : D_DISABLED;
	#if DEVLEVEL > 1
	dev_menu[4].flags = Playing ? 0 : D_DISABLED;
	#endif
	game_menu[3].flags =
		misc_menu[5].flags = Playing ? 0 : D_DISABLED;
	misc_menu[7].flags = !Playing ? 0 : D_DISABLED;
	fixes_menu[0].flags = (midi_patch_fix)?D_SELECTED:0;
	clear_keybuf();
	show_mouse(screen);
	
	DIALOG_PLAYER *p;
	
	if(!Playing || (!zcheats.flags && !get_debug() && DEVLEVEL < 2 && !zqtesting_mode))
	{
		p = init_dialog(system_dlg2,-1);
	}
	else
	{
		p = init_dialog(system_dlg,-1);
	}
	
	// drop the menu on startup if menu button pressed
	if(joybtn(Mbtn)||zc_getrawkey(KEY_ESC))
		simulate_keypress(KEY_G << 8);
		
	do
	{
		rest(17);
		
		if(mouse_down && !gui_mouse_b())
			mouse_down=0;
			
		title_menu[0].flags = (title_version==0) ? D_SELECTED : 0;
		title_menu[1].flags = (title_version==1) ? D_SELECTED : 0;
		title_menu[2].flags = (title_version==2) ? D_SELECTED : 0;
		
		settings_menu[0].flags = replay_is_replaying() ? D_DISABLED : 0;
		settings_menu[5].flags = Throttlefps?D_SELECTED:0;
		settings_menu[6].flags = ShowFPS?D_SELECTED:0;
		settings_menu[7].flags = TransLayers?D_SELECTED:0;
		settings_menu[8].flags = NESquit?D_SELECTED:0;
		settings_menu[9].flags = ClickToFreeze?D_SELECTED:0;
		settings_menu[10].flags = volkeys?D_SELECTED:0;
		//Epilepsy Prevention
		settings_menu[13].flags = (epilepsyFlashReduction) ? D_SELECTED : 0;
		
		name_entry_mode_menu[0].flags = (NameEntryMode==0)?D_SELECTED:0;
		name_entry_mode_menu[1].flags = (NameEntryMode==1)?D_SELECTED:0;
		name_entry_mode_menu[2].flags = (NameEntryMode==2)?D_SELECTED:0;
	
		misc_menu[12].flags =(zasm_debugger)?D_SELECTED:0;
		misc_menu[13].flags =(zscript_debugger)?D_SELECTED:0;
		
		cheat_menu[0].flags = 0;
		refill_menu[4].flags = get_bit(quest_rules, qr_TRUEARROWS) ? 0 : D_DISABLED;
		cheat_menu[1].text  = (cheat >= 1) || get_debug() ? bar_str : NULL;
		cheat_menu[3].text  = (cheat >= 2) || get_debug() ? bar_str : NULL;
		cheat_menu[8].text  = (cheat >= 3) || get_debug() ? bar_str : NULL;
		// TODO add onCheatConsole to new cheats_enqueue system for replay support.
		cheat_menu[9].flags = replay_is_active() ? D_DISABLED : 0;
		cheat_menu[10].text = (cheat >= 4) || get_debug() ? bar_str : NULL;
		cheat_menu[4].flags = getClock() ? D_SELECTED : 0;
		cheat_menu[11].flags = toogam ? D_SELECTED : 0;
		cheat_menu[12].flags = ignoreSideview ? D_SELECTED : 0;
		cheat_menu[13].flags = gofast ? D_SELECTED : 0;
		
		show_menu[0].flags = show_layer_0 ? D_SELECTED : 0;
		show_menu[1].flags = show_layer_1 ? D_SELECTED : 0;
		show_menu[2].flags = show_layer_2 ? D_SELECTED : 0;
		show_menu[3].flags = show_layer_3 ? D_SELECTED : 0;
		show_menu[4].flags = show_layer_4 ? D_SELECTED : 0;
		show_menu[5].flags = show_layer_5 ? D_SELECTED : 0;
		show_menu[6].flags = show_layer_6 ? D_SELECTED : 0;
		show_menu[7].flags = show_layer_over ? D_SELECTED : 0;
		show_menu[8].flags = show_layer_push ? D_SELECTED : 0;
		show_menu[9].flags = show_sprites ? D_SELECTED : 0;
		show_menu[10].flags = show_ffcs ? D_SELECTED : 0;
		show_menu[12].flags = show_walkflags ? D_SELECTED : 0;
		show_menu[13].flags = show_ff_scripts ? D_SELECTED : 0;
		show_menu[14].flags = show_hitboxes ? D_SELECTED : 0;
		show_menu[15].flags = show_effectflags ? D_SELECTED : 0;
		
		settings_menu[11].flags = heart_beep ? D_SELECTED : 0;
		settings_menu[12].flags = use_save_indicator ? D_SELECTED : 0;

		replay_menu[0].text = zc_get_config("zeldadx", "replay_new_saves", false) ?
			(char *)"Disable recording new saves" :
			(char *)"Enable recording new saves";
		replay_menu[1].flags = replay_is_active() ? 0 : D_DISABLED;
		replay_menu[1].text = replay_get_mode() == ReplayMode::Record ?
			(char *)"Stop recording" :
			(char *)"Stop replaying";
		replay_menu[3].flags = replay_get_mode() == ReplayMode::Record ? 0 : D_DISABLED;
	
		reset_snapshot_format_menu();
		snapshot_format_menu[SnapshotFormat].flags = D_SELECTED;
		
		if(debug_enabled)
		{
			settings_menu[16].flags = get_debug() ? D_SELECTED : 0;
		}
		
		if(gui_mouse_b() && !mouse_down)
			break;
			
		// press menu to drop the menu
		if(rMbtn())
			simulate_keypress(KEY_G << 8);
			
		{
			if(input_idle(true) > after_time())
				// run Screeen Saver
			{
				// Screen saver enabled for now.
				clear_keybuf();
				scare_mouse();
				Matrix(ss_speed, ss_density, 0);
				system_pal();
				unscare_mouse();
				broadcast_dialog_message(MSG_DRAW, 0);
			}
		}
		update_hw_screen();
	}
	while(update_dialog(p));
	
	//  font=oldfont;
	mouse_down=gui_mouse_b();
	shutdown_dialog(p);
	show_mouse(NULL);
	MenuOpen = false;
	if(Quit)
	{
		kill_sfx();
		music_stop();
		clear_to_color(screen,BLACK);
		update_hw_screen();
	}
	else
	{
		game_pal();
		music_resume();
		resume_all_sfx();
		
		if(rc)
			ringcolor(false);
	}
	
	eat_buttons();
	
	rc=false;
	clear_keybuf();
	//  text_mode(0);
}

void fix_dialog(DIALOG *d)
{
	for(; d->proc != NULL; d++)
	{
		d->x += scrx;
		d->y += scry;
	}
}

void fix_dialogs()
{
	/*
	  int32_t x = scrx-(sbig?160:0);
	  int32_t y = scry-(sbig?120:0);
	  if(x>0) x+=3;
	  if(y>0) y+=3;
	  if(x<0) x=0;
	  if(y<0) y=0;
	
	  system_dlg[0].x = x;
	  system_dlg[0].y = y;
	  system_dlg2[0].x = x;
	  system_dlg2[0].y = y;
	*/
	
	jwin_center_dialog(about_dlg);
	jwin_center_dialog(gamepad_dlg);
	jwin_center_dialog(credits_dlg);
	jwin_center_dialog(gamemode_dlg);
	jwin_center_dialog(getnum_dlg);
	jwin_center_dialog(goto_dlg);
	jwin_center_dialog(keyboard_control_dlg);
	jwin_center_dialog(midi_dlg);
	jwin_center_dialog(quest_dlg);
	jwin_center_dialog(scrsaver_dlg);
	jwin_center_dialog(sound_dlg);
	jwin_center_dialog(triforce_dlg);
	
	digi_dp[1] += scrx;
	digi_dp[2] += scry;
	midi_dp[1] += scrx;
	midi_dp[2] += scry;
	pan_dp[1]  += scrx;
	pan_dp[2]  += scry;
	emus_dp[1]  += scrx;
	emus_dp[2]  += scry;
	buf_dp[1]  += scrx;
	buf_dp[2]  += scry;
	sfx_dp[1]  += scrx;
	sfx_dp[2]  += scry;
}

/*****************************/
/**** Custom Sound System ****/
/*****************************/

INLINE int32_t mixvol(int32_t v1,int32_t v2)
{
	return (zc_min(v1,255)*zc_min(v2,255)) >> 8;
}

// Run an NSF, or a MIDI if the NSF is missing somehow.
bool try_zcmusic(char *filename, int32_t track, int32_t midi)
{
	ZCMUSIC *newzcmusic = zcmusic_load_for_quest(filename, qstpath);
	
	// Found it
	if(newzcmusic!=NULL)
	{
		zcmusic_stop(zcmusic);
		zcmusic_unload_file(zcmusic);
		stop_midi();
		
		zcmusic=newzcmusic;
		zcmusic_play(zcmusic, emusic_volume);
		
		if(track>0)
			zcmusic_change_track(zcmusic,track);
			
		return true;
	}
	
	// Not found, play MIDI - unless this was called by a script (yay, magic numbers)
	else if(midi>-1000)
		jukebox(midi);
		
	return false;
}

bool try_zcmusic_ex(char *filename, int32_t track, int32_t midi)
{
	ZCMUSIC *newzcmusic = zcmusic_load_for_quest(filename, qstpath);
	// Found it
	if(newzcmusic!=NULL)
	{
		zcmusic_stop(zcmusic);
		zcmusic_unload_file(zcmusic);
		stop_midi();
		
		zcmusic=newzcmusic;
		zcmusic_play(zcmusic, emusic_volume);
		
		if(track>0)
			zcmusic_change_track(zcmusic,track);
			
		return true;
	}
	
	// Not found, play MIDI - unless this was called by a script (yay, magic numbers)
	else if(midi>-1000)
		jukebox(midi);
		
	return false;
}

int32_t get_zcmusicpos()
{
	int32_t debugtracething = zcmusic_get_curpos(zcmusic);
	return debugtracething;
	return 0;
}

void set_zcmusicpos(int32_t position)
{
	zcmusic_set_curpos(zcmusic, position);
}

void set_zcmusicspeed(int32_t speed)
{
	int32_t newspeed = vbound(speed, 0, 10000);
	zcmusic_set_speed(zcmusic, newspeed);
}

void jukebox(int32_t index,int32_t loop)
{
	music_stop();
	
	if(index<0)		 index=MAXMIDIS-1;
	
	if(index>=MAXMIDIS) index=0;
	
	music_stop();
	
	// Allegro's DIGMID driver (the one normally used on on Linux) gets
	// stuck notes when a song stops. This fixes it.
	if(strcmp(midi_driver->name, "DIGMID")==0)
		set_volume(0, 0);
		
	set_volume(-1, mixvol(tunes[index].volume,midi_volume>>1));
	play_midi((MIDI*)tunes[index].data,loop);
	
	if(tunes[index].start>0)
		midi_seek(tunes[index].start);
		
	midi_loop_start = tunes[index].loop_start;
	midi_loop_end = tunes[index].loop_end;
	
	currmidi=index;
	master_volume(digi_volume,midi_volume);
	midi_paused=false;
}

void jukebox(int32_t index)
{
	if(index<0)		 index=MAXMIDIS-1;
	
	if(index>=MAXMIDIS) index=0;
	
	// do nothing if it's already playing
	if(index==currmidi && midi_pos>=0)
	{
		midi_paused=false;
		return;
	}
	
	jukebox(index,tunes[index].loop);
}

void play_DmapMusic()
{
	static char tfile[2048];
	static int32_t ttrack=0;
	bool domidi=false;
	
	if(DMaps[currdmap].tmusic[0]!=0)
	{
		if(zcmusic==NULL ||
		   strcmp(zcmusic->filename,DMaps[currdmap].tmusic)!=0 ||
		   (zcmusic->type==ZCMF_GME && zcmusic->track != DMaps[currdmap].tmusictrack))
		{
			if(zcmusic != NULL)
			{
				zcmusic_stop(zcmusic);
				zcmusic_unload_file(zcmusic);
				zcmusic = NULL;
			}
			
			zcmusic = zcmusic_load_for_quest(DMaps[currdmap].tmusic, qstpath);
			
			if(zcmusic!=NULL)
			{
				stop_midi();
				strcpy(tfile,DMaps[currdmap].tmusic);
				zcmusic_play(zcmusic, emusic_volume);
				int32_t temptracks=0;
				temptracks=zcmusic_get_tracks(zcmusic);
				temptracks=(temptracks<2)?1:temptracks;
				ttrack = vbound(DMaps[currdmap].tmusictrack,0,temptracks-1);
				zcmusic_change_track(zcmusic,ttrack);
			}
			else
			{
				tfile[0] = 0;
				domidi=true;
			}
		}
	}
	else
	{
		domidi=true;
	}
	
	if(domidi)
	{
		int32_t m=DMaps[currdmap].midi;
		
		switch(m)
		{
		case 1:
			jukebox(ZC_MIDI_OVERWORLD);
			break;
			
		case 2:
			jukebox(ZC_MIDI_DUNGEON);
			break;
			
		case 3:
			jukebox(ZC_MIDI_LEVEL9);
			break;
			
		default:
			if(m>=4 && m<4+MAXCUSTOMMIDIS)
				jukebox(m+MIDIOFFSET_DMAP);
			else
				music_stop();
		}
	}
}

void playLevelMusic()
{
	int32_t m=tmpscr->screen_midi;
	
	switch(m)
	{
	case -2:
		music_stop();
		break;
		
	case -1:
		play_DmapMusic();
		break;
		
	case 1:
		jukebox(ZC_MIDI_OVERWORLD);
		break;
		
	case 2:
		jukebox(ZC_MIDI_DUNGEON);
		break;
		
	case 3:
		jukebox(ZC_MIDI_LEVEL9);
		break;
		
	default:
		if(m>=4 && m<4+MAXCUSTOMMIDIS)
			jukebox(m+MIDIOFFSET_MAPSCR);
		else
			music_stop();
	}
}

void master_volume(int32_t dv,int32_t mv)
{
	if(dv>=0) digi_volume=zc_max(zc_min(dv,255),0);
	
	if(mv>=0) midi_volume=zc_max(zc_min(mv,255),0);
	
	int32_t i = zc_min(zc_max(currmidi,0),MAXMIDIS-1);
	set_volume(digi_volume,mixvol(tunes[i].volume,midi_volume));
}

/*****************/
/*****  SFX  *****/
/*****************/

// array of voices, one for each sfx sample in the data file
// 0+ = voice #
// -1 = voice not allocated
void Z_init_sound()
{
	for(int32_t i=0; i<WAV_COUNT; i++)
		sfx_voice[i]=-1;
		
	for(int32_t i=0; i<ZC_MIDI_COUNT; i++)
		tunes[i].data = (MIDI*)mididata[i].dat;
		
	for(int32_t j=0; j<MAXCUSTOMMIDIS; j++)
		tunes[ZC_MIDI_COUNT+j].data=NULL;
		
	master_volume(digi_volume,midi_volume);
}

// returns number of voices currently allocated
int32_t sfx_count()
{
	int32_t c=0;
	
	for(int32_t i=0; i<WAV_COUNT; i++)
		if(sfx_voice[i]!=-1)
			++c;
			
	return c;
}

// clean up finished samples
void sfx_cleanup()
{
	for(int32_t i=0; i<WAV_COUNT; i++)
		if(sfx_voice[i]!=-1 && voice_get_position(sfx_voice[i])<0)
		{
			deallocate_voice(sfx_voice[i]);
			sfx_voice[i]=-1;
		}
}

// allocates a voice for the sample "wav_index" (index into zelda.dat)
// if a voice is already allocated (and/or playing), then it just returns true
// Returns true:  voice is allocated
//		 false: unsuccessful
bool sfx_init(int32_t index)
{
	// check index
	if(index<=0 || index>=WAV_COUNT)
		return false;
		
	if(sfx_voice[index]==-1)
	{
		if(sfxdat)
		{
			if(index<Z35)
			{
				sfx_voice[index]=allocate_voice((SAMPLE*)sfxdata[index].dat);
			}
			else
			{
				sfx_voice[index]=allocate_voice((SAMPLE*)sfxdata[Z35].dat);
			}
		}
		else
		{
			sfx_voice[index]=allocate_voice(&customsfxdata[index]);
		}
		
		voice_set_volume(sfx_voice[index], sfx_volume);
	}
	
	return sfx_voice[index] != -1;
}

// plays an sfx sample
void sfx(int32_t index,int32_t pan,bool loop, bool restart)
{
	if(!sfx_init(index))
		return;
		
	voice_set_playmode(sfx_voice[index],loop?PLAYMODE_LOOP:PLAYMODE_PLAY);
	voice_set_pan(sfx_voice[index],pan);
	
	int32_t pos = voice_get_position(sfx_voice[index]);
	
	if(restart) voice_set_position(sfx_voice[index],0);
	
	if(pos<=0)
		voice_start(sfx_voice[index]);
}

// true if sfx is allocated
bool sfx_allocated(int32_t index)
{
	return (index>0 && index<WAV_COUNT && sfx_voice[index]!=-1);
}

// start it (in loop mode) if it's not already playing,
// otherwise adjust it to play in loop mode -DD
void cont_sfx(int32_t index)
{
	if(!sfx_init(index))
	{
		return;
	}
	
	if(voice_get_position(sfx_voice[index])<=0)
	{
		voice_set_position(sfx_voice[index],0);
		voice_set_playmode(sfx_voice[index],PLAYMODE_LOOP);
		voice_start(sfx_voice[index]);
	}
	else
	{
		adjust_sfx(index, 128, true);
	}
}

// adjust parameters while playing
void adjust_sfx(int32_t index,int32_t pan,bool loop)
{
	if(index<=0 || index>=WAV_COUNT || sfx_voice[index]==-1)
		return;
		
	voice_set_playmode(sfx_voice[index],loop?PLAYMODE_LOOP:PLAYMODE_PLAY);
	voice_set_pan(sfx_voice[index],pan);
}

// pauses a voice
void pause_sfx(int32_t index)
{
	if(index>0 && index<WAV_COUNT && sfx_voice[index]!=-1)
		voice_stop(sfx_voice[index]);
}

// resumes a voice
void resume_sfx(int32_t index)
{
	if(index>0 && index<WAV_COUNT && sfx_voice[index]!=-1)
		voice_start(sfx_voice[index]);
}

// pauses all active voices
void pause_all_sfx()
{
	for(int32_t i=0; i<WAV_COUNT; i++)
		if(sfx_voice[i]!=-1)
			voice_stop(sfx_voice[i]);
}

// resumes all paused voices
void resume_all_sfx()
{
	for(int32_t i=0; i<WAV_COUNT; i++)
		if(sfx_voice[i]!=-1)
			voice_start(sfx_voice[i]);
}

// stops an sfx and deallocates the voice
void stop_sfx(int32_t index)
{
	if(index<=0 || index>=WAV_COUNT)
		return;
		
	if(sfx_voice[index]!=-1)
	{
		deallocate_voice(sfx_voice[index]);
		sfx_voice[index]=-1;
	}
}

// Stops SFX played by Hero's item of the given family
void stop_item_sfx(int32_t family)
{
	int32_t id=current_item_id(family);
	
	if(id<0)
		return;
		
	stop_sfx(itemsbuf[id].usesound);
}

void kill_sfx()
{
	for(int32_t i=0; i<WAV_COUNT; i++)
		if(sfx_voice[i]!=-1)
		{
			deallocate_voice(sfx_voice[i]);
			sfx_voice[i]=-1;
		}
}

int32_t pan(int32_t x)
{
	switch(pan_style)
	{
	case 0:
		return 128;
		
	case 1:
		return vbound((x>>1)+68,0,255);
		
	case 2:
		return vbound(((x*3)>>2)+36,0,255);
	}
	
	return vbound(x,0,255);
}

/*******************************/
/******* Input Handlers ********/
/*******************************/

bool joybtn(int32_t b)
{
	if(b == 0)
		return false;
		
	return joy[joystick_index].button[b-1].b !=0;
}

const char* joybtn_name(int32_t b)
{
	if(b == 0)
		return "";

	return joy[joystick_index].button[b-1].name;
}

int32_t next_press_key()
{
	char k[127];
	
	for(int32_t i=0; i<127; i++)
		k[i]=key[i];
		
	for(;;)
	{
		for(int32_t i=0; i<127; i++)
			if(key[i]!=k[i])
				return i;
	}
	
	//	return (readkey()>>8);
}

int32_t next_press_btn()
{
	clear_keybuf();
	/*bool b[joy[joystick_index].num_buttons+1];
	
	for(int32_t i=1; i<=joy[joystick_index].num_buttons; i++)
		b[i]=joybtn(i);*/
		
	//first, we need to wait until they're pressing no buttons
	for(;;)
	{
		if(keypressed())
		{
			switch(readkey()>>8)
			{
			case KEY_ESC:
				return -1;
				
			case KEY_SPACE:
				return 0;
			}
		}
		
		poll_joystick();
		bool done = true;
		
		for(int32_t i=1; i<=joy[joystick_index].num_buttons; i++)
		{
			if(joybtn(i)) done = false;
		}
		
		if(done) break;
	}
	
	//now, we need to wait for them to press any button
	for(;;)
	{
		if(keypressed())
		{
			switch(readkey()>>8)
			{
			case KEY_ESC:
				return -1;
				
			case KEY_SPACE:
				return 0;
			}
		}
		
		poll_joystick();
		
		for(int32_t i=1; i<=joy[joystick_index].num_buttons; i++)
		{
			if(joybtn(i)) return i;
		}
	}
}

static bool rButton(bool(proc)(),bool &flag)
{
	if(!proc())
	{
		flag=false;
	}
	else if(!flag)
	{
		flag=true;
		return true;
	}
	
	return false;
}

static bool rButton(bool &btn, bool &flag)
{
	if(!btn)
	{
		flag=false;
	}
	else if(!flag)
	{
		flag=true;
		return true;
	}
	
	return false;
}
static bool rButtonPeek(bool btn, bool flag)
{
	if(!btn)
	{
		return false;
	}
	else if(!flag)
	{
		return true;
	}
	
	return false;
}

// Updated only by keyboard/gamepad.
// If in replay mode, this is set directly by the replay system.
// This should never be read from directly - use control_state instead.
bool raw_control_state[ZC_CONTROL_STATES]=
{
	false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false
};

// Every call to load_control_state (pretty much every frame) resets this to be equal to raw_control_state.
// This state can drift from raw_control_state if button states are "eaten" or overriden by a script. But that only
// lasts until the next call to load_control_state.
bool control_state[ZC_CONTROL_STATES]=
{
	false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false
};

bool disable_control[ZC_CONTROL_STATES]=
{
	false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false
};

bool drunk_toggle_state[11]=
{
	false, false, false, false, false, false, false, false, false, false, false
};

bool disabledKeys[127]=
{
	false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,
	false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,
	false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,
	false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,
	false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,
	false,false,false,false,false,false,false
};

bool KeyInput[127]=
{
	false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,
	false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,
	false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,
	false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,
	false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,
	false,false,false,false,false,false,false
};

bool KeyPress[127]=
{
	false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,
	false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,
	false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,
	false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,
	false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,
	false,false,false,false,false,false,false
};

bool key_truestate[127]=
{
	false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,
	false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,
	false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,
	false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,
	false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,
	false,false,false,false,false,false,false
};

bool button_press[ZC_CONTROL_STATES] = {false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false};
bool button_hold[ZC_CONTROL_STATES] = {false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false};

#define STICK_1_X joy[joystick_index].stick[js_stick_1_x_stick].axis[js_stick_1_x_axis]
#define STICK_1_Y joy[joystick_index].stick[js_stick_1_y_stick].axis[js_stick_1_y_axis]
#define STICK_2_X joy[joystick_index].stick[js_stick_2_x_stick].axis[js_stick_2_x_axis]
#define STICK_2_Y joy[joystick_index].stick[js_stick_2_y_stick].axis[js_stick_2_y_axis]
#define STICK_PRECISION   56 //define your own sensitivity

void load_control_state()
{
	locking_keys = true;
	if (!replay_is_replaying())
	{
		raw_control_state[0]=zc_getrawkey(DUkey, true)||(analog_movement ? STICK_1_Y.d1 || STICK_1_Y.pos - js_stick_1_y_offset < -STICK_PRECISION : joybtn(DUbtn));
		raw_control_state[1]=zc_getrawkey(DDkey, true)||(analog_movement ? STICK_1_Y.d2 || STICK_1_Y.pos - js_stick_1_y_offset > STICK_PRECISION : joybtn(DDbtn));
		raw_control_state[2]=zc_getrawkey(DLkey, true)||(analog_movement ? STICK_1_X.d1 || STICK_1_X.pos - js_stick_1_x_offset < -STICK_PRECISION : joybtn(DLbtn));
		raw_control_state[3]=zc_getrawkey(DRkey, true)||(analog_movement ? STICK_1_X.d2 || STICK_1_X.pos - js_stick_1_x_offset > STICK_PRECISION : joybtn(DRbtn));
		raw_control_state[4]=zc_getrawkey(Akey, true)||joybtn(Abtn);
		raw_control_state[5]=zc_getrawkey(Bkey, true)||joybtn(Bbtn);
		raw_control_state[6]=zc_getrawkey(Skey, true)||joybtn(Sbtn);
		raw_control_state[7]=zc_getrawkey(Lkey, true)||joybtn(Lbtn);
		raw_control_state[8]=zc_getrawkey(Rkey, true)||joybtn(Rbtn);
		raw_control_state[9]=zc_getrawkey(Pkey, true)||joybtn(Pbtn);
		raw_control_state[10]=zc_getrawkey(Exkey1, true)||joybtn(Exbtn1);
		raw_control_state[11]=zc_getrawkey(Exkey2, true)||joybtn(Exbtn2);
		raw_control_state[12]=zc_getrawkey(Exkey3, true)||joybtn(Exbtn3);
		raw_control_state[13]=zc_getrawkey(Exkey4, true)||joybtn(Exbtn4);
		
		if(num_joysticks != 0)
		{
			raw_control_state[14] = STICK_2_Y.pos - js_stick_2_y_offset < -STICK_PRECISION;
			raw_control_state[15] = STICK_2_Y.pos - js_stick_2_y_offset > STICK_PRECISION;
			raw_control_state[16] = STICK_2_X.pos - js_stick_2_x_offset < -STICK_PRECISION;
			raw_control_state[17] = STICK_2_X.pos - js_stick_2_x_offset > STICK_PRECISION;
			// zprint2("Detected %d joysticks... %d%d%d%d\n", num_joysticks, raw_control_state[14]?1:0, raw_control_state[15]?1:0, raw_control_state[16]?1:0, raw_control_state[17]?1:0);
		}
		else
		{
			raw_control_state[14] = false;
			raw_control_state[15] = false;
			raw_control_state[16] = false;
			raw_control_state[17] = false;
			// zprint2("Detected 0 joysticks... clearing inputaxis values.\n");
		}
	}
	replay_poll();
	locking_keys = false;

	// Some test replay files were made before a serious input bug was fixed, so instead
	// of re-doing them or tossing them out, just check for that zplay version.
	bool botched_input = replay_is_replaying() && replay_get_meta_int("version", 1) == 1;
	for (int i = 0; i < ZC_CONTROL_STATES; i++)
	{
		control_state[i] = raw_control_state[i];
		if(!botched_input && !control_state[i])
			down_control_states[i] = false;
	}
	
	button_press[0]=rButton(control_state[0],button_hold[0]);
	button_press[1]=rButton(control_state[1],button_hold[1]);
	button_press[2]=rButton(control_state[2],button_hold[2]);
	button_press[3]=rButton(control_state[3],button_hold[3]);
	button_press[4]=rButton(control_state[4],button_hold[4]);
	button_press[5]=rButton(control_state[5],button_hold[5]);
	button_press[6]=rButton(control_state[6],button_hold[6]);
	button_press[7]=rButton(control_state[7],button_hold[7]);
	button_press[8]=rButton(control_state[8],button_hold[8]);
	button_press[9]=rButton(control_state[9],button_hold[9]);
	button_press[10]=rButton(control_state[10],button_hold[10]);
	button_press[11]=rButton(control_state[11],button_hold[11]);
	button_press[12]=rButton(control_state[12],button_hold[12]);
	button_press[13]=rButton(control_state[13],button_hold[13]);
	button_press[14]=rButton(control_state[14],button_hold[14]);
	button_press[15]=rButton(control_state[15],button_hold[15]);
	button_press[16]=rButton(control_state[16],button_hold[16]);
	button_press[17]=rButton(control_state[17],button_hold[17]);
}

// Returns true if any game key is pressed. This is needed because keypressed()
// doesn't detect modifier keys and control_state[] can be modified by scripts.
bool zc_key_pressed()
//may also need to use zc_getrawkey
{
	if((zc_getrawkey(DUkey, true)||(analog_movement ? STICK_1_Y.d1 || STICK_1_Y.pos - js_stick_1_y_offset< -STICK_PRECISION : joybtn(DUbtn))) ||
	   (zc_getrawkey(DDkey, true)||(analog_movement ? STICK_1_Y.d2 || STICK_1_Y.pos - js_stick_1_y_offset > STICK_PRECISION : joybtn(DDbtn))) ||
	   (zc_getrawkey(DLkey, true)||(analog_movement ? STICK_1_X.d1 || STICK_1_X.pos - js_stick_1_x_offset < -STICK_PRECISION : joybtn(DLbtn))) ||
	   (zc_getrawkey(DRkey, true)||(analog_movement ? STICK_1_X.d2 || STICK_1_X.pos - js_stick_1_x_offset > STICK_PRECISION : joybtn(DRbtn))) ||
	   (zc_getrawkey(Akey, true)||joybtn(Abtn)) ||
	   (zc_getrawkey(Bkey, true)||joybtn(Bbtn)) ||
	   (zc_getrawkey(Skey, true)||joybtn(Sbtn)) ||
	   (zc_getrawkey(Lkey, true)||joybtn(Lbtn)) ||
	   (zc_getrawkey(Rkey, true)||joybtn(Rbtn)) ||
	   (zc_getrawkey(Pkey, true)||joybtn(Pbtn)) ||
	   (zc_getrawkey(Exkey1, true)||joybtn(Exbtn1)) ||
	   (zc_getrawkey(Exkey2, true)||joybtn(Exbtn2)) ||
	   (zc_getrawkey(Exkey3, true)||joybtn(Exbtn3)) ||
	   (zc_getrawkey(Exkey4, true)||joybtn(Exbtn4))) // Skipping joystick axes
		return true;
	
	return false;
}

bool getInput(int32_t btn, bool press, bool drunk, bool ignoreDisable, bool eatEntirely, bool peek)
{
	bool ret = false, drunkstate = false;
	bool* flag = &down_control_states[btn];
	switch(btn)
	{
		case btnF12:
			ret = zc_getkey(KEY_F12, ignoreDisable);
			eatEntirely = false;
			break;
		case btnF11:
			ret = zc_getkey(KEY_F11, ignoreDisable);
			eatEntirely = false;
			break;
		case btnF5:
			ret = zc_getkey(KEY_F5, ignoreDisable);
			eatEntirely = false;
			break;
		case btnQ:
			ret = zc_getkey(KEY_Q, ignoreDisable);
			eatEntirely = false;
			break;
		case btnI:
			ret = zc_getkey(KEY_I, ignoreDisable);
			eatEntirely = false;
			break;
		case btnM:
			if(FFCore.kb_typing_mode) return false;
			ret = zc_getrawkey(KEY_ESC, ignoreDisable);
			eatEntirely = false;
			break;
		default: //control_state[] index
			if(FFCore.kb_typing_mode) return false;
			if(!ignoreDisable && get_bit(quest_rules, qr_FIXDRUNKINPUTS) && disable_control[btn]) drunk = false;
			else if(btn<11) drunkstate = drunk_toggle_state[btn];
			ret = control_state[btn] && (ignoreDisable || !disable_control[btn]);
	}
	assert(flag);
	if(press)
	{
		if(peek)
			ret = rButtonPeek(ret, *flag);
		else ret = rButton(ret, *flag);
	}
	if(eatEntirely && ret) control_state[btn] = false;
	if(drunk && drunkstate) ret = !ret;
	return ret;
}

byte getIntBtnInput(byte intbtn, bool press, bool drunk, bool ignoreDisable, bool eatEntirely, bool peek)
{
	byte ret = 0;
	if(intbtn & INT_BTN_A) ret |= getInput(btnA, press, drunk, ignoreDisable, eatEntirely, peek) ? INT_BTN_A : 0;
	if(intbtn & INT_BTN_B) ret |= getInput(btnB, press, drunk, ignoreDisable, eatEntirely, peek) ? INT_BTN_B : 0;
	if(intbtn & INT_BTN_L) ret |= getInput(btnL, press, drunk, ignoreDisable, eatEntirely, peek) ? INT_BTN_L : 0;
	if(intbtn & INT_BTN_R) ret |= getInput(btnR, press, drunk, ignoreDisable, eatEntirely, peek) ? INT_BTN_R : 0;
	if(intbtn & INT_BTN_EX1) ret |= getInput(btnEx1, press, drunk, ignoreDisable, eatEntirely, peek) ? INT_BTN_EX1 : 0;
	if(intbtn & INT_BTN_EX2) ret |= getInput(btnEx2, press, drunk, ignoreDisable, eatEntirely, peek) ? INT_BTN_EX2 : 0;
	if(intbtn & INT_BTN_EX3) ret |= getInput(btnEx3, press, drunk, ignoreDisable, eatEntirely, peek) ? INT_BTN_EX3 : 0;
	if(intbtn & INT_BTN_EX4) ret |= getInput(btnEx4, press, drunk, ignoreDisable, eatEntirely, peek) ? INT_BTN_EX4 : 0;
	return ret; //No early return, to make sure all button presses are eaten that should be! -Em
}

byte checkIntBtnVal(byte intbtn, byte vals)
{
	return intbtn&vals;
}

bool Up()
{
	return getInput(btnUp);
}
bool Down()
{
	return getInput(btnDown);
}
bool Left()
{
	return getInput(btnLeft);
}
bool Right()
{
	return getInput(btnRight);
}
bool cAbtn()
{
	return getInput(btnA);
}
bool cBbtn()
{
	return getInput(btnB);
}
bool cSbtn()
{
	return getInput(btnS);
}
bool cLbtn()
{
	return getInput(btnL);
}
bool cRbtn()
{
	return getInput(btnR);
}
bool cPbtn()
{
	return getInput(btnP);
}
bool cEx1btn()
{
	return getInput(btnEx1);
}
bool cEx2btn()
{
	return getInput(btnEx2);
}
bool cEx3btn()
{
	return getInput(btnEx3);
}
bool cEx4btn()
{
	return getInput(btnEx4);
}
bool AxisUp()
{
	return getInput(btnAxisUp);
}
bool AxisDown()
{
	return getInput(btnAxisDown);
}
bool AxisLeft()
{
	return getInput(btnAxisLeft);
}
bool AxisRight()
{
	return getInput(btnAxisRight);
}

bool cMbtn()
{
	return getInput(btnM);
}
bool cF12()
{
	return getInput(btnF12);
}
bool cF11()
{
	return getInput(btnF11);
}
bool cF5()
{
	return getInput(btnF5);
}
bool cQ()
{
	return getInput(btnQ);
}
bool cI()
{
	return getInput(btnI);
}

bool rUp()
{
	return getInput(btnUp, true);
}
bool rDown()
{
	return getInput(btnDown, true);
}
bool rLeft()
{
	return getInput(btnLeft, true);
}
bool rRight()
{
	return getInput(btnRight, true);
}
bool rAbtn()
{
	return getInput(btnA, true);
}
bool rBbtn()
{
	return getInput(btnB, true);
}
bool rSbtn()
{
	return getInput(btnS, true);
}
bool rMbtn()
{
	return getInput(btnM, true);
}
bool rLbtn()
{
	return getInput(btnL, true);
}
bool rRbtn()
{
	return getInput(btnR, true);
}
bool rPbtn()
{
	return getInput(btnP, true);
}
bool rEx1btn()
{
	return getInput(btnEx1, true);
}
bool rEx2btn()
{
	return getInput(btnEx2, true);
}
bool rEx3btn()
{
	return getInput(btnEx3, true);
}
bool rEx4btn()
{
	return getInput(btnEx4, true);
}
bool rAxisUp()
{
	return getInput(btnAxisUp, true);
}
bool rAxisDown()
{
	return getInput(btnAxisDown, true);
}
bool rAxisLeft()
{
	return getInput(btnAxisLeft, true);
}
bool rAxisRight()
{
	return getInput(btnAxisRight, true);
}

bool rF12()
{
	return getInput(btnF12, true);
}
bool rF11()
{
	return getInput(btnF11, true);
}
bool rF5()
{
	return getInput(btnF5, true);
}
bool rQ()
{
	return getInput(btnQ, true);
}
bool rI()
{
	return getInput(btnI, true);
}

bool DrunkUp()
{
	return getInput(btnUp, false, true);
}
bool DrunkDown()
{
	return getInput(btnDown, false, true);
}
bool DrunkLeft()
{
	return getInput(btnLeft, false, true);
}
bool DrunkRight()
{
	return getInput(btnRight, false, true);
}
bool DrunkcAbtn()
{
	return getInput(btnA, false, true);
}
bool DrunkcBbtn()
{
	return getInput(btnB, false, true);
}
bool DrunkcEx1btn()
{
	return getInput(btnEx1, false, true);
}
bool DrunkcEx2btn()
{
	return getInput(btnEx2, false, true);
}
bool DrunkcSbtn()
{
	return getInput(btnS, false, true);
}
bool DrunkcMbtn()
{
	return getInput(btnM, false, true);
}
bool DrunkcLbtn()
{
	return getInput(btnL, false, true);
}
bool DrunkcRbtn()
{
	return getInput(btnR, false, true);
}
bool DrunkcPbtn()
{
	return getInput(btnP, false, true);
}

bool DrunkrUp()
{
	return getInput(btnUp, true, true);
}
bool DrunkrDown()
{
	return getInput(btnDown, true, true);
}
bool DrunkrLeft()
{
	return getInput(btnLeft, true, true);
}
bool DrunkrRight()
{
	return getInput(btnRight, true, true);
}
bool DrunkrAbtn()
{
	return getInput(btnA, true, true);
}
bool DrunkrBbtn()
{
	return getInput(btnB, true, true);
}
bool DrunkrEx1btn()
{
	return getInput(btnEx1, true, true);
}
bool DrunkrEx2btn()
{
	return getInput(btnEx2, true, true);
}
bool DrunkrEx3btn()
{
	return getInput(btnEx3, true, true);
}
bool DrunkrEx4btn()
{
	return getInput(btnEx4, true, true);
}
bool DrunkrSbtn()
{
	return getInput(btnS, true, true);
}
bool DrunkrMbtn()
{
	return getInput(btnM, true, true);
}
bool DrunkrLbtn()
{
	return getInput(btnL, true, true);
}
bool DrunkrRbtn()
{
	return getInput(btnR, true, true);
}
bool DrunkrPbtn()
{
	return getInput(btnP, true, true);
}

void eat_buttons()
{
	getInput(btnA, true, false, true);
	getInput(btnB, true, false, true);
	getInput(btnS, true, false, true);
	getInput(btnM, true, false, true);
	getInput(btnL, true, false, true);
	getInput(btnR, true, false, true);
	getInput(btnP, true, false, true);
	getInput(btnEx1, true, false, true);
	getInput(btnEx2, true, false, true);
	getInput(btnEx3, true, false, true);
	getInput(btnEx4, true, false, true);
}

bool zc_readkey(int32_t k, bool ignoreDisable)
{
	if(ignoreDisable) return KeyPress[k];
	switch(k)
	{
		case KEY_F7:
		case KEY_F8:
		case KEY_F9:
			return KeyPress[k];
			
		default:
			return KeyPress[k] && !disabledKeys[k];
	}
}

bool zc_getkey(int32_t k, bool ignoreDisable)
{
	if(ignoreDisable) return KeyInput[k];
	switch(k)
	{
		case KEY_F7:
		case KEY_F8:
		case KEY_F9:
			return KeyInput[k];
			
		default:
			return KeyInput[k] && !disabledKeys[k];
	}
}

bool zc_readrawkey(int32_t k, bool ignoreDisable)
{
	if(zc_getrawkey(k, ignoreDisable))
	{
		key[k]=0;
		return true;
	}
	key[k]=0;
	return false;
}

bool zc_getrawkey(int32_t k, bool ignoreDisable)
{
	if(ignoreDisable) return key[k];
	switch(k)
	{
		case KEY_F7:
		case KEY_F8:
		case KEY_F9:
			return key[k];
			
		default:
			return key[k] && !disabledKeys[k];
	}
}

void update_keys()
{
	for(int32_t q = 0; q < 127; ++q)
	{
		KeyPress[q] = key[q] && !key_truestate[q];
		KeyInput[q] = key[q];
		key_truestate[q] = key[q];
	}
}

bool zc_disablekey(int32_t k, bool val)
{
	switch(k)
	{
		case KEY_F7:
		case KEY_F8:
		case KEY_F9:
			return false;
			
		default:
			disabledKeys[k] = val;
			return true;
	}
}

void zc_putpixel(int32_t layer, int32_t x, int32_t y, int32_t cset, int32_t color, int32_t timer)
{
	timer=timer;
	particles.add(new particle(zfix(x), zfix(y), layer, cset, color));
}

// these are here so that copy_dialog won't choke when compiling zelda
int32_t d_alltriggerbutton_proc(int32_t, DIALOG*, int32_t)
{
	return D_O_K;
}

int32_t d_comboa_radio_proc(int32_t, DIALOG*, int32_t)
{
	return D_O_K;
}

int32_t d_comboabutton_proc(int32_t, DIALOG*, int32_t)
{
	return D_O_K;
}

int32_t d_ssdn_btn_proc(int32_t, DIALOG*, int32_t)
{
	return D_O_K;
}

int32_t d_ssdn_btn2_proc(int32_t, DIALOG*, int32_t)
{
	return D_O_K;
}

int32_t d_ssdn_btn3_proc(int32_t, DIALOG*, int32_t)
{
	return D_O_K;
}

int32_t d_ssdn_btn4_proc(int32_t, DIALOG*, int32_t)
{
	return D_O_K;
}

int32_t d_sslt_btn_proc(int32_t, DIALOG*, int32_t)
{
	return D_O_K;
}

int32_t d_sslt_btn2_proc(int32_t, DIALOG*, int32_t)
{
	return D_O_K;
}

int32_t d_sslt_btn3_proc(int32_t, DIALOG*, int32_t)
{
	return D_O_K;
}

int32_t d_sslt_btn4_proc(int32_t, DIALOG*, int32_t)
{
	return D_O_K;
}

int32_t d_ssrt_btn_proc(int32_t, DIALOG*, int32_t)
{
	return D_O_K;
}

int32_t d_ssrt_btn2_proc(int32_t, DIALOG*, int32_t)
{
	return D_O_K;
}

int32_t d_ssrt_btn3_proc(int32_t, DIALOG*, int32_t)
{
	return D_O_K;
}

int32_t d_ssrt_btn4_proc(int32_t, DIALOG*, int32_t)
{
	return D_O_K;
}

int32_t d_ssup_btn_proc(int32_t, DIALOG*, int32_t)
{
	return D_O_K;
}

int32_t d_ssup_btn2_proc(int32_t, DIALOG*, int32_t)
{
	return D_O_K;
}

int32_t d_ssup_btn3_proc(int32_t, DIALOG*, int32_t)
{
	return D_O_K;
}

int32_t d_ssup_btn4_proc(int32_t, DIALOG*, int32_t)
{
	return D_O_K;
}

int32_t d_tri_edit_proc(int32_t, DIALOG*, int32_t)
{
	return D_O_K;
}

int32_t d_triggerbutton_proc(int32_t, DIALOG*, int32_t)
{
	return D_O_K;
}

/*** end of zc_sys.cc ***/

