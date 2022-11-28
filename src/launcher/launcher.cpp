// Launcher executable main file
#include "base/zdefs.h"
#include "launcher.h"
#include "base/module.h"
#include "base/fonts.h"
#include "dialog/alert.h"
#include "launcher_dialog.h"
#include "zqscale.h"
#include "base/zapp.h"

#define QUICK_EXIT 0


#define QUIT_LAUNCHER() \
do{ \
	return 1; \
} \
while(false)

DATAFILE *fontsdata;
PALETTE RAMpal;

ZModule zcm;
zcmodule moduledata;

volatile int32_t lastfps = 0;
volatile int32_t framecnt = 0;
int32_t joystick_index = 0;
int32_t readsize = 0, writesize = 0;
volatile int32_t myvsync=0;
extern int32_t zqwin_scale;
int32_t zq_screen_w=800;
int32_t zq_screen_h=600;
BITMAP *tmp_scr;;
BITMAP *mouse_bmp;
int32_t gui_colorset = 99;
volatile bool close_button_quit = false;

byte quest_rules[QUESTRULES_NEW_SIZE];

char temppath[4096] = {0}, rootpath[4096] = {0};


void init_launcher_palette();
void fps_callback();
void myvsync_callback();

int32_t cursorColor(int32_t col)
{
	switch(col)
	{
		case dvc(1):
		case dvc(4):
			return jwin_pal[jcCURSORMISC];
		case dvc(2):
			return jwin_pal[jcCURSOROUTLINE];
		case dvc(3):
			return jwin_pal[jcCURSORLIGHT];
		case dvc(5):
			return jwin_pal[jcCURSORDARK];
	}
	return col;
}
void load_mouse()
{
	gui_mouse_focus=0;
	scare_mouse();
	set_mouse_sprite(NULL);
	byte mousepx[16][16] = { { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, }, { 0, 242, 242, 242, 242, 242, 242, 242, 242, 242, 242, 242, 242, 242, 242, 0, }, { 0, 241, 242, 243, 245, 245, 245, 245, 245, 245, 245, 245, 243, 241, 0, 0, }, { 0, 0, 241, 242, 243, 243, 245, 245, 245, 245, 245, 245, 243, 241, 0, 0, }, { 0, 0, 0, 241, 242, 243, 243, 243, 245, 245, 245, 243, 241, 0, 0, 0, }, { 0, 0, 0, 0, 241, 242, 243, 243, 243, 243, 245, 243, 241, 0, 0, 0, }, { 0, 0, 0, 0, 0, 241, 242, 243, 243, 243, 243, 241, 0, 0, 0, 0, }, { 0, 0, 0, 0, 0, 0, 241, 242, 243, 243, 243, 241, 0, 0, 0, 0, }, { 0, 0, 0, 0, 0, 0, 0, 241, 242, 243, 243, 241, 0, 0, 0, 0, }, { 0, 0, 0, 0, 0, 0, 0, 0, 241, 242, 241, 0, 0, 0, 0, 0, }, { 0, 0, 0, 0, 0, 0, 0, 0, 0, 241, 241, 0, 0, 0, 0, 0, }, { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 241, 0, 0, 0, 0, 0, }, { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, }, { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, }, { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, }, { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, } };
	for(auto x = 0; x < 16; ++x)
		for(auto y = 0; y < 16; ++y)
		{
			putpixel(mouse_bmp, x, y, cursorColor(mousepx[x][y]));
		}
	set_mouse_sprite(mouse_bmp);
	unscare_mouse();
}

void hit_close_button()
{
    close_button_quit=true;
    return;
}

int32_t main(int32_t argc, char* argv[])
{
	common_main_setup(App::launcher, argc, argv);

	set_uformat(U_ASCII);
	zc_srand(time(0));
	
	
	Z_message("Initializing Allegro... "); //{
	allegro_init();

	// Merge old a4 config into a5 system config.
	ALLEGRO_CONFIG *tempcfg = al_load_config_file(zc_get_standard_config_name());
	if (tempcfg) {
		al_merge_config_into(al_get_system_config(), tempcfg);
		al_destroy_config(tempcfg);
	}

	zc_set_config_standard();

	if(install_timer() < 0
		|| install_keyboard() < 0
		|| install_mouse() < 0)
	{
		Z_error_fatal(allegro_error);
		QUIT_LAUNCHER();
	}
	
	LOCK_VARIABLE(lastfps);
	
	LOCK_VARIABLE(framecnt);
	LOCK_FUNCTION(fps_callback);
	
	if(install_int_ex(fps_callback,SECS_TO_TIMER(1)))
	{
		Z_error_fatal("couldn't allocate timer\n");
		QUIT_LAUNCHER();
	}
	LOCK_VARIABLE(myvsync);
	LOCK_FUNCTION(myvsync_callback);
	if(install_int_ex(myvsync_callback,BPS_TO_TIMER(60)))
	{
		Z_error_fatal("couldn't allocate timer\n");
		QUIT_LAUNCHER();
	}
	
	LOCK_VARIABLE(dclick_status);
	LOCK_VARIABLE(dclick_time);
	lock_dclick_function();
	install_int(dclick_check, 20);
	
	
	set_gfx_mode(GFX_TEXT,80,50,0,0);
	Z_message("OK\n");
	//} end Initializing Allegro...OK
	
	Z_message("Initializing module...");
	zcm.init(true);
	Z_message("OK\n");
	
	Z_message("Loading data files:\n"); //{
	
	resolve_password(datapwd);
	packfile_password(datapwd);
	
	Z_message("....Fonts.Dat..."); //{
	if((fontsdata=load_datafile(moduledata.datafiles[fonts_dat]))==NULL)
	{
		Z_error_fatal("failed: load error\n");
		QUIT_LAUNCHER();
	}
	char fontsdat_sig[52]={0};
	sprintf(fontsdat_sig,"Fonts.Dat %s Build %d",VerStr(FONTSDAT_VERSION), FONTSDAT_BUILD);
	if(strncmp((char*)fontsdata[0].dat,fontsdat_sig,24))
	{
		Z_error_fatal("failed: version error\n");
		QUIT_LAUNCHER();
	}
	initFonts();
	Z_message("OK\n");
	//} end Fonts.Dat...OK
	packfile_password("");
	
	Z_message("....OK\n");
	//} end Loading data files:
	
	set_color_depth(8);

	int scale_arg = used_switch(argc,argv,"-scale");
	if(scale_arg && (argc>(scale_arg+1)))
	{
		scale_arg = atoi(argv[scale_arg+1]);

		if(scale_arg == 0)
		{
			scale_arg = 1;
		}

		zqwin_set_scale(scale_arg);
	} else {
#ifdef __APPLE__
		zqwin_set_scale(2);
#endif
	}

	int32_t videofail = set_gfx_mode(GFX_AUTODETECT_WINDOWED,zq_screen_w*zqwin_scale,zq_screen_h*zqwin_scale,0,0);
	
	if(videofail)
	{
		Z_error_fatal(allegro_error);
		QUIT_LAUNCHER();
	}

	if (zc_get_config("ZLAUNCH","hw_cursor",0) == 1)
	{
		// Must wait for the display thread to create the a5 display before the
		// hardware cursor can be enabled.
		while (!all_get_display()) rest(1);
		enable_hardware_cursor();
		select_mouse_cursor(MOUSE_CURSOR_ARROW);
	}
	else
	{
#ifdef _WIN32
		while (!all_get_display()) rest(1);
		al_hide_mouse_cursor(all_get_display());
#endif
	}
	
	Z_message("Loading bitmaps..."); //{
	tmp_scr = create_bitmap_ex(8,zq_screen_w,zq_screen_h);
	mouse_bmp = create_bitmap_ex(8,16,16);
	//{ Screen setup
	screen = create_bitmap_ex(8, zq_screen_w, zq_screen_h);
	//}
	
	if(!(tmp_scr && mouse_bmp && screen))
	{
		Z_error_fatal("failed\n");
		QUIT_LAUNCHER();
	}
	Z_message("OK\n");
	//}
	
	// while(!key[KEY_SPACE]);
	
	Z_message("Loading configs...");
	gui_colorset = zc_get_config("ZLAUNCH","gui_colorset",99);
	Z_message("OK\n");
	
	Z_message("Initializing palette...");
	init_launcher_palette();
	Z_message("OK\n");
	
	
	Z_message("Initializing mouse...");
	//{ Mouse setup
	load_mouse();
	show_mouse(screen);
	//}
	Z_message("OK\n");
	
	get_root_path(rootpath, 4096);
	
	set_window_title("ZQuest Launcher");
	set_close_button_callback((void (*)()) hit_close_button);
	//
	Z_message("Launcher opened successfully.\n");
	
	#if QUICK_EXIT > 0
	goto exit;
	#endif
	
	LauncherDialog().show();

	#if QUICK_EXIT > 0
	exit:
	#endif
	Z_message("Exiting launcher...\n");
	//
	
	flush_config_file();
	return 0;
}
END_OF_MAIN()

//Things required to compile from shared files... le sigh -Em
bool is_large = true; //scaling
bool is_zquest() //Used for sizing purposes
{
	return true;
}

//{ Nil funcs

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

int32_t dcs_auto_button_proc(int32_t, DIALOG*, int32_t)
{
	return D_O_K;
}

int32_t d_jbutton_proc(int32_t, DIALOG*, int32_t)
{
	return D_O_K;
}

int32_t d_kbutton_proc(int32_t, DIALOG*, int32_t)
{
	return D_O_K;
}

int32_t d_listen_proc(int32_t, DIALOG*, int32_t)
{
	return D_O_K;
}

int32_t d_savemidi_proc(int32_t, DIALOG*, int32_t)
{
	return D_O_K;
}

int32_t d_dummy_proc(int32_t,DIALOG *,int32_t)
{
	return D_O_K;
}

int32_t d_timer_proc(int32_t, DIALOG *, int32_t)
{
	return D_O_K;
}

void go()
{
	scare_mouse();
	blit(screen,tmp_scr,0,0,0,0,screen->w,screen->h);
	unscare_mouse();
}

void comeback()
{
	scare_mouse();
	blit(tmp_scr,screen,0,0,0,0,screen->w,screen->h);
	unscare_mouse();
}

void large_dialog(DIALOG *)
{
}

//}

void init_launcher_palette()
{
	get_palette(RAMpal);
	//{ Hardcoded instead of datafile
	for(auto q = 0; q < 224; ++q)
	{
		RAMpal[q].r = 16; RAMpal[q].g =  0; RAMpal[q].b = 16;
	}
	RAMpal[224].r =  0; RAMpal[224].g =  0; RAMpal[224].b =  0;
	RAMpal[225].r =  0; RAMpal[225].g =  0; RAMpal[225].b = 42;
	RAMpal[226].r =  0; RAMpal[226].g = 39; RAMpal[226].b =  0;
	RAMpal[227].r =  0; RAMpal[227].g = 42; RAMpal[227].b = 42;
	RAMpal[228].r = 44; RAMpal[228].g =  9; RAMpal[228].b =  9;
	RAMpal[229].r = 42; RAMpal[229].g =  0; RAMpal[229].b = 42;
	RAMpal[230].r = 41; RAMpal[230].g = 26; RAMpal[230].b =  2;
	RAMpal[231].r = 42; RAMpal[231].g = 42; RAMpal[231].b = 42;
	RAMpal[232].r = 21; RAMpal[232].g = 21; RAMpal[232].b = 21;
	RAMpal[233].r = 21; RAMpal[233].g = 21; RAMpal[233].b = 63;
	RAMpal[234].r = 21; RAMpal[234].g = 63; RAMpal[234].b = 21;
	RAMpal[235].r = 21; RAMpal[235].g = 63; RAMpal[235].b = 63;
	RAMpal[236].r = 63; RAMpal[236].g = 21; RAMpal[236].b = 21;
	RAMpal[237].r = 63; RAMpal[237].g = 21; RAMpal[237].b = 63;
	RAMpal[238].r = 63; RAMpal[238].g = 63; RAMpal[238].b = 21;
	RAMpal[239].r = 63; RAMpal[239].g = 63; RAMpal[239].b = 63;
	RAMpal[240].r =  0; RAMpal[240].g =  0; RAMpal[240].b = 32;
	RAMpal[241].r =  0; RAMpal[241].g =  0; RAMpal[241].b =  0;
	RAMpal[242].r = 16; RAMpal[242].g = 16; RAMpal[242].b = 16;
	RAMpal[243].r = 32; RAMpal[243].g = 32; RAMpal[243].b = 32;
	RAMpal[244].r = 52; RAMpal[244].g = 51; RAMpal[244].b = 49;
	RAMpal[245].r = 63; RAMpal[245].g = 63; RAMpal[245].b = 63;
	RAMpal[246].r = 63; RAMpal[246].g = 63; RAMpal[246].b = 55;
	RAMpal[247].r = 63; RAMpal[247].g = 55; RAMpal[247].b = 39;
	RAMpal[248].r =  0; RAMpal[248].g =  0; RAMpal[248].b = 19;
	RAMpal[249].r =  2; RAMpal[249].g =  8; RAMpal[249].b = 26;
	RAMpal[250].r =  8; RAMpal[250].g = 14; RAMpal[250].b = 31;
	RAMpal[251].r = 15; RAMpal[251].g = 21; RAMpal[251].b = 37;
	RAMpal[252].r = 21; RAMpal[252].g = 28; RAMpal[252].b = 42;
	RAMpal[253].r = 28; RAMpal[253].g = 35; RAMpal[253].b = 48;
	RAMpal[254].r = 34; RAMpal[254].g = 42; RAMpal[254].b = 53;
	RAMpal[255].r = 41; RAMpal[255].g = 49; RAMpal[255].b = 59;
	//}
	
	load_colorset(gui_colorset);
	
	set_palette(RAMpal);
	clear_to_color(screen,vc(0));
}

void fps_callback()
{
	lastfps=framecnt;
	framecnt=0;
}
END_OF_FUNCTION(fps_callback)

void myvsync_callback()
{
    ++myvsync;
}
END_OF_FUNCTION(myvsync_callback)

bool update_hw_pal = false;
void update_hw_screen(bool force)
{
	if(force || myvsync)
	{
		if(update_hw_pal)
		{
			set_palette(RAMpal);
			load_mouse();
		}
		update_hw_pal=false;
		myvsync=0;
		all_mark_screen_dirty();
	}
}

bool getname_nogo(const char *prompt,const char *ext,EXT_LIST *list,const char *def,bool usefilename)
{
    if(def!=temppath)
        strcpy(temppath,def);
        
    if(!usefilename)
    {
        int32_t i=(int32_t)strlen(temppath);
        
        while(i>=0 && temppath[i]!='\\' && temppath[i]!='/')
            temppath[i--]=0;
    }
    
    //  int32_t ret = file_select_ex(prompt,temppath,ext,255,-1,-1);
    int32_t ret=0;
    int32_t sel=0;
    
    if(list==NULL)
    {
        ret = jwin_file_select_ex(prompt,temppath,ext,2048,-1,-1,lfont);
    }
    else
    {
        ret = jwin_file_browse_ex(prompt, temppath, list, &sel, 2048, -1, -1, lfont);
    }
    
    return ret!=0;
}

bool getname(const char *prompt,const char *ext,EXT_LIST *list,const char *def,bool usefilename)
{
    go();
    int32_t ret=0;
    ret = getname_nogo(prompt,ext,list,def,usefilename);
    comeback();
    return ret != 0;
}


