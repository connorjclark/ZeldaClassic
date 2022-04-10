//--------------------------------------------------------
//  Zelda Classic
//  by Jeremy Craner, 1999-2000
//
//  zelda.cc
//
//  Main code for Zelda Classic. Originally written in
//  SPHINX C--, now rewritten in DJGPP with Allegro.
//
//--------------------------------------------------------

#include "precompiled.h" //always first

#include <memory>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
//#include <sdl/sdl.h>
#include <string>
#include <map>
#include <vector>

#include "zc_alleg.h"

#include <png.h>
#include <pngconf.h>
#include <stdlib.h>

#include <loadpng.h>
#include <jpgalleg.h>

#include "metadata/sigs/devsig.h.sig"

#include "zc_malloc.h"
#include "mem_debug.h"
#include "zscriptversion.h"
#include "zcmusic.h"
#include "zdefs.h"
#include "metadata/versionsig.h"
#include "zelda.h"
#include "tiles.h"
#include "colors.h"
#include "pal.h"
#include "aglogo.h"
#include "zsys.h"
#include "qst.h"
#include "matrix.h"
#include "jwin.h"
#include "jwinfsel.h"
#include "save_gif.h"
#include "load_gif.h" // not really needed; we're just saving GIF files in ZC.
#include "fontsdat.h"
#include "particles.h"
#include "gamedata.h"
#include "ffscript.h"
#include "ffasm.h"
#include "qst.h"
#include "util.h"
#include "drawing.h"
#include "dialog/info.h"
using namespace util;
extern FFScript FFCore; //the core script engine.
extern byte epilepsyFlashReduction;
#include "ConsoleLogger.h"
#ifndef _WIN32 //Unix
	#include <fcntl.h>
	#include <unistd.h>
	#include <iostream>
	#include <sstream>
	int32_t pt = 0;
	char* ptname = NULL;
	std::ostringstream lxconsole_oss;
#endif
extern ZModule zcm; //modules
extern zcmodule moduledata;
extern byte itemscriptInitialised[256];
extern char runningItemScripts[256];
extern char modulepath[2048];


extern byte dmapscriptInitialised;

extern char zc_builddate[80];
extern char zc_aboutstr[80];

int32_t DMapEditorLastMaptileUsed = 0;
int32_t switch_type = 0; //Init here to avoid Linux building error in g++.
bool saved = true;
bool zqtesting_mode = false;
static char testingqst_name[512] = {0};
static uint16_t testingqst_dmap = 0;
static uint8_t testingqst_screen = 0;
static uint8_t testingqst_retsqr = 0;

extern CConsoleLoggerEx zscript_coloured_console;
extern CConsoleLoggerEx coloured_console;

#include "init.h"
#include <assert.h>
#include "zc_array.h"
#include "rendertarget.h"
#include "zconsole.h"
#include "win32.h"
#include "vectorset.h"
#include "single_instance.h"
#include "zeldadat.h"

#define LOGGAMELOOP 0

#ifdef _MSC_VER
#include <crtdbg.h>
#define stricmp _stricmp
#define getcwd _getcwd
#endif

// MSVC fix
#if _MSC_VER >= 1900
FILE _iob[] = { *stdin, *stdout, *stderr };
extern "C" FILE * __cdecl __iob_func(void) { return _iob; }
#endif

#if DEVLEVEL > 0
bool dev_logging = true;
bool dev_debug = true;
bool dev_timestmp = true;
#endif

ZCMUSIC *zcmusic = NULL;
zinitdata zinit;
int32_t colordepth;
int32_t db=0;
//zinitdata  zinit;
int32_t detail_int[10];                                         //temporary holder for things you want to detail
int32_t lens_hint_item[MAXITEMS][2]= {{0,0},{0,0}};                            //aclk, aframe
int32_t lens_hint_weapon[MAXWPNS][5] = {{0,0},{0,0}};                           //aclk, aframe, dir, x, y
int32_t cheat_modifier_keys[4]; //two options each, default either control and either shift
int32_t strike_hint_counter=0;
uint8_t __isZQuest = 0; //shared functions can use this. -
int32_t strike_hint_timer=0;
int32_t strike_hint = 0;
int32_t slot_arg = 0, slot_arg2 = 0;
char save_file_name[1024] = "zc.sav";
//char *SAVE_FILE = (char *)"zc.sav";
char *SAVE_FILE = NULL;
int32_t previous_DMap = -1;
CScriptDrawingCommands script_drawing_commands;

using std::string;
using std::pair;

int32_t zq_screen_w = 0, zq_screen_h = 0;
int32_t passive_subscreen_height=56;
int32_t original_playing_field_offset=56;
int32_t playing_field_offset=original_playing_field_offset;
int32_t passive_subscreen_offset=0;
extern int32_t directItem;
extern int32_t directItemA;
extern int32_t directItemB;
extern int32_t directItemY;
extern int32_t directItemX;
extern int32_t hangcount;

extern bool kb_typing_mode;

bool is_large=false;

bool standalone_mode=false;
char *standalone_quest=NULL;
bool skip_title=false;

int32_t favorite_combos[MAXFAVORITECOMBOS] = {0};
int32_t favorite_comboaliases[MAXFAVORITECOMBOALIASES]= {0};

void playLevelMusic();

//Prevent restarting during ending sequence from creating a rect clip
int32_t draw_screen_clip_rect_x1=0;
int32_t draw_screen_clip_rect_x2=255;
int32_t draw_screen_clip_rect_y1=0;
int32_t draw_screen_clip_rect_y2=223;

extern int32_t script_hero_sprite;
extern int32_t script_hero_cset;
extern int32_t script_hero_flip;

volatile int32_t logic_counter=0;
bool trip=false;
extern byte midi_suspended;
extern byte callback_switchin;
extern bool midi_paused;
extern int32_t paused_midi_pos;
extern byte midi_patch_fix;
void update_logic_counter()
{
    ++logic_counter;
}
END_OF_FUNCTION(update_logic_counter)

#ifdef _SCRIPT_COUNTER
volatile int32_t script_counter=0;
void update_script_counter()
{
    ++script_counter;
}
END_OF_FUNCTION(update_script_counter)
#endif

void throttleFPS()
{
#ifdef _WIN32           // TEMPORARY!! -Trying to narrow down a win10 bug that affects performance.
    timeBeginPeriod(1); // Basically, jist is that other programs can affect the FPS of ZC in weird ways. (making it better for example... go figure)
#endif
    
    if( (Throttlefps ^ (zc_getkey(KEY_TILDE)!=0)) || get_bit(quest_rules, qr_NOFASTMODE) )
    {
        if(zc_vsync == FALSE)
        {
			int32_t t = 0;
            while(logic_counter < 1)
			{
				// bugfix: win xp/7/8 have incompatible timers.
				// preserve 60 fps and CPU based on user settings. -Gleeok
				int32_t ms = t >= 16 ? 0 : frame_rest_suggest;
                rest(ms);
				t += frame_rest_suggest;
			}
        }
        else
        {
            vsync();
        }
	
    }
#ifdef _WIN32
    timeEndPeriod(1);
#endif

    logic_counter = 0;
}

int32_t onHelp()
{
    //  restore_mouse();
    //  doHelp(vc(15),vc(0));
    return D_O_K;
}
int32_t d_dropdmaplist_proc(int32_t ,DIALOG *,int32_t)
{
    return D_O_K;
}

static char dmap_str_buf[37];
int32_t dmap_list_size=MAXDMAPS;
bool dmap_list_zero=true;

const char *dmaplist(int32_t index, int32_t *list_size)
{
    if(index>=0)
    {
        bound(index,0,dmap_list_size-1);
        sprintf(dmap_str_buf,"%3d-%s",index+(dmap_list_zero?0:1), DMaps[index].name);
        return dmap_str_buf;
    }
    
    *list_size=dmap_list_size;
    return NULL;
}

int32_t startdmapxy[6] = {0,0,0,0,0,0};

/**********************************/
/******** Global Variables ********/
/**********************************/

int32_t curr_tb_page=0;
bool triplebuffer_not_available=false;

RGB_MAP rgb_table;
COLOR_MAP trans_table, trans_table2;

BITMAP     *framebuf, *scrollbuf, *tmp_bmp, *tmp_scr, *screen2, *fps_undo,
           *msg_portrait_display_buf, *msg_txt_display_buf, *msg_bg_display_buf,
		   *pricesdisplaybuf, *tb_page[3], *temp_buf, *prim_bmp,
		   *script_menu_buf, *f6_menu_buf, *hw_screen, *scrtmp;
BITMAP     *zcmouse[4];
DATAFILE   *data, *sfxdata, *fontsdata, *mididata;
PALETTE    RAMpal;
byte       *colordata, *trashbuf;
//byte       *tilebuf;
itemdata   *itemsbuf;
wpndata    *wpnsbuf;
comboclass *combo_class_buf;
guydata    *guysbuf;
item_drop_object    item_drop_sets[MAXITEMDROPSETS];
ZCHEATS    zcheats;
byte       use_cheats;
byte       use_tiles;
char       palnames[MAXLEVELS][PALNAMESIZE];
/*
tiledata   *newtilebuf, *grabtilebuf;
newcombo   *combobuf;
word animated_combo_table[MAXCOMBOS][2];                    //[0]=position in act2, [1]=original tile
word animated_combo_table4[MAXCOMBOS][2];                   //[0]=combo, [1]=clock
word animated_combos;
word animated_combo_table2[MAXCOMBOS][2];                    //[0]=position in act2, [1]=original tile
word animated_combo_table24[MAXCOMBOS][2];                   //[0]=combo, [1]=clock
word animated_combos2;
bool blank_tile_table[NEWMAXTILES];                         //keeps track of blank tiles
bool blank_tile_quarters_table[NEWMAXTILES*4];              //keeps track of blank tiles
*/
bool ewind_restart=false;

word     msgclk = 0, msgstr = 0, enqueued_str = 0,
         msgpos = 0,	// screen position of the next character.
         msgptr = 0,	// position within the string of the next character.
         msgcolour = 0,	// colour to use for the displayed text.
         msgspeed = 0,	// delay between each character (5 = default).
         msg_w = 0,
         msg_h = 0,
         msg_count = 0,
         cursor_x = 0,
         cursor_y = 0,
         msg_xpos=0,
         msg_ypos=0,
         msgorig=0;
byte msg_margins[4] = {0};
int32_t prt_tile=0;
byte prt_cset=0, prt_x=0, prt_y=0, prt_tw=0, prt_th=0, msg_shdtype=0, msg_shdcol=0;
bool msg_onscreen = false, msg_active = false, msgspace = false;
BITMAP   *msg_txt_bmp_buf = NULL, *msg_bg_bmp_buf = NULL, *msg_portrait_bmp_buf = NULL, *msg_menu_bmp_buf = NULL;
BITMAP   *darkscr_bmp_curscr = NULL, *darkscr_bmp_scrollscr = NULL,
         *darkscr_bmp_curscr_trans = NULL, *darkscr_bmp_scrollscr_trans = NULL;
BITMAP *lightbeam_bmp = NULL;
FONT	 *msgfont;
word     door_combo_set_count;
word     introclk  = 0, intropos = 0, dmapmsgclk = 0, linkedmsgclk = 0;
int16_t    lensclk = 0;
int32_t     lensid = 0; // Lens's item id. -1 if lens is off.
int32_t    Bpos = 0;
byte screengrid[22]={0};
byte screengrid_layer[2][22]={0};
byte ffcgrid[4]={0};
bool halt=false;
bool screenscrolling=false;
bool close_button_quit=false;
PALETTE tempbombpal;
bool usebombpal = false;

int32_t readsize = 0, writesize = 0;
bool fake_pack_writing=false;
combo_alias combo_aliases[MAXCOMBOALIASES];  //Temporarily here so ZC can compile. All memory from this is freed after loading the quest file.

SAMPLE customsfxdata[WAV_COUNT] = {0};
uint8_t customsfxflag[WAV_COUNT>>3]  = {0};
int32_t sfxdat=1;
int32_t zqwin_scale = 0;

extern int32_t jwin_pal[jcMAX];
int32_t gui_colorset=0;
int32_t fullscreen = 0;
byte frame_rest_suggest=0,forceExit=0,zc_vsync=0;
byte disable_triplebuffer=0,can_triplebuffer_in_windowed_mode=0;
byte zc_color_depth=8;
byte use_debug_console=0, console_on_top = 0, use_win32_proc=1, zasm_debugger = 0, zscript_debugger = 0; //windows-build configs
int32_t homescr,currscr,frame=0,currmap=0,dlevel,warpscr,worldscr,scrolling_scr=0,scrolling_map=0;
int32_t newscr_clk=0,opendoors=0,currdmap=0,fadeclk=-1,currgame=0,listpos=0;
int32_t lastentrance=0,lastentrance_dmap=0,prices[3]= {0},loadside = 0, Bwpn = 0, Awpn = 0, Xwpn = 0, Ywpn = 0;
int32_t digi_volume = 0,midi_volume = 0,sfx_volume = 0,emusic_volume = 0,currmidi = 0,hasitem = 0,whistleclk = 0,pan_style = 0;
bool analog_movement=true;
int32_t joystick_index=0,Akey = 0,Bkey = 0,Skey = 0,Lkey = 0,Rkey = 0,Pkey = 0,Exkey1 = 0,Exkey2 = 0,Exkey3 = 0,Exkey4 = 0,Abtn = 0,Bbtn = 0,Sbtn = 0,Mbtn = 0,Lbtn = 0,Rbtn = 0,Pbtn = 0,Exbtn1 = 0,Exbtn2 = 0,Exbtn3 = 0,Exbtn4 = 0,Quit=0;
uint32_t GameFlags=0;
int32_t js_stick_1_x_stick = 0, js_stick_1_x_axis = 0, js_stick_1_x_offset = 0;
int32_t js_stick_1_y_stick = 0, js_stick_1_y_axis = 0, js_stick_1_y_offset = 0;
int32_t js_stick_2_x_stick = 0, js_stick_2_x_axis = 0, js_stick_2_x_offset = 0;
int32_t js_stick_2_y_stick = 0, js_stick_2_y_axis = 0, js_stick_2_y_offset = 0;
int32_t DUkey = 0, DDkey = 0, DLkey = 0, DRkey = 0, DUbtn = 0, DDbtn = 0, DLbtn = 0, DRbtn = 0, ss_after = 0, ss_speed = 0, ss_density = 0, ss_enable = 0;
int32_t hs_startx = 0, hs_starty = 0, hs_xdist = 0, hs_ydist = 0, clockclk = 0, clock_zoras[eMAXGUYS]={0};
int32_t cheat_goto_dmap=0, cheat_goto_screen=0, currcset = 0;
int32_t gfc = 0, gfc2 = 0, pitx = 0, pity = 0, refill_what = 0, refill_why = 0, heart_beep_timer=0, new_enemy_tile_start=1580;
int32_t nets=1580, magicitem=-1,nayruitem=-1, title_version = 0, magiccastclk = 0, quakeclk=0, wavy=0, castx = 0, casty = 0, df_x = 0, df_y = 0, nl1_x = 0, nl1_y = 0, nl2_x = 0, nl2_y = 0;
int32_t magicdrainclk=0, conveyclk=3, memrequested=0;
float avgfps=0;
dword fps_secs=0;
bool do_cheat_goto=false, do_cheat_light=false;
int32_t checkx = 0, checky = 0;
int32_t loadlast=0;
int32_t skipcont=0;
int32_t skipicon=0;

bool monochrome = false; //GFX are monochrome.
bool palette_user_tinted = false;

bool show_layer_0=true, show_layer_1=true, show_layer_2=true, show_layer_3=true, show_layer_4=true, show_layer_5=true, show_layer_6=true,
//oveheard combos     //pushblocks
     show_layer_over=true, show_layer_push=true, show_sprites=true, show_ffcs=true, show_hitboxes=false, show_walkflags=false, show_ff_scripts=false, show_effectflags = false;


bool Throttlefps = true, MenuOpen = false, ClickToFreeze=false, Paused=false, Advance=false, ShowFPS = true, Showpal=false, disableClickToFreeze=false;
bool Playing, FrameSkip=false, TransLayers = true;
bool __debug=false,debug_enabled = false;
bool refreshpal,blockpath = false,loaded_guys= false,freeze_guys= false,
     loaded_enemies= false,drawguys= false,details=false,watch= false;
bool darkroom=false,naturaldark=false,BSZ= false;                         //,NEWSUBSCR;
bool Udown= false,Ddown= false,Ldown= false,Rdown= false,Adown= false,Bdown= false,Sdown= false,Mdown= false,LBdown= false,RBdown= false,Pdown= false,Ex1down= false,Ex2down= false,Ex3down= false,Ex4down= false,AUdown= false,ADdown= false,ALdown= false,ARdown= false,F12= false,F11= false, F5= false,keyI= false, keyQ= false,
     SystemKeys=true,NESquit= false,volkeys= false,useCD=false,boughtsomething=false,
     fixed_door=false, hookshot_used=false, hookshot_frozen=false,
     pull_hero=false, hs_fix=false, hs_switcher=false,
     cheat_superman=false, gofast=false, checkhero=true, didpit=false, heart_beep=true,
     pausenow=false, castnext=false, add_df1asparkle= false, add_df1bsparkle= false, add_nl1asparkle= false, add_nl1bsparkle= false, add_nl2asparkle= false, add_nl2bsparkle= false,
     is_on_conveyor= false, activated_timed_warp=false;
int32_t hooked_combopos = -1, switchhook_cost_item = -1;
uint16_t hooked_layerbits = 0;
int32_t hooked_undercombos[14] = {0};
sprite* switching_object = NULL;

byte COOLSCROLL = 0;

int32_t  add_asparkle=0, add_bsparkle=0;
int32_t SnapshotFormat, NameEntryMode=0;

char   zeldadat_sig[52]={0};
char   sfxdat_sig[52]={0};
char   fontsdat_sig[52]={0};
char   cheat_goto_dmap_str[4]={0};
char   cheat_goto_screen_str[3]={0};
int16_t  visited[6]={0};
byte   guygrid[176]={0};
mapscr tmpscr[2];
mapscr tmpscr2[6];
mapscr tmpscr3[6];
gamedata *game=NULL;
script_data *ffscripts[NUMSCRIPTFFC];
script_data *itemscripts[NUMSCRIPTITEM];
script_data *globalscripts[NUMSCRIPTGLOBAL];
script_data *guyscripts[NUMSCRIPTGUYS];
script_data *wpnscripts[NUMSCRIPTWEAPONS];
script_data *lwpnscripts[NUMSCRIPTWEAPONS];
script_data *ewpnscripts[NUMSCRIPTWEAPONS];
script_data *playerscripts[NUMSCRIPTPLAYER];
script_data *screenscripts[NUMSCRIPTSCREEN];
script_data *dmapscripts[NUMSCRIPTSDMAP];
script_data *itemspritescripts[NUMSCRIPTSITEMSPRITE];
script_data *comboscripts[NUMSCRIPTSCOMBODATA];

extern refInfo globalScriptData[NUMSCRIPTGLOBAL];
extern refInfo playerScriptData;
extern refInfo screenScriptData;
extern refInfo dmapScriptData;
extern word g_doscript;
extern word player_doscript;
extern word dmap_doscript;
extern word passive_subscreen_doscript;
extern word global_wait;
extern bool player_waitdraw;
extern bool dmap_waitdraw;
extern bool passive_subscreen_waitdraw;

ScriptOwner::ScriptOwner() : scriptType(SCRIPT_NONE), ownerUID(0) {}
void ScriptOwner::clear()
{
	scriptType = SCRIPT_NONE;
	ownerUID = 0;
}

//ZScript array storage
std::vector<ZScriptArray> globalRAM;
ZScriptArray localRAM[NUM_ZSCRIPT_ARRAYS];
ScriptOwner arrayOwner[NUM_ZSCRIPT_ARRAYS];

//script bitmap drawing
ZScriptDrawingRenderTarget* zscriptDrawingRenderTarget;

DebugConsole DebugConsole::singleton = DebugConsole();
ZASMSTackTrace ZASMSTackTrace::singleton = ZASMSTackTrace();


void setZScriptVersion(int32_t s_version)
{
    ZScriptVersion::setVersion(s_version);
}

void initZScriptArrayRAM(bool firstplay)
{
    for(word i = 0; i < NUM_ZSCRIPT_ARRAYS; i++)
    {
        localRAM[i].Clear();
        arrayOwner[i].clear();
    }
    
    if(game->globalRAM.size() != 0)
        game->globalRAM.clear();
        
    if(firstplay)
    {
        //leave to global script ~Init to allocate global memory first time round
        game->globalRAM.resize(getNumGlobalArrays());
    }
    else
    {
        //allocate from save file
        game->globalRAM.resize(saves[currgame].globalRAM.size());
        
        for(dword i = 0; i < game->globalRAM.size(); i++)
        {
#ifdef _DEBUGARRAYALLOC
            al_trace("Global Array: %i\n",i);
#endif
            ZScriptArray &from = saves[currgame].globalRAM[i];
            ZScriptArray &to = game->globalRAM[i];
            to.Resize(from.Size());
            
            for(dword j = 0; j < from.Size(); j++)
            {
#ifdef _DEBUGARRAYALLOC
                al_trace("Element: %i\nInit: %i, From save file: %i\n", j, to[j], from[j]);
#endif
                to[j] = from[j];
            }
        }
    }
}

void initZScriptGlobalRAM()
{
	g_doscript = 0xFFFF; //Set all scripts active
	global_wait = 0; //Clear waitdraws
	for(int32_t q = 0; q < NUMSCRIPTGLOBAL; ++q)
	{
		globalScriptData[q].Clear();
		clear_global_stack(q);
	}
}

void initZScriptGlobalScript(int32_t ID)
{
	g_doscript |= (1<<ID);
	global_wait &= ~(1<<ID);
	globalScriptData[ID].Clear();
	clear_global_stack(ID);
}

dword getNumGlobalArrays()
{
    word scommand, pc = 0, ret = 0;
    
    do
    {
        scommand = globalscripts[GLOBAL_SCRIPT_INIT]->zasm[pc].command;
        
        if(scommand == ALLOCATEGMEMV || scommand == ALLOCATEGMEMR)
            ret++;
            
        pc++;
    }
    while(scommand != 0xFFFF);
    
    return ret;
}

//movingblock mblock2; //mblock[4]?
//HeroClass   Hero;

int32_t resx= 0,resy= 0,scrx= 0,scry= 0;
bool sbig=false;                                                  // big screen
bool sbig2=false;													// bigger screen
int32_t screen_scale = 2; //default = 2 (640x480)
bool scanlines=false; 
extern byte pause_in_background;
extern signed char pause_in_background_menu_init;//do scanlines if sbig==1
bool toogam=false;
bool ignoreSideview=false;

int32_t cheat = (DEVLEVEL > 1) ? 4 : 0;                         // 0 = none; 1,2,3,4 = cheat level
int32_t maxcheat = (DEVLEVEL > 1) ? 4 : 0;

int32_t mouse_down=0;                                             // used to hold the last reading of 'gui_mouse_b()' status
int32_t idle_count=0, active_count=0;


// quest file data
zquestheader QHeader;
byte                quest_rules[QUESTRULES_NEW_SIZE];
byte                extra_rules[EXTRARULES_SIZE];
byte                midi_flags[MIDIFLAGS_SIZE];
byte                music_flags[MUSICFLAGS_SIZE];
word                map_count=0;
MsgStr              *MsgStrings;
int32_t					msg_strings_size=0;
DoorComboSet        *DoorComboSets;
dmap                *DMaps;
miscQdata           QMisc;
std::vector<mapscr> TheMaps;
zcmap               *ZCMaps;
byte                *quest_file;
dword               quest_map_pos[MAPSCRS*MAXMAPS2]={0};

char     *qstpath=NULL;
char     *qstdir=NULL;
gamedata *saves=NULL;

volatile int32_t lastfps=0;
volatile int32_t framecnt=0;
volatile int32_t myvsync=0;

bool update_hw_pal = false;
PALETTE* hw_palette = NULL;
void update_hw_screen(bool force)
{
	//if(!hw_screen) return;
	if(force || (!is_sys_pal && !Throttlefps) || myvsync)
	{
		blit(screen, hw_screen, 0, 0, 0, 0, screen->w, screen->h);
		if(update_hw_pal && hw_palette)
		{
			set_palette(*hw_palette);
			update_hw_pal = false;
		}
		myvsync=0;
	}
}

void myvsync_callback()
{
    ++myvsync;
}
END_OF_FUNCTION(myvsync_callback)

/*
enum { 	SAVESC_BACKGROUND, 		SAVESC_TEXT, 			SAVESC_USETILE, 	
	SAVESC_CURSOR_CSET, 		SAVESC_CUR_SOUND,  		SAVESC_TEXT_CONTINUE_COLOUR, 
	SAVESC_TEXT_SAVE_COLOUR, 	SAVESC_TEXT_RETRY_COLOUR, 	SAVESC_TEXT_CONTINUE_FLASH, 
	SAVESC_TEXT_SAVE_FLASH, 	SAVESC_TEXT_RETRY_FLASH,	SAVESC_MIDI,
	SAVESC_CUR_FLIP, 		    SAVESC_TEXT_DONTSAVE_COLOUR, 	SAVESC_TEXT_SAVEQUIT_COLOUR, 
	SAVESC_TEXT_SAVE2_COLOUR, 	SAVESC_TEXT_QUIT_COLOUR, 	SAVESC_TEXT_DONTSAVE_FLASH,
	SAVESC_TEXT_SAVEQUIT_FLASH,	SAVESC_TEXT_SAVE2_FLASH, 	SAVESC_TEXT_QUIT_FLASH,
	SAVESC_EXTRA1, 			SAVESC_EXTRA2,			SAVESC_EXTRA3,			
	SAVESC_LAST	}; */
#define SAVESC_DEF_TILE 2
int32_t SaveScreenSettings[24] = {
	BLACK, 			WHITE, 			SAVESC_DEF_TILE, 
	1, 			WAV_CHINK,	 	QMisc.colors.msgtext,
	QMisc.colors.msgtext,	QMisc.colors.msgtext,	QMisc.colors.caption,
	QMisc.colors.caption,	QMisc.colors.caption,	-4,
	0, 			QMisc.colors.msgtext, 	QMisc.colors.msgtext, 
	QMisc.colors.msgtext, 	QMisc.colors.msgtext,  	QMisc.colors.caption,
	QMisc.colors.caption,	QMisc.colors.caption,	QMisc.colors.caption,
	0, 			0, 			0 }; //BG, Text, Cursor CSet, Sound, UseTile, Misc
char SaveScreenText[7][32]={"CONTINUE", "SAVE", "RETRY", "DON'T SAVE", "SAVE AND QUIT", "SAVE", "QUIT" };

void ResetSaveScreenSettings()
{
	SaveScreenSettings[SAVESC_TEXT_CONTINUE_COLOUR] = QMisc.colors.msgtext; 
	SaveScreenSettings[SAVESC_TEXT_SAVE_COLOUR] = QMisc.colors.msgtext; 
	SaveScreenSettings[SAVESC_TEXT_RETRY_COLOUR] = QMisc.colors.msgtext; 
	SaveScreenSettings[SAVESC_TEXT_CONTINUE_FLASH] = QMisc.colors.caption; 
	SaveScreenSettings[SAVESC_TEXT_SAVE_FLASH] = QMisc.colors.caption; 
	SaveScreenSettings[SAVESC_TEXT_RETRY_FLASH] = QMisc.colors.caption;
	SaveScreenSettings[SAVESC_MIDI] = -4;
	//SaveScreenSettings[SAVESC_BACKGROUND] = BLACK;
	SaveScreenSettings[SAVESC_BACKGROUND] = 0; //Isle of Rebirth changed the game over background by changing color 0 of the palette; this needs to be respected!
	SaveScreenSettings[SAVESC_TEXT] = QMisc.colors.msgtext;
	SaveScreenSettings[SAVESC_USETILE] = SAVESC_DEF_TILE;
	SaveScreenSettings[SAVESC_CURSOR_CSET] = 1;
	SaveScreenSettings[SAVESC_CUR_SOUND] =  WAV_CHINK;
	SaveScreenSettings[SAVESC_CUR_FLIP] = 0;
	SaveScreenSettings[SAVESC_TEXT_DONTSAVE_COLOUR] = QMisc.colors.msgtext;
	SaveScreenSettings[SAVESC_TEXT_SAVEQUIT_COLOUR] = QMisc.colors.msgtext;
	SaveScreenSettings[SAVESC_TEXT_SAVE2_COLOUR] = QMisc.colors.msgtext;
	SaveScreenSettings[SAVESC_TEXT_QUIT_COLOUR] = QMisc.colors.msgtext;
	SaveScreenSettings[SAVESC_TEXT_DONTSAVE_FLASH] = QMisc.colors.caption;
	SaveScreenSettings[SAVESC_TEXT_SAVEQUIT_FLASH] = QMisc.colors.caption;
	SaveScreenSettings[SAVESC_TEXT_SAVE2_FLASH] = QMisc.colors.caption;
	SaveScreenSettings[SAVESC_TEXT_QUIT_FLASH] = QMisc.colors.caption;
	SaveScreenSettings[SAVESC_EXTRA1] = 0;
	SaveScreenSettings[SAVESC_EXTRA2] = 0;
	SaveScreenSettings[SAVESC_EXTRA3] = 0;
}

void SetSaveScreenSetting(int32_t indx, int32_t value)
{
	switch(indx)
	{
		case SAVESC_BACKGROUND:
			SaveScreenSettings[SAVESC_BACKGROUND] = vbound(value,0,255);
			break;
		case SAVESC_TEXT:
			SaveScreenSettings[SAVESC_TEXT] = vbound(value,0,255);
			break;
		case SAVESC_USETILE:
			SaveScreenSettings[SAVESC_USETILE] = vbound(value,0,NEWMAXTILES);
			break; 
		case SAVESC_CURSOR_CSET:
			SaveScreenSettings[SAVESC_CURSOR_CSET] = vbound(value,0,14);
			break;
		case SAVESC_CUR_SOUND:
			SaveScreenSettings[SAVESC_CUR_SOUND] = vbound(value,0,255); //MAX_SOUNDS
			break;
		case SAVESC_TEXT_CONTINUE_COLOUR:
			SaveScreenSettings[SAVESC_TEXT_CONTINUE_COLOUR] = vbound(value,0,255);
			break;
		case SAVESC_TEXT_SAVE_COLOUR:
			SaveScreenSettings[SAVESC_TEXT_SAVE_COLOUR] = vbound(value,0,255);
			break;
		case SAVESC_TEXT_RETRY_COLOUR:
			SaveScreenSettings[SAVESC_TEXT_RETRY_COLOUR] = vbound(value,0,255);
			break;
		case SAVESC_TEXT_CONTINUE_FLASH:
			SaveScreenSettings[SAVESC_TEXT_CONTINUE_FLASH] = vbound(value,0,255);
			break;
		case SAVESC_TEXT_SAVE_FLASH:
			SaveScreenSettings[SAVESC_TEXT_SAVE_FLASH] = vbound(value,0,255);
			break;
		case SAVESC_TEXT_RETRY_FLASH:
			SaveScreenSettings[SAVESC_TEXT_RETRY_FLASH] = vbound(value,0,255);
			break;
		case SAVESC_MIDI:
			SaveScreenSettings[SAVESC_MIDI] = vbound(value,0,255);
			break;
		case SAVESC_CUR_FLIP:
			SaveScreenSettings[SAVESC_CUR_FLIP] = vbound(value,0,3);
			break;
		case SAVESC_TEXT_DONTSAVE_COLOUR:
			SaveScreenSettings[SAVESC_TEXT_DONTSAVE_COLOUR] = vbound(value,0,255);
			break;
		case SAVESC_TEXT_SAVEQUIT_COLOUR:
			SaveScreenSettings[SAVESC_TEXT_SAVEQUIT_COLOUR] = vbound(value,0,255);
			break;
		case SAVESC_TEXT_SAVE2_COLOUR:
			SaveScreenSettings[SAVESC_TEXT_SAVE2_COLOUR] = vbound(value,0,255);
			break;
		case SAVESC_TEXT_QUIT_COLOUR:
			SaveScreenSettings[SAVESC_TEXT_QUIT_COLOUR] = vbound(value,0,255);
			break;
		case SAVESC_TEXT_DONTSAVE_FLASH:
			SaveScreenSettings[SAVESC_TEXT_DONTSAVE_FLASH] = vbound(value,0,255);
			break;
		case SAVESC_TEXT_SAVEQUIT_FLASH:
			SaveScreenSettings[SAVESC_TEXT_SAVEQUIT_FLASH] = vbound(value,0,255);
			break;
		case SAVESC_TEXT_SAVE2_FLASH:
			SaveScreenSettings[SAVESC_TEXT_SAVE2_FLASH] = vbound(value,0,255);
			break;
		case SAVESC_TEXT_QUIT_FLASH:
			SaveScreenSettings[SAVESC_TEXT_QUIT_FLASH] = vbound(value,0,255);
			break;
		default: break;
	}
}


void ChangeSubscreenText(int32_t index, const char *f)
{
	index = vbound(index, 0, SAVESC_END-1);
	strncpy(SaveScreenText[index], f, 31);
	SaveScreenText[index][32]='\0';
}



/**********************************/
/*********** Misc Data ************/
/**********************************/

const char startguy[8] = {-13,-13,-13,-14,-15,-18,-21,-40};
const char gambledat[12*6] =
{
    20,-10,-10, 20,-10,-10, 20,-40,-10, 20,-10,-40,
    50,-10,-10, 50,-10,-10, 50,-40,-10, 50,-10,-40,
    -10,20,-10, -10,20,-10, -40,20,-10, -10,20,-40,
    -10,50,-10, -10,50,-10, -40,50,-10, -10,50,-40,
    -10,-10,20, -10,-10,20, -10,-40,20, -40,-10,20,
    -10,-10,50, -10,-10,50, -10,-40,50, -40,-10,50
};
const byte stx[4][9] =
{
    { 48, 80, 80, 96,112,144,160,160,192},
    { 48, 80, 80, 96,128,144,160,160,192},
    { 80, 80,128,128,160,160,192,192,208},
    { 32, 48, 48, 80, 80,112,112,160,160}
};
const byte sty[4][9] =
{
    {112, 64,128, 96, 80, 96, 64,128,112},
    { 48, 32, 96, 64, 80, 64, 32, 96, 48},
    { 32,128, 64, 96, 64, 96, 48,112, 80},
    { 80, 48,112, 64, 96, 64, 96, 32,128}
};

const byte ten_rupies_x[10] = {120,112,128,96,112,128,144,112,128,120};
const byte ten_rupies_y[10] = {49,65,65,81,81,81,81,97,97,113};

zctune tunes[MAXMIDIS] =
{
    // (title)                              (s) (ls) (le) (l) (vol) (midi)(fmt)
    zctune((char *)"Zelda - Dungeon",     0,  -1,  -1,  1,  176,  NULL, 0),
    zctune((char *)"Zelda - Ending",      0, 129, 225,  1,  160,  NULL, 0),
    zctune((char *)"Zelda - Game Over",   0,  -1,  -1,  1,  224,  NULL, 0),
    zctune((char *)"Zelda - Level 9",     0,  -1,  -1,  1,  255,  NULL, 0),
    zctune((char *)"Zelda - Overworld",   0,  17,  -1,  1,  208,  NULL, 0),
    zctune((char *)"Zelda - Title",       0,  -1,  -1,  0,  168,  NULL, 0),
    zctune((char *)"Zelda - Triforce",    0,  -1,  -1,  0,  168,  NULL, 0)
};

// emusic enhancedMusic[MAXMUSIC];

FONT *setmsgfont()
{
    switch(MsgStrings[msgstr].font)
    {
    default:
        return zfont;
        
    case font_z3font:
        return z3font;
        
    case font_z3smallfont:
        return z3smallfont;
        
    case font_deffont:
        return deffont;
        
    case font_lfont:
        return lfont;
        
    case font_lfont_l:
        return lfont_l;
        
    case font_pfont:
        return pfont;
        
    case font_mfont:
        return mfont;
        
    case font_ztfont:
        return ztfont;
        
    case font_sfont:
        return sfont;
        
    case font_sfont2:
        return sfont2;
        
    case font_spfont:
        return spfont;
        
    case font_ssfont1:
        return ssfont1;
        
    case font_ssfont2:
        return ssfont2;
        
    case font_ssfont3:
        return ssfont3;
        
    case font_ssfont4:
        return ssfont4;
        
    case font_gblafont:
        return gblafont;
        
    case font_goronfont:
        return goronfont;
        
    case font_zoranfont:
        return zoranfont;
        
    case font_hylian1font:
        return hylian1font;
        
    case font_hylian2font:
        return hylian2font;
        
    case font_hylian3font:
        return hylian3font;
        
    case font_hylian4font:
        return hylian4font;
        
    case font_gboraclefont:
        return gboraclefont;
        
    case font_gboraclepfont:
        return gboraclepfont;
        
    case font_dsphantomfont:
        return dsphantomfont;
        
    case font_dsphantompfont:
        return dsphantompfont;
    
     case font_atari800font: return atari800font;
		 case font_acornfont: return acornfont;
		 case font_adosfont: return adosfont;
		 case font_baseallegrofont: return  baseallegrofont;  
		 case font_apple2font: return apple2font;
		 case font_apple280colfont: return apple280colfont;   
		 case font_apple2gsfont: return  apple2gsfont;
		 case font_aquariusfont: return  aquariusfont;  
		 case font_atari400font: return  atari400font;  
		 case font_c64font: return c64font;   
		 case font_c64hiresfont: return c64hiresfont;   
		 case font_cgafont: return cgafont;   
		 case font_cocofont: return cocofont;
		 case font_coco2font: return coco2font;
		 case font_coupefon: return  coupefont;
		 case font_cpcfon: return  cpcfont;
		 case font_fantasyfon: return  fantasyfont;
		 case font_fdskanafon: return  fdskanafont;
		 case font_fdslikefon: return  fdslikefont;
		 case font_fdsromanfon: return fdsromanfont; 
		 case font_finalffont: return finalffont; 
		 case font_futharkfont: return  futharkfont;
		 case font_gaiafont: return gaiafont; 
		 case font_hirafont: return hirafont; 
		 case font_jpfont: return jpfont; 
		 case font_kongfont: return  kongfont;
		 case font_manafont: return manafont; 
		 case font_mlfont: return  mlfont;
		 case font_motfont: return motfont;
		 case font_msxmode0font: return  msxmode0font;
		 case font_msxmode1font: return  msxmode1font;
		 case font_petfont: return  petfont;
		 case font_pstartfont: return  pstartfont;
		 case font_saturnfont: return  saturnfont;
		 case font_scififont: return  scififont;
		 case font_sherwoodfont: return sherwoodfont;
		 case font_sinqlfont: return  sinqlfont;
		 case font_spectrumfont: return  spectrumfont;
		 case font_speclgfont: return  speclgfont;
		 case font_ti99font: return  ti99font;
		 case font_trsfont: return  trsfont;
		 case font_z2font: return  z2font;
		 case font_zxfont: return zxfont;
		 case font_lisafont: return lisafont;
    }
}

void zc_trans_blit(BITMAP* dest, BITMAP* src, int32_t sx, int32_t sy, int32_t dx, int32_t dy, int32_t w, int32_t h)
{
	for(int32_t tx = 0; tx < w; ++tx)
		for(int32_t ty = 0; ty < h; ++ty)
		{
			int32_t c1 = src->line[sy+ty][sx+tx];
			int32_t c2 = dest->line[dy+ty][dx+tx];
			if(c1)
			{
				dest->line[dy+ty][dx+tx] = trans_table.data[c1][c2];
			}
		}
}
void msg_bg(MsgStr const& msg)
{
	if(msg.tile == 0) return;
	if(msg.stringflags & STRINGFLAG_FULLTILE)
	{
		draw_block_flip(msg_bg_bmp_buf,0,0,msg.tile,msg.cset,
			(int32_t)ceil(msg.w/16.0),(int32_t)ceil(msg.h/16.0),0,true,false);
	}
	else
	{
		int32_t add = (get_bit(quest_rules,qr_STRING_FRAME_OLD_WIDTH_HEIGHT)!=0 ? 2 : 0);
		frame2x2(msg_bg_bmp_buf,&QMisc,0,0,msg.tile,msg.cset,
                 (msg.w>>3)+add,(msg.h>>3)+add,0,true,0);
	}
}
void msg_prt()
{
	clear_bitmap(msg_portrait_bmp_buf);
	if(prt_tile > 0 && prt_th > 0 && prt_tw > 0)
	{
		draw_block_flip(msg_portrait_bmp_buf,0,0,prt_tile,prt_cset,
			prt_tw, prt_th, 0, true, false);
	}
}
void blit_msgstr_bg(BITMAP* dest, int32_t sx, int32_t sy, int32_t dx, int32_t dy, int32_t w, int32_t h)
{
	if(MsgStrings[msgstr].stringflags & STRINGFLAG_TRANS_BG)
	{
		BITMAP* subbmp = create_bitmap_ex(8,w,h);
		if(subbmp)
		{
			color_map = &trans_table2;
			clear_bitmap(subbmp);
			masked_blit(msg_bg_display_buf, subbmp, sx, sy, 0, 0, w, h);
			draw_trans_sprite(dest, subbmp, dx, dy);
			destroy_bitmap(subbmp);
			color_map = &trans_table;
		}
		//zc_trans_blit(dest, msg_bg_display_buf, sx, sy, dx, dy, w, h);
	}
	else
	{
		masked_blit(msg_bg_display_buf, dest, sx, sy, dx, dy, w, h);
	}
}
void blit_msgstr_fg(BITMAP* dest, int32_t sx, int32_t sy, int32_t dx, int32_t dy, int32_t w, int32_t h)
{
	if(MsgStrings[msgstr].stringflags & STRINGFLAG_TRANS_FG)
	{
		BITMAP* subbmp = create_bitmap_ex(8,w,h);
		if(subbmp)
		{
			color_map = &trans_table2;
			clear_bitmap(subbmp);
			masked_blit(msg_txt_display_buf, subbmp, sx, sy, 0, 0, w, h);
			draw_trans_sprite(dest, subbmp, dx, dy);
			destroy_bitmap(subbmp);
			color_map = &trans_table;
		}
		//zc_trans_blit(dest, msg_txt_display_buf, sx, sy, dx, dy, w, h);
	}
	else
	{
		masked_blit(msg_txt_display_buf, dest, sx, sy, dx, dy, w, h);
	}
}
void blit_msgstr_prt(BITMAP* dest, int32_t sx, int32_t sy, int32_t dx, int32_t dy, int32_t w, int32_t h)
{
	masked_blit(msg_portrait_display_buf, dest, sx, sy, dx, dy, w, h);
}

void clearmsgnext(int32_t str)
{
	MsgStrings[str].nextstring = 0;
}

void clr_msg_data();
void donewmsg(int32_t str)
{
	if(msg_onscreen || msg_active)
		dismissmsg();
	clr_msg_data();
    //al_trace("donewmsg %d\n",str);
    
        
    linkedmsgclk=0;
    msg_active = true;
    // Don't set msg_onscreen - not onscreen just yet
    msgstr = str;
    msgorig = msgstr;
    msgfont = setmsgfont();
    msgcolour=QMisc.colors.msgtext;
    msgspeed=zinit.msg_speed;
    
    if(introclk==0 || (introclk>=72 && dmapmsgclk==0))
	{
        clear_bitmap(msg_bg_display_buf);
        clear_bitmap(msg_txt_display_buf);
	}
        
    clear_bitmap(msg_bg_display_buf);
    set_clip_state(msg_bg_display_buf, 1);
	clear_bitmap(msg_portrait_display_buf);
    set_clip_state(msg_portrait_display_buf, 1);
    clear_bitmap(msg_txt_display_buf);
    set_clip_state(msg_txt_display_buf, 1);
    clear_bitmap(msg_txt_bmp_buf);
    clear_bitmap(msg_menu_bmp_buf);
    clear_bitmap(msg_bg_bmp_buf);
    clear_bitmap(msg_portrait_bmp_buf);
    msgclk=msgpos=msgptr=0;
    msgspace=true;
    msg_w=MsgStrings[msgstr].w;
    msg_h=MsgStrings[msgstr].h;
    msg_xpos=MsgStrings[msgstr].x;
    msg_ypos=MsgStrings[msgstr].y;
	prt_tile=MsgStrings[msgstr].portrait_tile;
	prt_cset=MsgStrings[msgstr].portrait_cset;
	prt_x=MsgStrings[msgstr].portrait_x;
	prt_y=MsgStrings[msgstr].portrait_y;
	prt_tw=MsgStrings[msgstr].portrait_tw;
	prt_th=MsgStrings[msgstr].portrait_th;
	msg_shdtype=MsgStrings[msgstr].shadow_type;
	msg_shdcol=MsgStrings[msgstr].shadow_color;
    
    msg_bg(MsgStrings[msgstr]);
    msg_prt();
    
	for(int32_t q = 0; q < 4; ++q)
	{
		msg_margins[q] = get_bit(quest_rules,qr_OLD_STRING_EDITOR_MARGINS)!=0 ? 0 : MsgStrings[msgstr].margins[q];
	}
    cursor_x=msg_margins[left];
    cursor_y=msg_margins[up];
}

// Called to make a message disappear
void dismissmsg()
{
    linkedmsgclk=0;
    msgstr = msgclk=msgpos=msgptr=0;
    cursor_x=0;
    cursor_y=0;
	prt_tile=0;
    msg_onscreen = msg_active = false;
    //Hero.finishedmsg(); //Not possible?
    clear_bitmap(msg_bg_display_buf);
    set_clip_state(msg_bg_display_buf, 1);
    clear_bitmap(msg_txt_display_buf);
    set_clip_state(msg_txt_display_buf, 1);
    clear_bitmap(msg_portrait_display_buf);
    set_clip_state(msg_portrait_display_buf, 1);
	clr_msg_data();
}

void dointro()
{
    if(game->visited[currdmap]!=1 || (DMaps[currdmap].flags&dmfALWAYSMSG)!=0)
    {
        dmapmsgclk=0;
        game->visited[currdmap]=1;
        introclk=intropos=0;
    }
    else
    {
        introclk=intropos=72;
    }
}

bool bad_version(int32_t version)
{
    // minimum zquest version allowed for any quest file
    if(version < 0x183)
        return true;
        
    return false;
}

extern char *weapon_string[];
extern char *item_string[];
extern char *sfx_string[];
extern char *guy_string[];


/**********************************/
/******* Other Source Files *******/
/**********************************/

bool get_debug()
{
    //return false; //remove later
    return __debug;
}

void set_debug(bool d)
{
    __debug=d;
    return;
}

void hit_close_button()
{
    close_button_quit=true;
    return;
}



// Yay, more extern globals.
extern byte curScriptType;
extern word curScriptNum;

void Z_eventlog(const char *format,...)
{
    if(get_bit(quest_rules,qr_LOG) || DEVLEVEL > 0)
    {
        char buf[2048];
        
        va_list ap;
        va_start(ap, format);
        vsprintf(buf, format, ap);
        va_end(ap);
        al_trace("%s",buf);
        
        if(zconsole)
            printf("%s",buf);
		if ( zscript_debugger ) {zscript_coloured_console.cprintf((CConsoleLoggerEx::COLOR_GREEN | CConsoleLoggerEx::COLOR_RED | CConsoleLoggerEx::COLOR_INTENSITY | 
			CConsoleLoggerEx::COLOR_BACKGROUND_BLACK),"%s",buf); }
    }
}

void Z_scripterrlog(const char * const format,...)
{
    if(get_bit(quest_rules,qr_SCRIPTERRLOG) || DEVLEVEL > 0)
    {
        switch(curScriptType)
        {
			case SCRIPT_GLOBAL:
				al_trace("Global script %u (%s): ", curScriptNum+1, globalmap[curScriptNum].scriptname.c_str());
				if ( zscript_debugger ) {zscript_coloured_console.cprintf((CConsoleLoggerEx::COLOR_GREEN | CConsoleLoggerEx::COLOR_INTENSITY | 
					CConsoleLoggerEx::COLOR_BACKGROUND_BLACK),"Global script %u (%s): \n", 
					curScriptNum+1, globalmap[curScriptNum].scriptname.c_str()); }
				break;
	
			case SCRIPT_PLAYER:
				al_trace("Hero script %u (%s): ", curScriptNum, playermap[curScriptNum-1].scriptname.c_str());
				if ( zscript_debugger ) { zscript_coloured_console.cprintf((CConsoleLoggerEx::COLOR_GREEN | CConsoleLoggerEx::COLOR_INTENSITY | 
					CConsoleLoggerEx::COLOR_BACKGROUND_BLACK),"Player script %u (%s): \n", curScriptNum, playermap[curScriptNum-1].scriptname.c_str()); }
				break;
			
			case SCRIPT_LWPN:
				al_trace("LWeapon script %u (%s): ", curScriptNum, lwpnmap[curScriptNum-1].scriptname.c_str());
				if ( zscript_debugger ) {zscript_coloured_console.cprintf((CConsoleLoggerEx::COLOR_GREEN | CConsoleLoggerEx::COLOR_INTENSITY | 
					CConsoleLoggerEx::COLOR_BACKGROUND_BLACK),"LWeapon script %u (%s): \n", curScriptNum, lwpnmap[curScriptNum-1].scriptname.c_str());}
				break;
			
			case SCRIPT_EWPN:
				al_trace("EWeapon script %u (%s): ", curScriptNum, ewpnmap[curScriptNum-1].scriptname.c_str());
				if ( zscript_debugger ) { zscript_coloured_console.cprintf((CConsoleLoggerEx::COLOR_GREEN | CConsoleLoggerEx::COLOR_INTENSITY | 
					CConsoleLoggerEx::COLOR_BACKGROUND_BLACK),"EWeapon script %u (%s): \n", curScriptNum, ewpnmap[curScriptNum-1].scriptname.c_str());}     
				break;
			
			case SCRIPT_NPC:
				al_trace("NPC script %u (%s): ", curScriptNum, npcmap[curScriptNum-1].scriptname.c_str());
				if ( zscript_debugger ) {zscript_coloured_console.cprintf((CConsoleLoggerEx::COLOR_GREEN | CConsoleLoggerEx::COLOR_INTENSITY | 
					CConsoleLoggerEx::COLOR_BACKGROUND_BLACK),"NPC script %u (%s): \n", curScriptNum, npcmap[curScriptNum-1].scriptname.c_str());}     
				break;
            
			case SCRIPT_FFC:
				al_trace("FFC script %u (%s): ", curScriptNum, ffcmap[curScriptNum-1].scriptname.c_str());
				if ( zscript_debugger ) {zscript_coloured_console.cprintf((CConsoleLoggerEx::COLOR_GREEN | CConsoleLoggerEx::COLOR_INTENSITY | 
					CConsoleLoggerEx::COLOR_BACKGROUND_BLACK),"FFC script %u (%s): ", curScriptNum, ffcmap[curScriptNum-1].scriptname.c_str());}
				break;
            
			case SCRIPT_ITEM:
				al_trace("Item script %u (%s): ", curScriptNum, itemmap[curScriptNum-1].scriptname.c_str());
				if ( zscript_debugger ) {zscript_coloured_console.cprintf((CConsoleLoggerEx::COLOR_GREEN | CConsoleLoggerEx::COLOR_INTENSITY | 
					CConsoleLoggerEx::COLOR_BACKGROUND_BLACK),"Itemdata script %u (%s): ", curScriptNum, itemmap[curScriptNum-1].scriptname.c_str());}  
				break;
			
			case SCRIPT_DMAP:
				al_trace("DMap script %u (%s): ", curScriptNum, dmapmap[curScriptNum-1].scriptname.c_str());
				if ( zscript_debugger ) {zscript_coloured_console.cprintf((CConsoleLoggerEx::COLOR_GREEN | CConsoleLoggerEx::COLOR_INTENSITY | 
					CConsoleLoggerEx::COLOR_BACKGROUND_BLACK),"DMap script %u (%s): ", curScriptNum, dmapmap[curScriptNum-1].scriptname.c_str());} 
				break;
			
			case SCRIPT_ITEMSPRITE:
				al_trace("itemsprite script %u (%s): ", curScriptNum, itemspritemap[curScriptNum-1].scriptname.c_str());
				if ( zscript_debugger ) {zscript_coloured_console.cprintf((CConsoleLoggerEx::COLOR_GREEN | CConsoleLoggerEx::COLOR_INTENSITY | 
					CConsoleLoggerEx::COLOR_BACKGROUND_BLACK),"itemsprite script %u (%s): ", curScriptNum, itemspritemap[curScriptNum-1].scriptname.c_str());} 
				break;
			
			case SCRIPT_SCREEN:
				al_trace("Screen script %u (%s): ", curScriptNum, screenmap[curScriptNum-1].scriptname.c_str());
				
				if ( zscript_debugger ) {zscript_coloured_console.cprintf((CConsoleLoggerEx::COLOR_GREEN | CConsoleLoggerEx::COLOR_INTENSITY | 
					CConsoleLoggerEx::COLOR_BACKGROUND_BLACK),"Screen script %u (%s): ", curScriptNum, screenmap[curScriptNum-1].scriptname.c_str());} 
				break;
			
			case SCRIPT_SUBSCREEN:
				al_trace("Subscreen script %u (%s): ", curScriptNum, itemmap[curScriptNum-1].scriptname.c_str());
				if ( zscript_debugger ) {zscript_coloured_console.cprintf((CConsoleLoggerEx::COLOR_GREEN | CConsoleLoggerEx::COLOR_INTENSITY | 
					CConsoleLoggerEx::COLOR_BACKGROUND_BLACK),"Subscreen script %u (%s): ", curScriptNum, itemmap[curScriptNum-1].scriptname.c_str());} 
				break;
			
			case SCRIPT_COMBO:
				al_trace("Combodata script %u (%s): ", curScriptNum, comboscriptmap[curScriptNum-1].scriptname.c_str());
				if ( zscript_debugger ) {zscript_coloured_console.cprintf((CConsoleLoggerEx::COLOR_GREEN | CConsoleLoggerEx::COLOR_INTENSITY | 
					CConsoleLoggerEx::COLOR_BACKGROUND_BLACK),"Combo script %u (%s): ", curScriptNum, comboscriptmap[curScriptNum-1].scriptname.c_str());}
				break;
        }
        
        char buf[2048];
        
        va_list ap;
        va_start(ap, format);
        vsprintf(buf, format, ap);
        va_end(ap);
        al_trace("%s",buf);
        
        if(zconsole)
		{
            printf("%s",buf);
		}
		if ( zscript_debugger ) 
		{
			zscript_coloured_console.cprintf((CConsoleLoggerEx::COLOR_RED | CConsoleLoggerEx::COLOR_INTENSITY | 
				CConsoleLoggerEx::COLOR_BACKGROUND_BLACK),"%s",buf);
		}
    }
}

bool blockmoving;
#include "sprite.h"
movingblock mblock2;                                        //mblock[4]?
portal* mirror_portal = NULL;

sprite_list  guys, items, Ewpns, Lwpns, Sitems, chainlinks, decorations;
particle_list particles;

#include "zc_custom.h"
#include "hero.h"
HeroClass   Hero;

#include "maps.h"
#include "subscr.h"
#include "guys.h"

#include "title.h"
#include "ending.h"

#include "zc_sys.h"
//extern MENU the_player_menu;
//extern MENU the_player_menu2;
//extern byte refresh_select_screen;

// Wait... this is only used by ffscript.cpp!?
void addLwpn(int32_t x,int32_t y,int32_t z,int32_t id,int32_t type,int32_t power,int32_t dir, int32_t parentid)
{
    Lwpns.add(new weapon((zfix)x,(zfix)y,(zfix)z,id,type,power,dir,-1,parentid));
}


void addLwpnEx(int32_t x,int32_t y,int32_t z,int32_t id,int32_t type,int32_t power,int32_t dir, int32_t parentitem, int32_t parentid, byte script_gen)
{
	//weapon::weapon(zfix X,zfix Y,zfix Z,int32_t Id,int32_t Type,int32_t pow,int32_t Dir, int32_t Parentitem, int32_t prntid, byte script_gen, bool isDummy)
    Lwpns.add(new weapon((zfix)x,(zfix)y,(zfix)z,id,type,power,dir,parentitem,parentid,false,1));
	
}

void ALLOFF(bool messagesToo, bool decorationsToo)
{
    if(messagesToo)
    {
        clear_bitmap(msg_bg_display_buf);
        set_clip_state(msg_bg_display_buf, 1);
        clear_bitmap(msg_txt_display_buf);
        set_clip_state(msg_txt_display_buf, 1);
        clear_bitmap(msg_portrait_display_buf);
        set_clip_state(msg_portrait_display_buf, 1);
    }
    
    clear_bitmap(pricesdisplaybuf);
    set_clip_state(pricesdisplaybuf, 1);
    
    if(items.idCount(iPile))
    {
        loadlvlpal(DMaps[currdmap].color);
    }
    
    items.clear();
    guys.clear();
    Lwpns.clear();
    Ewpns.clear();
    chainlinks.clear();
	if(mirror_portal)
	{
		delete mirror_portal;
		mirror_portal = NULL;
	}
    clearScriptHelperData();
    
    if(decorationsToo)
        decorations.clear();
        
    particles.clear();
    
    if(Hero.getNayrusLoveShieldClk())
    {
        Hero.setNayrusLoveShieldClk(Hero.getNayrusLoveShieldClk());
    }
    
    Hero.resetflags(false);
    Hero.reset_hookshot();
    linkedmsgclk=0;
    add_asparkle=0;
    add_bsparkle=0;
    add_df1asparkle=false;
    add_df1bsparkle=false;
    add_nl1asparkle=false;
    add_nl1bsparkle=false;
    add_nl2asparkle=false;
    add_nl2bsparkle=false;
    //  for(int32_t i=0; i<1; i++)
    mblock2.clk=0;
    dismissmsg();
    fadeclk=-1;
    introclk=intropos=72;
    
    lensclk = 0;
    lensid=-1;
    drawguys=Udown=Ddown=Ldown=Rdown=Adown=Bdown=Sdown=true;
    
    if(watch && !cheat_superman)
    {
        Hero.setClock(false);
    }
    
    //  if(watch)
    //    Hero.setClock(false);
    watch=freeze_guys=loaded_guys=loaded_enemies=blockpath=false;
    stop_sfx(WAV_BRANG);
    
    for(int32_t i=0; i<176; i++)
        guygrid[i]=0;
        
    sle_clk=0;
    blockmoving=false;
    fairy_cnt=0;
    
    if(usebombpal)
    {
        memcpy(RAMpal, tempbombpal, PAL_SIZE*sizeof(RGB));
        refreshpal=true;
        usebombpal=false;
    }
}
void centerHero()
{
    Hero.setX(120);
    Hero.setY(80);
}
zfix  HeroX()
{
    return Hero.getX();
}
zfix  HeroY()
{
    return Hero.getY();
}
zfix  HeroZ()
{
    return Hero.getZ();
}
int32_t  HeroHClk()
{
    return Hero.getHClk();
}
int32_t  HeroAction()
{
    return Hero.getAction();
}
int32_t  HeroCharged()
{
    return Hero.isCharged();
}
int32_t  HeroNayrusLoveShieldClk()
{
    return Hero.getNayrusLoveShieldClk();
}
int32_t  HeroHoverClk()
{
    return Hero.getHoverClk();
}
int32_t  HeroSwordClk()
{
    return Hero.getSwordClk();
}
int32_t  HeroItemClk()
{
    return Hero.getItemClk();
}
void setSwordClk(int32_t newclk)
{
    Hero.setSwordClk(newclk);
}
void setItemClk(int32_t newclk)
{
    Hero.setItemClk(newclk);
}
int32_t  HeroLStep()
{
    return Hero.getLStep();
}
void HeroCheckItems()
{
    Hero.checkitems();
}
byte HeroGetDontDraw()
{
    return Hero.getDontDraw();
}
zfix  GuyX(int32_t j)
{
    return guys.getX(j);
}
zfix  GuyY(int32_t j)
{
    return guys.getY(j);
}
int32_t  GuyID(int32_t j)
{
    return guys.getID(j);
}
int32_t  GuyMisc(int32_t j)
{
    return guys.getMisc(j);
}
bool  GuySuperman(int32_t j)
{
    if((j>=guys.Count())||(j<0))
    {
        return true;
    }
    
    return ((enemy*)guys.spr(j))->superman !=0;
}

int32_t  GuyCount()
{
    return guys.Count();
}
void StunGuy(int32_t j,int32_t stun)
{
    if(stun<=0) return;
    
    if(((enemy*)guys.spr(j))->z==0 && canfall(((enemy*)guys.spr(j))->id))
    {
        ((enemy*)guys.spr(j))->stunclk=zc_min(360,stun*4);
        ((enemy*)guys.spr(j))->fall=-zc_min(FEATHERJUMP,(stun*8)+zc_oldrand()%5);
    }
    else if(((enemy*)guys.spr(j))->z==0 && ((enemy*)guys.spr(j))->family == eeLEV && get_bit(quest_rules, qr_QUAKE_STUNS_LEEVERS))
    {
        ((enemy*)guys.spr(j))->stunclk=zc_min(360,stun*4);
    }
}

zfix HeroModifiedX()
{
    return Hero.getModifiedX();
}
zfix HeroModifiedY()
{
    return Hero.getModifiedY();
}
int32_t HeroDir()
{
    return Hero.getDir();
}
void add_grenade(int32_t wx, int32_t wy, int32_t wz, int32_t size, int32_t parentid)
{
    if(size)
    {
        Lwpns.add(new weapon((zfix)wx,(zfix)wy,(zfix)wz,wSBomb,0,16*game->get_hero_dmgmult(),HeroDir(),
                             -1, parentid));
        Lwpns.spr(Lwpns.Count()-1)->id=wSBomb;
    }
    else
    {
        Lwpns.add(new weapon((zfix)wx,(zfix)wy,(zfix)wz,wBomb,0,4*game->get_hero_dmgmult(),HeroDir(),
                             -1, parentid));
        Lwpns.spr(Lwpns.Count()-1)->id=wBomb;
    }
    
    Lwpns.spr(Lwpns.Count()-1)->clk=48;
    Lwpns.spr(Lwpns.Count()-1)->misc=50;
}

zfix distance(int32_t x1, int32_t y1, int32_t x2, int32_t y2)

{
    return (zfix)sqrt(pow((double)abs(x1-x2),2)+pow((double)abs(y1-y2),2));
}

bool getClock()
{
    return Hero.getClock();
}
void setClock(bool state)
{
    Hero.setClock(state);
}
void CatchBrang()
{
    Hero.Catch();
}

/**************************/
/***** Main Game Code *****/
/**************************/

int8_t smart_vercmp(char const* a, char const* b)
{
	for(int q = 0; a[q]; ++q)
	{
		if((a[q] >= '0' && a[q] <= '9') || a[q] == '.')
			continue;
		return strcmp(a, b);
	}
	for(int q = 0; b[q]; ++q)
	{
		if((b[q] >= '0' && b[q] <= '9') || b[q] == '.')
			continue;
		return strcmp(a, b);
	}
	char *cpya = (char*)zc_malloc(strlen(a)+1), *cpyb = (char*)zc_malloc(strlen(b)+1);
	strcpy(cpya, a); strcpy(cpyb, b);
	char *ptra = cpya, *ptrb = cpyb, *tmpa = cpya, *tmpb = cpyb;
	std::vector<int32_t> avec, bvec;
	while(*(ptra++))
	{
		if(*ptra == '.')
		{
			*ptra = 0;
			avec.push_back(atoi(tmpa));
			tmpa = ++ptra;
			if(!*ptra) break;
		}
		if(!*ptra)
		{
			avec.push_back(atoi(tmpa));
			break;
		}
	}
	while(*(ptrb++))
	{
		if(*ptrb == '.')
		{
			*ptrb = 0;
			bvec.push_back(atoi(tmpb));
			tmpb = ++ptrb;
			if(!*ptrb) break;
		}
		if(!*ptrb)
		{
			avec.push_back(atoi(tmpb));
			break;
		}
	}
	zc_free(cpya); zc_free(cpyb);
	while(avec.size() < bvec.size())
		avec.push_back(0);
	while(bvec.size() < avec.size())
		bvec.push_back(0);
	for(uint32_t q = 0; q < avec.size(); ++q)
	{
		if(avec.at(q) < bvec.at(q))
			return -1;
		if(avec.at(q) > bvec.at(q))
			return 1;
	}
	return 0;
}

int32_t load_quest(gamedata *g, bool report, byte printmetadata)
{
	chop_path(qstpath);
	char *tempdir=(char *)"";
	int32_t ret = 0;
#ifndef ALLEGRO_MACOSX
	tempdir=qstdir;
#endif
	byte qst_num = byte(g->get_quest()-1);
	if(!g->qstpath[0])
	{
		if(qst_num<moduledata.max_quest_files)
		{
			sprintf(qstpath, moduledata.quests[qst_num], ordinal(qst_num+1));
			strcpy(g->qstpath, qstpath);
		}
	}
	if(g->qstpath[0])
	{
		if(is_relative_filename(g->qstpath))
		{
			sprintf(qstpath,"%s%s",qstdir,g->qstpath);
		}
		else
		{
			sprintf(qstpath,"%s", g->qstpath);
		}

		// ZC paths are retarded.
		// This is just an awful hack, and generally some of the worst code ever written, but it only ever gets run
		// when there would be a "file not found error"... (it's easier than dealing with everything else. Sue me.)
		//
		// *side note*
		// It does have a few perks though: You can now move around entire folders and sub-folders from one ZC directory to the next,
		// and we can find them! You can even put all the ZC quests into different sub-directories and share save files, etc..
		// ~Gleeok
		if(!exists(qstpath))
		{
			Z_error("File not found \"%s\". Searching...\n", qstpath);

			if(exists(g->qstpath)) //not found? -try this place first:
			{
				sprintf(qstpath,"%s", g->qstpath);
				Z_error("Set quest path to \"%s\".\n", qstpath);
			}
			else // Howsabout in here?
			{
				std::string gQstPath = g->qstpath;
				size_t bs1 = 0;
				size_t bs2 = std::string::npos;

				// Keep searching through every parent directory as if it was the current one.
				while(bs1 != std::string::npos || bs2 != std::string::npos)
				{
					bs1 = gQstPath.find_first_of('/');
					if(bs1 != std::string::npos)
					{
						gQstPath = gQstPath.substr(bs1 + 1, std::string::npos);
					}
					else
					{
						bs2 = gQstPath.find_first_of('\\');
						if(bs2 != std::string::npos)
							gQstPath = gQstPath.substr(bs2 + 1, std::string::npos);
					}

					if(exists(gQstPath.c_str())) //Quick! Try it now!
					{
						sprintf(qstpath,"%s", gQstPath.c_str());
						Z_error("Set quest path to \"%s\".\n", qstpath);
						break;
					}
					else //Still no dice eh?
					{
						char cwdbuf[260];
						memset(cwdbuf,0,260*sizeof(char));
						getcwd(cwdbuf, 260);

						std::string path = cwdbuf;
						std::string fn;

						if(path.size() != 0 && 
							!(*(path.end()-1)=='/' || *(path.end()-1)=='\\')
							)
						{
							path += '/';
						}

						fn = path + gQstPath;

						if(exists(fn.c_str())) //Last chance for hookers and blackjack truck stop
						{
							sprintf(qstpath,"%s", fn.c_str());
							Z_error("Set quest path to \"%s\".\n", qstpath);
							break;
						}
					}
				} //while
			}
		}//end hack
	}
	else ret = qe_no_qst;
	
	if(!ret)
	{
		//setPackfilePassword(datapwd);
		byte skip_flags[4];
		
		for(int32_t i=0; i<4; ++i)
		{
			skip_flags[i]=0;
		}
		
		ret = loadquest(qstpath,&QHeader,&QMisc,tunes+ZC_MIDI_COUNT,false,true,true,true,skip_flags,printmetadata,report,qst_num);
		//zprint2("qstpath: '%s', qstdir(cfg): '%s', standalone_quest: '%s'\n",qstpath,get_config_string("zeldadx",qst_dir_name,""),standalone_quest?standalone_quest:"");
		//setPackfilePassword(NULL);
		
		if(!g->title[0] || g->get_hasplayed() == 0)
		{
			strcpy(g->version,QHeader.version);
			strcpy(g->title,QHeader.title);
		}
		else
		{
			if(!ret && strcmp(g->title,QHeader.title))
			{
				ret = qe_match;
			}
		}
		
		if(QHeader.minver[0])
		{
			if(smart_vercmp(g->version,QHeader.minver) < 0)
				ret = qe_minver;
		}
	}
	if(ret && report)
	{
		system_pal();
		std::ostringstream oss;
		if(ret == qe_no_qst)
		{
			oss << qst_error[ret] << ". Press the 'A' button twice to select a custom quest,"
				"\nor load a module that has a default " << ordinal(qst_num+1) << " quest.";
		}
		else
		{
			oss << "Error loading " << get_filename(qstpath)
				<< ":\n" << qst_error[ret] << "\n" << qstpath;
		}
		InfoDialog("File Error",oss.str()).show();
		
		if(standalone_mode)
		{
			exit(1);
		}
		
		game_pal();
	}
	
	return ret;
}

void init_dmap()
{
    // readjust disabled items; could also do dmap-specific scripts here
    for(int32_t i=0; i<MAXITEMS; i++)
    {
        game->items_off[i] &= (~1); // disable last bit - this one is set by dmap
        game->items_off[i] |= DMaps[currdmap].disableditems[i]; // and reset if required
    }
    
    flushItemCache();
    // also update subscreens
    update_subscreens();
    verifyBothWeapons();
    
    return;
}



int32_t init_game()
{
	//port250QuestRules();	
	zc_srand(time(0));
	//introclk=intropos=msgclk=msgpos=dmapmsgclk=0;
	FFCore.kb_typing_mode = false;
	
	for ( int32_t q = 0; q < 256; q++ ) runningItemScripts[q] = 0; //Clear scripts that were running before. 
	draw_screen_clip_rect_x1=0; //Prevent the ending sequence from carrying over through 'Reset System' -Z
	draw_screen_clip_rect_x2=255;
	draw_screen_clip_rect_y1=0;
	draw_screen_clip_rect_y2=223;	
	
	//Some initialising globals
	didpit=false;
	Hero.unfreeze();
	Hero.reset_hookshot();
	Hero.reset_ladder();
	linkedmsgclk=0;
	blockmoving=false;
	add_asparkle=0;
	add_bsparkle=0;
	add_df1asparkle=false;
	add_df1bsparkle=false;
	add_nl1asparkle=false;
	add_nl1bsparkle=false;
	add_nl2asparkle=false;
	add_nl2bsparkle=false;
	gofast=false;
	FFCore.init();
	FFCore.user_bitmaps_init();
	FFCore.user_files_init();
	FFCore.user_dirs_init();
	cheat=0;
	wavy=quakeclk=0;
	show_layer_0=show_layer_1=show_layer_2=show_layer_3=show_layer_4=show_layer_5=show_layer_6=true;
	show_layer_over=show_layer_push=show_sprites=show_ffcs=true;
	cheat_superman=do_cheat_light=do_cheat_goto=show_walkflags=show_effectflags=show_ff_scripts=show_hitboxes=false;
	//al_trace("Clearing old RenderTarget\n");
	if ( zscriptDrawingRenderTarget ) zscriptDrawingRenderTarget->SetCurrentRenderTarget(-1); //clear the last set Rendertarget between games
	//zscriptDrawingRenderTarget = new ZScriptDrawingRenderTarget();
	
	
	for(int32_t x = 0; x < MAXITEMS; x++)
	{
		lens_hint_item[x][0]=0;
		lens_hint_item[x][1]=0;
	}
	
	for(int32_t x = 0; x < MAXWPNS; x++)
	{
		lens_hint_weapon[x][0]=0;
		lens_hint_weapon[x][1]=0;
	}
	
	/* Disabling to see if this is causing virus scanner redflags. -Z
	//Confuse the cheaters by moving the game data to a random location
	if(game != NULL)
		delete game;
		
	char *dummy = (char *) zc_malloc((zc_oldrand()%(RAND_MAX/2))+32);
	game = new gamedata;
	game->Clear();
	
	zc_free(dummy);
	*/
	//Copy saved data to RAM data (but not global arrays)
	game->Copy(saves[currgame]);
	flushItemCache();
	ResetSaveScreenSettings();
	
	//ResetSaveScreenSettings();
	
	//Load the quest
	//setPackfilePassword(datapwd);
	int32_t ret = load_quest(game);
	
	if(ret != qe_OK)
	{
		Quit = qERROR;
		//setPackfilePassword(NULL);
		return 1;
	}
	
	ResetSaveScreenSettings();
	
	maxcheat = game->get_cheat();
	
	//setPackfilePassword(NULL);
	
	char keyfilename[2048]; //master key .key
	char keyfilename2[2048]; //zquest key .zpwd
	char keyfilename3[2048]; //zc cheats only key, .zcheat
	replace_extension(keyfilename, qstpath, "key", 2047);
	replace_extension(keyfilename2, qstpath, "zpwd", 2047);
	replace_extension(keyfilename3, qstpath, "zcheat", 2047);
	bool gotfromkey=false;
	bool gotfrompwdfile=false;
	bool gotfromcheatfile=false;
	
	if(exists(keyfilename))
	{
		al_trace("Found a Quest Master Key, filename: %s\n", keyfilename);
		char password[32], pwd[32];
		PACKFILE *fp = pack_fopen_password(keyfilename, F_READ,"");
		char msg[80];
		memset(msg,0,80);
		pfread(msg, 80, fp,true);
		
		if(strcmp(msg,"ZQuest Auto-Generated Quest Password Key File.  DO NOT EDIT!")==0)
		{
			al_trace("Found Quest Master Key\n");
			int16_t ver;
			byte  bld;
			p_igetw(&ver,fp,true);
			p_getc(&bld,fp,true);
			memset(password,0,32);
			pfread(password, 30, fp,true);
			/*
			get-questpwd(&QHeader, pwd);
			if (strcmp(pwd,password)==0)
			{
				gotfromkey=true;
			}
			*/
			gotfromkey=check_questpwd(&QHeader, password);
			memset(password,0,32);
			memset(pwd,0,32);
		}
		
		pack_fclose(fp);
	goto skip_keychecks;
	}
	
	if(exists(keyfilename2)) //zquest key...superfluous?
	{
		al_trace("Found a ZQuest Password Key, filename: %s\n", keyfilename2);
		char password[32], pwd[32];
		PACKFILE *fp = pack_fopen_password(keyfilename2, F_READ,"");
		char msg[80];
		memset(msg,0,80);
		pfread(msg, 80, fp,true);
		
		if(strcmp(msg,"ZQuest Auto-Generated Quest Password Key File.  DO NOT EDIT!")==0)
		{
			int16_t ver;
			byte  bld;
			p_igetw(&ver,fp,true);
			p_getc(&bld,fp,true);
			memset(password,0,32);
			pfread(password, 30, fp,true);
			/*
			get-questpwd(&QHeader, pwd);
			if (strcmp(pwd,password)==0)
			{
				gotfromkey=true;
			}
			*/
			gotfrompwdfile=check_questpwd(&QHeader, password);
			memset(password,0,32);
			memset(pwd,0,32);
		}
		
		pack_fclose(fp);
	}
	
	if(exists(keyfilename3)) //zc cheat key
	{
		al_trace("Found a ZC Cheat Key, filename: %s\n", keyfilename3);
		
		char password[32], pwd[32];
		PACKFILE *fp = pack_fopen_password(keyfilename3, F_READ,"");
		char msg[80];
		memset(msg,0,80);
		pfread(msg, 80, fp,true);
		
		if(strcmp(msg,"ZQuest Auto-Generated Quest Password Key File.  DO NOT EDIT!")==0)
		{
		//al_trace("checking .zcheat file header %s","\n");
			int16_t ver;
			byte  bld;
			p_igetw(&ver,fp,true);
			p_getc(&bld,fp,true);
		//al_trace("about to read password\n");
			memset(password,0,32);
		pfread(password, 30, fp,true);
			//pfread(password, 30, fp,true);
		
		//al_trace("making space for hash\n");
		char unhashed_pw[32];
		memset(unhashed_pw,0,32);
		
			
		
		char hashmap = 'Z';
		hashmap += 'Q';
		hashmap += 'U';
		hashmap += 'E';
		hashmap += 'S';
		hashmap += 'T';
		
		//al_trace("applying reverse hash\n");
		for ( int32_t q = 0; q < 32; q++ ) 
		{
			unhashed_pw[q] = password[q] - hashmap;
		}
		//al_trace("hashed password is: %s\n", password);
		//al_trace("un-hashed password is: %s\n", unhashed_pw);
		
		
		
			
			//get-questpwd(&QHeader, pwd);
			//if (strcmp(pwd,password)==0)
			//{
			//	gotfromkey=true;
			//}
			
		   // gotfromcheatfile=check_questpwd(&QHeader, unhashed_pw);
			memset(password,0,32);
			memset(unhashed_pw,0,32);
			memset(pwd,0,32);
			cheat = 4;
			maxcheat = 4;
		}
		
		pack_fclose(fp);
	}
	else goto skip_keycheats;
	
	skip_keychecks:
	
	if(gotfromkey)
	{
		cheat = 4;
		maxcheat = 4;
	}
	
	skip_keycheats:
	//Calculate the quest's script-file-storage path -V
	{
		memset(qst_files_path, sizeof(qst_files_path), 0);
		string str(qstpath);
		size_t pos = str.find_last_of("/\\");
		if(pos==string::npos) pos=0;
		else ++pos;
		size_t dotpos = str.find_last_of(".");
		sprintf(qst_files_path,"%sFiles/%s",get_config_string("zeldadx", qst_dir_name, "./"),str.substr(pos, dotpos-pos).c_str());
		regulate_path(qst_files_path);
		// zprint2("Calculated path: '%s'\n",qst_files_path);
		// zprint2("Path creating... %s\n",create_path(qst_files_path)?"Success!":"Failure!");
	}
	
	if(zqtesting_mode)
	{
		cheat = 4;
		maxcheat = 4;
	}
	bool firstplay = (game->get_hasplayed() == 0);
	
	BSZ = get_bit(quest_rules,qr_BSZELDA)!=0;
	//setupherotiles(zinit.heroAnimationStyle);
	
	COOLSCROLL = (get_bit(quest_rules,qr_COOLSCROLL)!=0 ? 1 : 0) |
				 (get_bit(quest_rules,qr_OVALWIPE)!=0 ? 2 : 0) |
				 (get_bit(quest_rules,qr_TRIANGLEWIPE)!=0 ? 4 : 0) |
				 (get_bit(quest_rules,qr_SMASWIPE)!=0 ? 8 : 0) |
				 (get_bit(quest_rules,qr_FADEBLACKWIPE)!=0 ? 16 : 0);
	identifyCFEnemies();
				 
	//  NEWSUBSCR = get_bit(quest_rules,qr_NEWSUBSCR);
	
	//  homescr = currscr = DMaps[0].cont;
	//  currdmap = warpscr = worldscr=0;
	if(firstplay)
	{
		if(!zqtesting_mode)
			game->set_continue_dmap(zinit.start_dmap);
		resetItems(game,&zinit,true);
		if ( FFCore.getQuestHeaderInfo(vZelda) < 0x190 )
		{
			game->set_maxbombs(8);
			//al_trace("Starting bombs set to %d for a quest made in ZC %x\n", game->get_maxbombs(), (unsigned)FFCore.getQuestHeaderInfo(vZelda));
		}
	}
	
	previous_DMap = currdmap = warpscr = worldscr=game->get_continue_dmap();
	init_dmap();
	
	if(game->get_continue_scrn() >= 0x80)
	{
		//if ((DMaps[currdmap].type&dmfTYPE)==dmOVERW || QHeader.zelda_version <= 0x190)
		if((DMaps[currdmap].type&dmfTYPE)==dmOVERW)
		{
			homescr = currscr = DMaps[currdmap].cont;
		}
		else
		{
			homescr = currscr = DMaps[currdmap].cont + DMaps[currdmap].xoff;
		}
	}
	else
	{
		homescr = currscr = game->get_continue_scrn();
	}
	
	lastentrance = currscr;
	game->set_continue_scrn(lastentrance);
	lastentrance_dmap = currdmap;
	currmap = DMaps[currdmap].map;
	dlevel = DMaps[currdmap].level;
	sle_x=sle_y=newscr_clk=opendoors=Bwpn=Bpos=0;
	fadeclk=-1;
	
	if(currscr < 0x80 && (DMaps[currdmap].flags&dmfVIEWMAP))
	{
		game->maps[(currmap*MAPSCRSNORMAL)+currscr] |= mVISITED;			  // mark as visited
	}
	
	for(int32_t i=0; i<6; i++)
	{
		visited[i]=-1;
	}
	
	game->lvlitems[9] &= ~liBOSS;
	
	ALLOFF();
	whistleclk=-1;
	clockclk=0;
	currcset=DMaps[currdmap].color;
	darkroom=naturaldark=false;
	
	tmpscr[0].zero_memory();
	tmpscr[1].zero_memory();
	//clear initialise dmap script 
	dmapscriptInitialised = 0;
	//Script-related nonsense
	script_drawing_commands.Clear();
	
	//CLear the scripted Player sprites. 
	script_hero_sprite = 0; 
	script_hero_flip = -1; 
	script_hero_cset = -1; 
	
	initZScriptArrayRAM(firstplay);
	initZScriptGlobalRAM();
	FFCore.initZScriptHeroScripts();
	FFCore.initZScriptDMapScripts();
	FFCore.initZScriptActiveSubscreenScript();
	FFCore.initZScriptItemScripts();
	
	
	//Run the init script or the oncontinue script with the highest priority.
	//GLobal Script Init ~Init
/*
	if(firstplay)
	{
		memset(game->screen_d, 0, MAXDMAPS * 64 * 8 * sizeof(int32_t));
		ZScriptVersion::RunScript(SCRIPT_GLOBAL, GLOBAL_SCRIPT_INIT, GLOBAL_SCRIPT_INIT);
	}
	else
	{
		//Global script OnContinue
		ZScriptVersion::RunScript(SCRIPT_GLOBAL, GLOBAL_SCRIPT_ONSAVELOAD, GLOBAL_SCRIPT_ONSAVELOAD); //Do this after global arrays have been loaded
	}
*/
	global_wait=0;
	
	//show quest metadata when loading it
	print_quest_metadata(QHeader, qstpath, byte(game->get_quest()-1));
	
	FFCore.init(); ///Initialise new ffscript engine core. 
	if(!firstplay && !get_bit(quest_rules, qr_OLD_INIT_SCRIPT_TIMING))
	{
		ZScriptVersion::RunScript(SCRIPT_GLOBAL, GLOBAL_SCRIPT_ONSAVELOAD, GLOBAL_SCRIPT_ONSAVELOAD); //Do this after global arrays have been loaded
		FFCore.deallocateAllArrays(SCRIPT_GLOBAL, GLOBAL_SCRIPT_ONSAVELOAD);
	}
	//loadscr(0,currscr,up);
	loadscr(0,currdmap,currscr,-1,false);
	putscr(scrollbuf,0,0,&tmpscr[0]);
	putscrdoors(scrollbuf,0,0,&tmpscr[0]);
	
	//preloaded freeform combos
	//ffscript_engine(true); Can't do this here! Global arrays haven't been allocated yet... ~Joe
	
	Hero.init();
	if(zqtesting_mode
		&& currscr == testingqst_screen
		&& currdmap == testingqst_dmap)
	{
		Hero.setX(tmpscr->warpreturnx[testingqst_retsqr]);
		Hero.setY(tmpscr->warpreturny[testingqst_retsqr]);
	}
	if(DMaps[currdmap].flags&dmfBUNNYIFNOPEARL)
	{
		int32_t itemid = current_item_id(itype_pearl);
		if(itemid < 0)
		{
			Hero.setBunnyClock(-1);
		}
	}
	
	if(firstplay) //Move up here, so that arrays are initialised before we run Hero's Init script.
	{
		memset(game->screen_d, 0, MAXDMAPS * 64 * 8 * sizeof(int32_t));
		if(!get_bit(quest_rules, qr_OLD_INIT_SCRIPT_TIMING))
		{
			ZScriptVersion::RunScript(SCRIPT_GLOBAL, GLOBAL_SCRIPT_INIT, GLOBAL_SCRIPT_INIT);
			FFCore.deallocateAllArrays(SCRIPT_GLOBAL, GLOBAL_SCRIPT_INIT); //Deallocate LOCAL arrays declared in the init script. This function does NOT deallocate global arrays.
		}
		if ( FFCore.getQuestHeaderInfo(vZelda) >= 0x255 )
		{
			ZScriptVersion::RunScript(SCRIPT_PLAYER, SCRIPT_PLAYER_INIT, SCRIPT_PLAYER_INIT); //We run this here so that the user can set up custom
									//positional data, sprites, tiles, csets, invisibility states, and the like.
			FFCore.deallocateAllArrays(SCRIPT_PLAYER, SCRIPT_PLAYER_INIT);
		}
		FFCore.initZScriptHeroScripts(); //Clear the stack and the refinfo data to be ready for Hero's active script.
		Hero.set_respawn_point(); //This should be after the init script, so that Hero->X and Hero->Y set by the script
						//are properly set by the engine.
	}
	Hero.resetflags(true); //This should probably occur after running Hero's init script. 
	
	
	copy_pal((RGB*)data[PAL_GUI].dat,RAMpal);
	loadfullpal();
	ringcolor(false);
	loadlvlpal(DMaps[currdmap].color);
	lighting(false,true);
	wavy=quakeclk=0;
	
	if(firstplay)
	{
		game->set_life(zinit.start_heart*game->get_hp_per_heart());
	}
	else
	{
		if(game->get_cont_percent())
		{
			if(game->get_maxlife()%game->get_hp_per_heart()==0)
				game->set_life(((game->get_maxlife()*game->get_cont_hearts()/100)/game->get_hp_per_heart())*game->get_hp_per_heart());
			else
				game->set_life(game->get_maxlife()*game->get_cont_hearts()/100);
		}
		else
		{
			game->set_life(game->get_cont_hearts()*game->get_hp_per_heart());
		}
	}
	
	if(firstplay)
		game->set_hasplayed(1);
	
	if(firstplay)
	{
		game->awpn=-1;
		game->bwpn=-1;
		game->ywpn=-1;
		game->xwpn=-1;
		game->forced_awpn = -1; 
		game->forced_bwpn = -1;  
		game->forced_xwpn = -1; 
		game->forced_ywpn = -1;	
	}
		
	update_subscreens();
	
	load_Sitems(&QMisc);
	
	//load the previous weapons -DD	
	
	bool usesaved = (game->get_quest() == 0xFF); //What was wrong with firstplay?
	int32_t apos = 0, bpos = 0, xpos = 0, ypos = 0;
	
	//Setup button items
	{
		bool use_x = get_bit(quest_rules, qr_SET_XBUTTON_ITEMS), use_y = get_bit(quest_rules, qr_SET_YBUTTON_ITEMS);
		if(use_x || use_y)
		{
			if(!get_bit(quest_rules, qr_SELECTAWPN))
			{
				Awpn = selectSword();
				apos = -1;
				bpos = selectWpn_new(SEL_VERIFY_RIGHT, usesaved ? game->bwpn : 0xFF);
				if(use_x)
					xpos = selectWpn_new(SEL_VERIFY_RIGHT, usesaved ? game->bwpn : 0xFF, bpos);
				else xpos = -1;
				if(use_y)
					ypos = selectWpn_new(SEL_VERIFY_RIGHT, usesaved ? game->bwpn : 0xFF, bpos, xpos);
				else ypos = -1;
				directItem = -1;
				directItemA = directItem; 
			}
			else
			{
				apos = selectWpn_new(SEL_VERIFY_RIGHT, usesaved ? game->awpn : 0xFF);
				bpos = selectWpn_new(SEL_VERIFY_RIGHT, usesaved ? game->bwpn : 0xFF, apos);
				if(use_x)
					xpos = selectWpn_new(SEL_VERIFY_RIGHT, usesaved ? game->bwpn : 0xFF, apos, bpos);
				else xpos = -1;
				if(use_y)
					ypos = selectWpn_new(SEL_VERIFY_RIGHT, usesaved ? game->bwpn : 0xFF, apos, bpos, xpos);
				else ypos = -1;
				
				if(bpos==0xFF)
				{
					bpos=-1;
				}
				if(apos==0xFF)
				{
					apos=-1;
				}
				if(xpos==0xFF)
				{
					xpos=-1;
				}
				if(bpos==0xFF)
				{
					ypos=-1;
				}
				
				Awpn = Bweapon(apos); //Bweapon() sets directItem
				directItemA = directItem;
			}

			game->awpn = apos;
			
			game->bwpn = bpos;
			Bwpn = Bweapon(bpos);
			directItemB = directItem;
			
			game->xwpn = xpos;
			Xwpn = Bweapon(xpos);
			directItemX = directItem;
			
			game->ywpn = ypos;
			Ywpn = Bweapon(ypos);
			directItemY = directItem;
			
			update_subscr_items();

			reset_subscr_items();
		}
		else
		{
			if(!get_bit(quest_rules,qr_SELECTAWPN))
			{
				Awpn = selectSword();
				apos = -1;
				bpos = selectWpn_new(SEL_VERIFY_RIGHT, usesaved ? game->bwpn : 0xFF, -1);
				directItem = -1;
				directItemA = directItem; 
			}
			else
			{
				apos = selectWpn_new(SEL_VERIFY_RIGHT, usesaved ? game->awpn : 0xFF);
				bpos = selectWpn_new(SEL_VERIFY_RIGHT, usesaved ? game->bwpn : 0xFF, apos);
				
				if(bpos==0xFF)
				{
					bpos=apos;
					apos=-1;
				}
				
				Awpn = Bweapon(apos); //Bweapon() sets directItem
				directItemA = directItem;
			}

			game->awpn = apos;
			game->bwpn = bpos;
			Bwpn = Bweapon(bpos);
			directItemB = directItem;
			update_subscr_items();

			reset_subscr_items();
		}
	}
	
	
	show_subscreen_dmap_dots=true;
	show_subscreen_items=true;
	show_subscreen_numbers=true;
	show_subscreen_life=true;
	
	Playing=true;
	
	map_bkgsfx(true);
	
	
	
	//if(firstplay)
	//{
	//	memset(game->screen_d, 0, MAXDMAPS * 64 * 8 * sizeof(int32_t));
	//	ZScriptVersion::RunScript(SCRIPT_GLOBAL, GLOBAL_SCRIPT_INIT, GLOBAL_SCRIPT_INIT);
	//ZScriptVersion::RunScript(SCRIPT_PLAYER, SCRIPT_PLAYER_INIT, SCRIPT_PLAYER_INIT);
	//}
	//else
	//if(!firstplay && !get_bit(quest_rules, qr_OLD_INIT_SCRIPT_TIMING))
	//{
	//	ZScriptVersion::RunScript(SCRIPT_GLOBAL, GLOBAL_SCRIPT_ONSAVELOAD, GLOBAL_SCRIPT_ONSAVELOAD); //Do this after global arrays have been loaded
	//	FFCore.deallocateAllArrays(SCRIPT_GLOBAL, GLOBAL_SCRIPT_ONSAVELOAD);
	//}
	//Run after Init/onSaveLoad, regardless of firstplay -V
	FFCore.runOnLaunchEngine();
	FFCore.deallocateAllArrays(SCRIPT_GLOBAL, GLOBAL_SCRIPT_ONLAUNCH);
	
	
	if ( Hero.getDontDraw() < 2 ) { Hero.setDontDraw(0); }
	openscreen();
	show_subscreen_numbers=true;
	show_subscreen_life=true;
	dointro();
		if(!(tmpscr->room==rGANON && !get_bit(quest_rules, qr_GANON_CANT_SPAWN_ON_CONTINUE)))
	{
		loadguys();
	}

	
	activated_timed_warp=false;
	newscr_clk = frame;
	
	if(isdungeon() && currdmap>0) // currdmap>0 is weird, but at least one quest (Mario's Insane Rampage) depends on it
	{
		Hero.stepforward(get_bit(quest_rules,qr_LTTPWALK) ? 11: 12, false);
	}
	
	if(!Quit)
	{
		if(tmpscr->room==rGANON && !get_bit(quest_rules, qr_GANON_CANT_SPAWN_ON_CONTINUE))
		{
			Hero.ganon_intro();
		}
		else playLevelMusic();
	}

	 
	//2.53 timing
	if(get_bit(quest_rules, qr_OLD_INIT_SCRIPT_TIMING))
	{
		if(firstplay)
		{
			memset(game->screen_d, 0, MAXDMAPS * 64 * 8 * sizeof(int32_t));
			ZScriptVersion::RunScript(SCRIPT_GLOBAL, GLOBAL_SCRIPT_INIT);
			if(!get_bit(quest_rules, qr_DO_NOT_DEALLOCATE_INIT_AND_SAVELOAD_ARRAYS) ) FFCore.deallocateAllArrays(SCRIPT_GLOBAL, GLOBAL_SCRIPT_INIT); //Deallocate LOCAL arrays declared in the init script. This function does NOT deallocate global arrays.
		}
		else
		{
			ZScriptVersion::RunScript(SCRIPT_GLOBAL, GLOBAL_SCRIPT_ONSAVELOAD); //Do this after global arrays have been loaded
			if(!get_bit(quest_rules, qr_DO_NOT_DEALLOCATE_INIT_AND_SAVELOAD_ARRAYS) ) FFCore.deallocateAllArrays(SCRIPT_GLOBAL, GLOBAL_SCRIPT_ONSAVELOAD);
		}	
	}
	
	initZScriptGlobalScript(GLOBAL_SCRIPT_GAME);
	FFCore.initZScriptHeroScripts(); //Call again so we're set up for GLOBAL_SCRIPT_GAME
	FFCore.initZScriptDMapScripts(); //Call again so we're set up for GLOBAL_SCRIPT_GAME
	FFCore.initZScriptItemScripts(); //Call again so we're set up for GLOBAL_SCRIPT_GAME
	FFCore.initZScriptActiveSubscreenScript();
	ffscript_engine(true);  //Here is a much safer place...
	
	return 0;
}

int32_t cont_game()
{
	//  introclk=intropos=msgclk=msgpos=dmapmsgclk=0;
	FFCore.init();
	didpit=false;
	Hero.unfreeze();
	Hero.reset_hookshot();
	Hero.reset_ladder();
	linkedmsgclk=0;
	blockmoving=0;
	add_asparkle=0;
	add_bsparkle=0;
	add_df1asparkle=false;
	add_df1bsparkle=false;
	add_nl1asparkle=false;
	add_nl1bsparkle=false;
	add_nl2asparkle=false;
	add_nl2bsparkle=false;
	/*
	  if(DMaps[currdmap].cont >= 0x80)
	  {
	  homescr = currscr = DMaps[0].cont;
	  currdmap = warpscr = worldscr=0;
	  currmap = DMaps[0].map;
	  dlevel = DMaps[0].level;
	  }
	  */
	currdmap = lastentrance_dmap;
	homescr = currscr = lastentrance;
	currmap = DMaps[currdmap].map;
	dlevel = DMaps[currdmap].level;
	init_dmap();
	
	for(int32_t i=0; i<6; i++)
	{
		visited[i]=-1;
	}
	
	if(dlevel==0)
	{
		game->lvlitems[9]&=~liBOSS;
	}
	
	ALLOFF();
	whistleclk=-1;
	currcset=DMaps[currdmap].color;
	darkroom=naturaldark=false;
	tmpscr[0].zero_memory();
	tmpscr[1].zero_memory();
	
//loadscr(0,currscr,up);
	loadscr(0,currdmap,currscr,-1,false);
	putscr(scrollbuf,0,0,&tmpscr[0]);
	putscrdoors(scrollbuf,0,0,&tmpscr[0]);
	script_drawing_commands.Clear();
	
	//preloaded freeform combos
	ffscript_engine(true);
	
	loadfullpal();
	ringcolor(false);
	loadlvlpal(DMaps[currdmap].color);
	lighting(false,true);
	Hero.init();
	if(zqtesting_mode
		&& currscr == testingqst_screen
		&& currdmap == testingqst_dmap)
	{
		Hero.setX(tmpscr->warpreturnx[testingqst_retsqr]);
		Hero.setY(tmpscr->warpreturny[testingqst_retsqr]);
	}
	if(DMaps[currdmap].flags&dmfBUNNYIFNOPEARL)
	{
		int32_t itemid = current_item_id(itype_pearl);
		if(itemid < 0)
		{
			Hero.setBunnyClock(-1);
		}
	}
	
	wavy=quakeclk=0;
	
	//if(get_bit(zinit.misc,idM_CONTPERCENT))
	if(game->get_cont_percent())
	{
		if(game->get_maxlife()%game->get_hp_per_heart()==0)
			game->set_life(((game->get_maxlife()*game->get_cont_hearts()/100)/game->get_hp_per_heart())*game->get_hp_per_heart());
		else
			game->set_life(game->get_maxlife()*game->get_cont_hearts()/100);
	}
	else
	{
		game->set_life(game->get_cont_hearts()*game->get_hp_per_heart());
	}
		
	/*
	  else
	  game->life=3*game->get_hp_per_heart();
	  */
	
	//  for(int32_t i=0; i<128; i++)
	//	key[i]=0;
	
	//Run onContGame script -V
	initZScriptGlobalScript(GLOBAL_SCRIPT_ONCONTGAME);
	ZScriptVersion::RunScript(SCRIPT_GLOBAL, GLOBAL_SCRIPT_ONCONTGAME, GLOBAL_SCRIPT_ONCONTGAME);	
	FFCore.deallocateAllArrays(SCRIPT_GLOBAL, GLOBAL_SCRIPT_ONCONTGAME);
	
	initZScriptGlobalScript(GLOBAL_SCRIPT_GAME);
	FFCore.initZScriptHeroScripts();
	FFCore.initZScriptDMapScripts();
	FFCore.initZScriptActiveSubscreenScript();
	FFCore.initZScriptItemScripts();
	
	update_subscreens();
	Playing=true;
	map_bkgsfx(true);
	openscreen();
	show_subscreen_numbers=true;
	show_subscreen_life=true;
	dointro();
	if(!(tmpscr->room==rGANON && !get_bit(quest_rules, qr_GANON_CANT_SPAWN_ON_CONTINUE)))
	{
		loadguys();
	}

	
	if(!Quit)
	{
		//play_DmapMusic();
		if(!(tmpscr->room==rGANON && !get_bit(quest_rules, qr_GANON_CANT_SPAWN_ON_CONTINUE))) playLevelMusic();
		
		if(isdungeon())
			Hero.stepforward(get_bit(quest_rules,qr_LTTPWALK)?11:12, false);
			
		newscr_clk=frame;
		activated_timed_warp=false;
		if(tmpscr->room==rGANON && !get_bit(quest_rules, qr_GANON_CANT_SPAWN_ON_CONTINUE))
		{
			Hero.ganon_intro();
		}
	}
	
	return 0;
}

void restart_level()
{
	blackscr(16,true);
	map_bkgsfx(false);
	
	if(dlevel)
	{
		currdmap = lastentrance_dmap;
		homescr = currscr = lastentrance;
		init_dmap();
	}
	else
	{
		if((DMaps[currdmap].type&dmfTYPE)==dmOVERW)
		{
			homescr = currscr = DMaps[currdmap].cont;
		}
		else
		{
			homescr = currscr = DMaps[currdmap].cont + DMaps[currdmap].xoff;
		}
	}
	
	currmap = DMaps[currdmap].map;
	dlevel = DMaps[currdmap].level;
	
	for(int32_t i=0; i<6; i++)
		visited[i]=-1;
		
	ALLOFF();
	whistleclk=-1;
	darkroom=naturaldark=false;
	tmpscr[0].zero_memory();
	tmpscr[1].zero_memory();
	
	loadscr(0,currdmap,currscr,-1,false);
	putscr(scrollbuf,0,0,&tmpscr[0]);
	putscrdoors(scrollbuf,0,0,&tmpscr[0]);
	//preloaded freeform combos
	ffscript_engine(true);
	
	loadfullpal();
	ringcolor(false);
	loadlvlpal(DMaps[currdmap].color);
	lighting(false,true);
	Hero.init();
	if(DMaps[currdmap].flags&dmfBUNNYIFNOPEARL)
	{
		int32_t itemid = current_item_id(itype_pearl);
		if(itemid < 0)
		{
			Hero.setBunnyClock(-1);
		}
	}
	
	currcset=DMaps[currdmap].color;
	openscreen();
	map_bkgsfx(true);
	Hero.set_respawn_point();
	Hero.trySideviewLadder();
	show_subscreen_numbers=true;
	show_subscreen_life=true;
	if(!(tmpscr->room==rGANON && !get_bit(quest_rules, qr_GANON_CANT_SPAWN_ON_CONTINUE)))
	{
		loadguys();
	}
	
	if(!Quit)
	{
		//play_DmapMusic();
		if(!(tmpscr->room==rGANON && !get_bit(quest_rules, qr_GANON_CANT_SPAWN_ON_CONTINUE))) playLevelMusic();
		
		if(isdungeon())
			Hero.stepforward(get_bit(quest_rules,qr_LTTPWALK)?11:12, false);
			
		newscr_clk=frame;
		activated_timed_warp=false;
		if(tmpscr->room==rGANON && !get_bit(quest_rules, qr_GANON_CANT_SPAWN_ON_CONTINUE))
		{
			Hero.ganon_intro();
		}
	}
}


void putintro()
{
    if(!stricmp("                                                                        ", DMaps[currdmap].intro))
    {
        introclk=intropos=72;
        return;
    }
    
    if((cBbtn())&&(get_bit(quest_rules,qr_ALLOWMSGBYPASS)))
    {
        //finish writing out the string
        for(; intropos<72; ++intropos)
        {
            textprintf_ex(msg_txt_display_buf,zfont,((intropos%24)<<3)+32,((intropos/24)<<3)+40,QMisc.colors.msgtext,-1,
                          "%c",DMaps[currdmap].intro[intropos]);
        }
    }
    
    if(intropos>=72)
    {
        if(dmapmsgclk >= 51)
            dmapmsgclk=50;
            
        return;
    }
    
    if(((introclk++)%6<5)&&((!cAbtn())||(!get_bit(quest_rules,qr_ALLOWFASTMSG))))
        return;
        
    dmapmsgclk=51;
    
    if(intropos == 0)
    {
        while(DMaps[currdmap].intro[intropos]==' ')
            ++intropos;
    }
    
    sfx(WAV_MSG); //actual message display
    
    
    //using the clip value to indicate the bitmap is "dirty"
    //rather than add yet another global variable
    set_clip_state(msg_txt_display_buf, 0);
    textprintf_ex(msg_txt_display_buf,zfont,((intropos%24)<<3)+32,((intropos/24)<<3)+40,QMisc.colors.msgtext,-1,
                  "%c",DMaps[currdmap].intro[intropos]);
                  
    ++intropos;
    
    if(DMaps[currdmap].intro[intropos]==' ' && DMaps[currdmap].intro[intropos+1]==' ')
        while(DMaps[currdmap].intro[intropos]==' ')
            ++intropos;
}

//static char *dirstr[4] = {"Up","Down","Left","Right"};
//static char *dirstr[32] = {"U","D","L","R"," "," "," "," "," "," "," "," "," "," "," "," "," "," "," "," "," "," "," "," "," "," "," "," "," "," "," "," "};

//use detail_int[x] for global detail info
void show_details()
{
    //textprintf_ex(framebuf,font,-3,-5,WHITE,BLACK,"%-4d",whistleclk);
    textprintf_ex(framebuf,font,0,8,WHITE,BLACK,"dlvl:%-2d dngn:%d", dlevel, isdungeon());
    textprintf_ex(framebuf,font,0,176,WHITE,BLACK,"%ld %s",game->get_time(),time_str_long(game->get_time()));
    
//    textprintf_ex(framebuf,font,200,16,WHITE,BLACK,"%3d",Hero.getPushing());
//    for(int32_t i=0; i<Lwpns.Count(); i++)
//      textprintf_ex(framebuf,font,200,(i<<3)+16,WHITE,BLACK,"%3d",items.spr(i)->id);

//    for(int32_t i=0; i<Ewpns.Count(); i++)
//      textprintf_ex(framebuf,font,90,(i<<3)+16,WHITE,BLACK,"%3d %3d %3d %3d %3d",((weapon*)Ewpns.spr(i))->id, ((weapon*)Ewpns.spr(i))->tile, ((weapon*)Ewpns.spr(i))->clk, ((weapon*)Ewpns.spr(i))->aframe, wpnsbuf[((weapon*)Ewpns.spr(i))->id].frames);

//    for(int32_t i=0; i<items.Count(); i++)
//      textprintf_ex(framebuf,font,90,(i<<3)+16,WHITE,BLACK,"%3d %3d %3d",((weapon*)Lwpns.spr(i))->tile, ((weapon*)Lwpns.spr(i))->dir, ((weapon*)Lwpns.spr(i))->flip);

    for(int32_t i=0; i<guys.Count(); i++)
        textprintf_ex(framebuf,font,90,(i<<3)+16,WHITE,BLACK,"%d",(int32_t)((enemy*)guys.spr(i))->id);
        
//      textprintf_ex(framebuf,font,90,16,WHITE,BLACK,"%3d, %3d",int32_t(HeroModifiedX()),int32_t(HeroModifiedY()));
    //textprintf_ex(framebuf,font,90,24,WHITE,BLACK,"%3d, %3d",detail_int[0],detail_int[1]);
//      textprintf_ex(framebuf,font,200,16,WHITE,BLACK,"%3d",Hero.getAction());

    /*
      for(int32_t i=0; i<Ewpns.Count(); i++)
      {
      sprite *s=Ewpns.spr(i);
      textprintf_ex(framebuf,font,100,(i<<3)+16,WHITE,BLACK,"%3d>%3d %3d>%3d %3d<%3d %3d<%3d ",
      int32_t(Hero.getX()+0+16), int32_t(s->x+s->hxofs),  int32_t(Hero.getY()+0+16), int32_t(s->y+s->hyofs),
      int32_t(Hero.getX()+0), int32_t(s->x+s->hxofs+s->hxsz), int32_t(Hero.getY()+0), int32_t(s->y+s->hyofs+s->hysz));
      }
      */
//        textprintf_ex(framebuf,font,200,16,WHITE,BLACK,"gi=%3d",guycarryingitem);
}

void show_ffscript_names()
{
    int32_t ypos = 8;
    
    for(int32_t i=0; i< MAXFFCS; i++)
    {
        if(tmpscr->ffscript[i])
        {
            textout_shadowed_ex(framebuf,font, ffcmap[tmpscr->ffscript[i]-1].scriptname.c_str(),2,ypos,WHITE,BLACK,-1);
            ypos+=12;
        }
    }
}

void do_magic_casting()
{
    static int32_t tempx, tempy;
    static byte herotilebuf[256];
    int32_t ltile=0;
    int32_t lflip=0;
    bool shieldModify=true;
    
    if(magicitem==-1)
    {
        return;
    }
    
    switch(itemsbuf[magicitem].family)
    {
    case itype_dinsfire:
    {
        if(magiccastclk==0)
        {
            Lwpns.add(new weapon(HeroX(),HeroY(),HeroZ(),wPhantom,pDINSFIREROCKET,0,up, magicitem, Hero.getUID()));
            weapon *w1 = (weapon*)(Lwpns.spr(Lwpns.Count()-1));
            w1->step=4;
            //          Hero.tile=(BSZ)?32:29;
            herotile(&Hero.tile, &Hero.flip, &Hero.extend, Hero.IsSideSwim()?ls_sidewaterhold2:ls_landhold2, Hero.getDir(), zinit.heroAnimationStyle);
            
            if(get_bit(quest_rules,qr_EXPANDEDLTM))
            {
                Hero.tile+=Hero.getTileModifier();
            }
            
            casty=Hero.getY();
        }
        
        if(magiccastclk==64)
        {
            Lwpns.add(new weapon((zfix)HeroX(),(zfix)(-32),(zfix)HeroZ(),wPhantom,pDINSFIREROCKETRETURN,0,down, magicitem, Hero.getUID()));
            weapon *w1 = (weapon*)(Lwpns.spr(Lwpns.Count()-1));
            w1->step=4;
            //          Hero.tile=29;
            herotile(&Hero.tile, &Hero.flip, &Hero.extend, Hero.IsSideSwim()?ls_sidewaterhold2:ls_landhold2, Hero.getDir(), zinit.heroAnimationStyle);
            
            if(get_bit(quest_rules,qr_EXPANDEDLTM))
            {
                Hero.tile+=Hero.getTileModifier();
            }
            
            castnext=false;
        }
        
        if(castnext)
        {
            herotile(&Hero.tile, &Hero.flip, &Hero.extend, Hero.IsSideSwim()?ls_sideswimcast:ls_cast, Hero.getDir(), zinit.heroAnimationStyle);
            
            if(get_bit(quest_rules,qr_EXPANDEDLTM))
            {
                Hero.tile+=Hero.getTileModifier();
            }
            
            if(get_bit(quest_rules,qr_MORESOUNDS))
                sfx(itemsbuf[magicitem].usesound,pan(int32_t(Hero.getX())));
                
            int32_t flamemax=itemsbuf[magicitem].misc1;
            
            for(int32_t flamecounter=((-1)*(flamemax/2))+1; flamecounter<=((flamemax/2)+1); flamecounter++)
            {
		    //din't fire level fix to go here
                //Lwpns.add(new weapon((zfix)HeroX(),(zfix)HeroY(),(zfix)HeroZ(),wFire,3,itemsbuf[magicitem].power*game->get_hero_dmgmult(),
                Lwpns.add(new weapon((zfix)HeroX(),(zfix)HeroY(),(zfix)HeroZ(),wFire,itemsbuf[magicitem].fam_type,itemsbuf[magicitem].power*game->get_hero_dmgmult(),
                                     isSideViewGravity() ? (flamecounter<flamemax ? left : right) : 0, magicitem, Hero.getUID(), false, 0, 0, 0, itemsbuf[magicitem].family));
                weapon *w = (weapon*)(Lwpns.spr(Lwpns.Count()-1));
                w->step=(itemsbuf[magicitem].misc2/100.0);
                w->angular=true;
                w->angle=(flamecounter*PI/(flamemax/2.0));
            }
            
            castnext=false;
            magiccastclk=128;
        }
        
        if((magiccastclk++)>=226)
        {
            magicitem=-1;
            magiccastclk=0;
        }
    }
    break;
    
    case itype_faroreswind:
    {
        if(magiccastclk==0)
        {
            herotile(&ltile, &lflip, Hero.IsSideSwim()?ls_sideswimstab:ls_stab, down, zinit.heroAnimationStyle);
            
            if(get_bit(quest_rules,qr_EXPANDEDLTM))
            {
                ltile+=Hero.getTileModifier();
            }
            
            unpack_tile(newtilebuf, ltile, lflip, true);
            memcpy(herotilebuf, unpackbuf, 256);
            tempx=Hero.getX();
            tempy=Hero.getY();
            herotile(&Hero.tile, &Hero.flip, &Hero.extend, Hero.IsSideSwim()?ls_sideswimpound:ls_pound, down, zinit.heroAnimationStyle);
            
            if(get_bit(quest_rules,qr_EXPANDEDLTM))
            {
                Hero.tile+=Hero.getTileModifier();
            }
        }
        
        if(magiccastclk>=0&&magiccastclk<64)
        {
            Hero.setX(tempx+((zc_oldrand()%3)-1));
            Hero.setY(tempy+((zc_oldrand()%3)-1));
        }
        
        if(magiccastclk==64)
        {
            Hero.setX(tempx);
            Hero.setY(tempy);
            herotile(&Hero.tile, &Hero.flip, &Hero.extend, Hero.IsSideSwim()?ls_sideswimstab:ls_stab, down, zinit.heroAnimationStyle);
            
            if(get_bit(quest_rules,qr_EXPANDEDLTM))
            {
                Hero.tile+=Hero.getTileModifier();
            }
        }
        
        if(magiccastclk==96)
        {
            if(get_bit(quest_rules,qr_MORESOUNDS))
                sfx(itemsbuf[magicitem].usesound,pan(int32_t(Hero.getX())));
                
            if ( Hero.getDontDraw() < 2 ) { Hero.setDontDraw(1); }
            
            for(int32_t i=0; i<16; ++i)
            {
                for(int32_t j=0; j<16; ++j)
                {
                    if(herotilebuf[i*16+j])
                    {
                        if(itemsbuf[magicitem].misc1==1)  // Twilight
                        {
                            particles.add(new pTwilight(Hero.getX()+j, Hero.getY()-Hero.getZ()+i, 5, 0, 0, (zc_oldrand()%8)+i*4));
                            int32_t k=particles.Count()-1;
                            particle *p = (particles.at(k));
                            p->step=3;
                        }
                        else if(itemsbuf[magicitem].misc1==2)  // Sands of Hours
                        {
                            particles.add(new pTwilight(Hero.getX()+j, Hero.getY()-Hero.getZ()+i, 5, 1, 2, (zc_oldrand()%16)+i*2));
                            int32_t k=particles.Count()-1;
                            particle *p = (particles.at(k));
                            p->step=4;
                            
                            if(zc_oldrand()%10 < 2)
                            {
                                p->color=1;
                                p->cset=0;
                            }
                        }
                        else
                        {
                            particles.add(new pFaroresWindDust(Hero.getX()+j, Hero.getY()-Hero.getZ()+i, 5, 6, herotilebuf[i*16+j], zc_oldrand()%96));
                            
                            int32_t k=particles.Count()-1;
                            particle *p = (particles.at(k));
                            p->angular=true;
                            p->angle=zc_oldrand();
                            p->step=(((double)j)/8);
                            p->yofs=Hero.getYOfs();
                        }
                    }
                }
            }
        }
        
        if((magiccastclk++)>=226)
        {
            //attackclk=0;
            int32_t nayrutemp=nayruitem;
            restart_level();
            nayruitem=nayrutemp;
            //xofs=0;
            //action=none;
            magicitem=-1;
            magiccastclk=0;
            if ( Hero.getDontDraw() < 2 ) { Hero.setDontDraw(0); }
        }
    }
    break;
    
    case itype_nayruslove:
    {
        // See also hero.cpp, HeroClass::checkhit().
        if(magiccastclk==0)
        {
            Lwpns.add(new weapon(HeroX(),HeroY(),(zfix)0,wPhantom,pNAYRUSLOVEROCKET1,0,left, magicitem, Hero.getUID()));
            weapon *w1 = (weapon*)(Lwpns.spr(Lwpns.Count()-1));
            w1->step=4;
            Lwpns.add(new weapon(HeroX(),HeroY(),(zfix)0,wPhantom,pNAYRUSLOVEROCKET2,0,right, magicitem, Hero.getUID()));
            w1 = (weapon*)(Lwpns.spr(Lwpns.Count()-1));
            w1->step=4;
            //          Hero.tile=(BSZ)?32:29;
            herotile(&Hero.tile, &Hero.flip, &Hero.extend, Hero.IsSideSwim()?ls_sideswimcast:ls_cast, Hero.getDir(), zinit.heroAnimationStyle);
            
            if(get_bit(quest_rules,qr_EXPANDEDLTM))
            {
                Hero.tile+=Hero.getTileModifier();
            }
            
            castx=Hero.getX();
        }
        
        if(magiccastclk==64)
        {
            for(int32_t i=0; i<Lwpns.Count(); i++)
            {
                weapon* w=static_cast<weapon*>(Lwpns.spr(i));
                if(w->id==wPhantom &&
                  w->type>=pNAYRUSLOVEROCKET1 && w->type<=pNAYRUSLOVEROCKETTRAILRETURN2)
                    Lwpns.del(i);
            }
            
            int32_t d=zc_max(HeroX(),256-HeroX())+32;
            Lwpns.add(new weapon((zfix)(HeroX()-d),(zfix)HeroY(),(zfix)HeroZ(),wPhantom,pNAYRUSLOVEROCKETRETURN1,0,right, magicitem,Hero.getUID()));
            weapon *w1 = (weapon*)(Lwpns.spr(Lwpns.Count()-1));
            w1->step=4;
            Lwpns.add(new weapon((zfix)(HeroX()+d),(zfix)HeroY(),(zfix)HeroZ(),wPhantom,pNAYRUSLOVEROCKETRETURN2,0,left, magicitem,Hero.getUID()));
            w1 = (weapon*)(Lwpns.spr(Lwpns.Count()-1));
            w1->step=4;
            //          Hero.tile=29;
            herotile(&Hero.tile, &Hero.flip, &Hero.extend, Hero.IsSideSwim()?ls_sideswimcast:ls_cast, Hero.getDir(), zinit.heroAnimationStyle);
            
            if(get_bit(quest_rules,qr_EXPANDEDLTM))
            {
                Hero.tile+=Hero.getTileModifier();
            }
            
            castnext=false;
        }
        
        if(castnext)
        {
            //          Hero.tile=4;
            herotile(&Hero.tile, &Hero.flip, &Hero.extend, Hero.IsSideSwim()?ls_sidewaterhold2:ls_landhold2, Hero.getDir(), zinit.heroAnimationStyle);
            
            if(get_bit(quest_rules,qr_EXPANDEDLTM))
            {
                Hero.tile+=Hero.getTileModifier();
            }
            
            Hero.setNayrusLoveShieldClk(itemsbuf[magicitem].misc1);
            
            if(get_bit(quest_rules,qr_MORESOUNDS))
            {
                if(nayruitem != -1)
                {
                    stop_sfx(itemsbuf[nayruitem].usesound+1);
                    stop_sfx(itemsbuf[nayruitem].usesound);
                }
                
                cont_sfx(itemsbuf[magicitem].usesound);
            }
            
            castnext=false;
            magiccastclk=128;
            nayruitem = magicitem;
        }
        
        // Finish the final spell pose
        if((magiccastclk++)>=160)
        {
            for(int32_t i=0; i<Lwpns.Count(); i++)
            {
                weapon* w=static_cast<weapon*>(Lwpns.spr(i));
                if(w->id==wPhantom &&
                  w->type>=pNAYRUSLOVEROCKET1 && w->type<=pNAYRUSLOVEROCKETTRAILRETURN2)
                    Lwpns.del(i);
            }
            
            magicitem=-1;
            magiccastclk=0;
        }
    }
    break;
    
    default:
        magiccastclk=0;
        break;
    }
}

void update_hookshot()
{
    int32_t hs_x, hs_y, hs_z,hs_dx, hs_dy;
    bool check_hs=false;
    int32_t dist_bx, dist_by, hs_w;
    chainlinks.animate();
    //  char tempbuf[80];
    //  char tempbuf2[80];
    
    //find out where the head is and make it
    //easy to reference
    if(Lwpns.idFirst(wHookshot)>-1)
    {
        check_hs=true;
    }
    
    if(check_hs)
    {
		weapon* hookweap = (weapon*)Lwpns.spr(Lwpns.idFirst(wHookshot));
        int32_t parentitem = hookweap->parentitem;
        hs_x=hookweap->x;
        hs_y=hookweap->y;
        hs_z=hookweap->z;
        hs_dx=hs_x-hs_startx;
        hs_dy=hs_y-hs_starty;
        
        //extending
        if(hookweap->misc==0)
        {
            int32_t maxchainlinks=itemsbuf[parentitem].misc2;
            
            if(chainlinks.Count()<maxchainlinks)           //extending chain
            {
                if(abs(hs_dx)>=hs_xdist+8)
                {
                    hs_xdist=abs(hs_x-hs_startx);
                    chainlinks.add(new weapon((zfix)hs_x, (zfix)hs_y, (zfix)hs_z,wHSChain, 0,0,Hero.getDir(), parentitem,Hero.getUID()));
                }
                else if(abs(hs_dy)>=hs_ydist+8)
                {
                    hs_ydist=abs(hs_y-hs_starty);
                    chainlinks.add(new weapon((zfix)hs_x, (zfix)hs_y, (zfix)hs_z,wHSChain, 0,0,Hero.getDir(), parentitem,Hero.getUID()));
                }
            }                                                     //stretching chain
            else
            {
                dist_bx=(abs(hs_dx)-(8*chainlinks.Count()))/(chainlinks.Count()+1);
                dist_by=(abs(hs_dy)-(8*chainlinks.Count()))/(chainlinks.Count()+1);
                hs_w=8;
                
                if(hs_dx<0)
                {
                    dist_bx=0-dist_bx;
                    hs_w=-8;
                }
                
                if(hs_dy<0)
                {
                    dist_by=0-dist_by;
                    hs_w=-8;
                }
                
                for(int32_t counter=0; counter<chainlinks.Count(); counter++)
                {
                    if(Hero.getDir()>down)                            //chain is moving horizontally
                    {
                        chainlinks.spr(counter)->x=hs_startx+hs_w+dist_bx+(counter*(hs_w+dist_bx));
                    }
                    else
                    {
                        chainlinks.spr(counter)->y=hs_starty+hs_w+dist_by+(counter*(hs_w+dist_by));
                    }
                }
            }
        }                                                       //retracting
        else if(hookweap->misc==1)
        {
            dist_bx=(abs(hs_dx)-(8*chainlinks.Count()))/(chainlinks.Count()+1);
            dist_by=(abs(hs_dy)-(8*chainlinks.Count()))/(chainlinks.Count()+1);
            hs_w=8;
            
            if(hs_dx<0)
            {
                dist_bx=0-dist_bx;
                hs_w=-8;
            }
            
            if(hs_dy<0)
            {
                dist_by=0-dist_by;
                hs_w=-8;
            }
            
            /* With ZScript modification, chains can conceivably move diagonally.*/
            //if (Hero.getDir()>down)                               //chain is moving horizontally
            {
                if(abs(hs_dx)-(8*chainlinks.Count())>0)             //chain is stretched
                {
                    for(int32_t counter=0; counter<chainlinks.Count(); counter++)
                    {
                        chainlinks.spr(counter)->x=hs_startx+hs_w+dist_bx+(counter*(hs_w+dist_bx));
                    }
                }
                else
                {
                    if(abs(hs_x-hs_startx)<=hs_xdist-8)
                    {
                        hs_xdist=abs(hs_x-hs_startx);
                        
                        if(pull_hero==false)
                        {
                            chainlinks.del(chainlinks.idLast(wHSChain));
                        }
                        else
                        {
                            chainlinks.del(chainlinks.idFirst(wHSChain));
                        }
                    }
                }
            }                                                     //chain is moving vertically
            //else
            {
                if(abs(hs_dy)-(8*chainlinks.Count())>0)             //chain is stretched
                {
                    for(int32_t counter=0; counter<chainlinks.Count(); counter++)
                    {
                        chainlinks.spr(counter)->y=hs_starty+hs_w+dist_by+(counter*(hs_w+dist_by));
                    }
                }
                else
                {
                    if(abs(hs_y-hs_starty)<=hs_ydist-8)
                    {
                        hs_ydist=abs(hs_y-hs_starty);
                        
                        if(pull_hero==false)
                        {
                            chainlinks.del(chainlinks.idLast(wHSChain));
                        }
                        else
                        {
                            chainlinks.del(chainlinks.idFirst(wHSChain));
                        }
                    }
                }
            }
        }
    }
}

void do_dcounters()
{
    for(int32_t i=0; i<32; i++)
    {
        if(game->get_dcounter(i)==0)
        {
            continue;
        }
        
		byte sfx_to_use = 0;
        if(frame&1)
        {
            if(game->get_dcounter(i)>0)
            {
				sfx_to_use = QMisc.miscsfx[sfxREFILL];
                int32_t drain = (i==4 ? game->get_mp_per_block()/4 : 1);
                drain = zc_min(game->get_dcounter(i),drain);
                
                if(game->get_counter(i) < game->get_maxcounter(i))
                {
                    game->change_counter(drain, i);
                    game->change_dcounter(-drain, i);
                    
                    if(game->get_dcounter(i) < 0)  // Better safe
                        game->set_dcounter(0, i);
                }
                else
                {
                    game->set_dcounter(0, i);
                    game->set_counter(game->get_maxcounter(i), i);
                }
            }
            else
            {
                if(i==1)   // Only rupee drain is sounded
                    sfx_to_use = QMisc.miscsfx[sfxDRAIN];;
                    
                int32_t drain = (i==4 ? 2*game->get_magicdrainrate() : 1);
                
                if(game->get_counter(i)>0)
                {
                    game->change_counter(-drain, i);
                    game->change_dcounter(drain, i);
                    
                    if(game->get_dcounter(i) > 0)  // Better safe
                        game->set_dcounter(0, i);
                }
                else
                {
                    game->set_dcounter(0, i);
                    game->set_counter(0, i);
                }
            }
        }
        
        if((sfx_to_use) && !lensclk && (i<2 || i==4)) // Life, Rupees and Magic
            sfx(sfx_to_use);
    }
}
#define F7 46+7
//bool zasmstacktrace = false;
void game_loop()
{
	while(true)
	{
		GameFlags &= ~GAMEFLAG_RESET_GAME_LOOP;
		if((pause_in_background && callback_switchin && midi_patch_fix))
		{
			if(currmidi!=0)
			{
				if(callback_switchin == 2) 
				{
					if ( currmidi != 0 )
					{
						int32_t digi_vol, midi_vol;
					
						get_volume(&digi_vol, &midi_vol);
						stop_midi();
						jukebox(currmidi);
						set_volume(digi_vol, midi_vol);
						midi_seek(paused_midi_pos);
					}
					midi_paused=false;
					midi_suspended = midissuspNONE;
					callback_switchin = 0;
				}
				if(callback_switchin == 1) 
				{
					paused_midi_pos = midi_pos;
					midi_paused=true;
					stop_midi();
					++callback_switchin;
				}
			}
			else //no MIDI playing
			{
				callback_switchin = 0;
			}
		}
		else if(midi_suspended==midissuspRESUME )
		{
			if ( currmidi != 0 )
			{
				int32_t digi_vol, midi_vol;
			
				get_volume(&digi_vol, &midi_vol);
				stop_midi();
				jukebox(currmidi);
				set_volume(digi_vol, midi_vol);
				midi_seek(paused_midi_pos);
			}
			midi_paused=false;
			midi_suspended = midissuspNONE;
		}
		
		//  walkflagx=0; walkflagy=0;
		runDrunkRNG();
		clear_to_color(darkscr_bmp_curscr, game->get_darkscr_color());
		clear_to_color(darkscr_bmp_curscr_trans, game->get_darkscr_color());
		clear_to_color(darkscr_bmp_scrollscr, game->get_darkscr_color());
		clear_to_color(darkscr_bmp_scrollscr_trans, game->get_darkscr_color());
		
		// Three kinds of freezes: freeze, freezemsg, freezeff
		
		// freezemsg if message is being printed && qr_MSGFREEZE is on,
		// or if a message is being prepared && qr_MSGDISAPPEAR is on.
		bool freezemsg = ((msg_active || (intropos && intropos<72) || (linkedmsgclk && get_bit(quest_rules,qr_MSGDISAPPEAR)))
			&& (get_bit(quest_rules,qr_MSGFREEZE)));
		
		if(fadeclk>=0 && !freezemsg)
		{
			if(fadeclk==0 && currscr<128)
				blockpath=false;
				
			--fadeclk;
		}
		
		// Messages also freeze FF combos.
		bool freezeff = freezemsg;
		
		bool freeze = false;
		
		for(int32_t i=0; i<32; i++)
		{
			if(combobuf[tmpscr->ffdata[i]].type==cSCREENFREEZE) freeze=true;
			
			if(combobuf[tmpscr->ffdata[i]].type==cSCREENFREEZEFF) freezeff=true;
		}
		
		for(int32_t i=0; i<176; i++)
		{
			if(combobuf[tmpscr->data[i]].type == cSCREENFREEZE) freeze=true;
			
			if(combobuf[tmpscr->data[i]].type == cSCREENFREEZEFF) freezeff=true;
		}
		
		if(!freeze_guys && !freeze && !freezemsg && !FFCore.system_suspend[susptGUYS])
		{
			for(int32_t i=0; i<176; i++)
			{
				if(guygrid[i]>0)
				{
					--guygrid[i];
				}
			}
		}
		#if LOGGAMELOOP > 0
		al_trace("game_loop is calling: %s\n", "animate_combos()\n");
		#endif
		if ( !FFCore.system_suspend[susptCOMBOANIM] ) animate_combos();
		#if LOGGAMELOOP > 0
		al_trace("game_loop is calling: %s\n", "load_control_state()\n");
		#endif
		if ( !FFCore.system_suspend[susptCONTROLSTATE] ) load_control_state();
		
		if(!freezemsg)
		{
			if ( !FFCore.system_suspend[susptSCRIPDRAWCLEAR] ) script_drawing_commands.Clear();
		}
		
		if(!freezeff)
		{
			if ( !FFCore.system_suspend[susptUPDATEFFC] ) update_freeform_combos();
		}
		
		// Arbitrary Rule 637: neither 'freeze' nor 'freezeff' freeze the global script.
		if(!FFCore.system_suspend[susptGLOBALGAME] && !freezemsg && (g_doscript & (1<<GLOBAL_SCRIPT_GAME)))
		{
			ZScriptVersion::RunScript(SCRIPT_GLOBAL, GLOBAL_SCRIPT_GAME, GLOBAL_SCRIPT_GAME);
		}
		if(!FFCore.system_suspend[susptHEROACTIVE] && !freezemsg && player_doscript && FFCore.getQuestHeaderInfo(vZelda) >= 0x255)
		{
			ZScriptVersion::RunScript(SCRIPT_PLAYER, SCRIPT_PLAYER_ACTIVE, SCRIPT_PLAYER_ACTIVE);
		}
		if(!FFCore.system_suspend[susptDMAPSCRIPT] && !freezemsg && dmap_doscript && FFCore.getQuestHeaderInfo(vZelda) >= 0x255)
		{
			ZScriptVersion::RunScript(SCRIPT_DMAP, DMaps[currdmap].script,currdmap);
		}
		if(!FFCore.system_suspend[susptDMAPSCRIPT] && !freezemsg && passive_subscreen_doscript && FFCore.getQuestHeaderInfo(vZelda) >= 0x255)
		{
			ZScriptVersion::RunScript(SCRIPT_PASSIVESUBSCREEN, DMaps[currdmap].passive_sub_script,currdmap);
		}
		if ( !FFCore.system_suspend[susptCOMBOSCRIPTS] && !freezemsg && FFCore.getQuestHeaderInfo(vZelda) >= 0x255 )
		{
			FFCore.combo_script_engine(false);    
		}
		
		
		if(!freeze && !freezemsg)
		{
			#if LOGGAMELOOP > 0
			al_trace("game_loop is calling: %s\n", "mblock2.animate()\n");
			#endif
			if ( !FFCore.system_suspend[susptMOVINGBLOCKS] )  mblock2.animate(0);
			#if LOGGAMELOOP > 0
			al_trace("game_loop is calling: %s\n", "items.animate()\n");
			#endif
			if ( !FFCore.system_suspend[susptITEMSPRITESCRIPTS] )  FFCore.itemSpriteScriptEngine();
			if ( !FFCore.system_suspend[susptITEMS] ) items.animate();
		
			//Can't be called in items.animate(), as ZQuest also uses this function.
			#if LOGGAMELOOP > 0
			al_trace("game_loop is calling: %s\n", "items.check_conveyor()\n");
			#endif
			if ( !FFCore.system_suspend[susptCONVEYORSITEMS] ) items.check_conveyor();
			#if LOGGAMELOOP > 0
			al_trace("game_loop is calling: %s\n", "guys.animate()\n");
			#endif
			if ( !FFCore.system_suspend[susptGUYS] ) guys.animate();
			#if LOGGAMELOOP > 0
			al_trace("game_loop is calling: %s\n", "roaming_item()\n");
			#endif
			if ( !FFCore.system_suspend[susptROAMINGITEM] ) roaming_item();
			#if LOGGAMELOOP > 0
			al_trace("game_loop is calling: %s\n", "dragging_item()\n");
			#endif
			if ( !FFCore.system_suspend[susptDRAGGINGITEM] ) dragging_item();
			#if LOGGAMELOOP > 0
			al_trace("game_loop is calling: %s\n", "Ewpns.animate()\n");
			#endif
			if ( !FFCore.system_suspend[susptEWEAPONS] ) Ewpns.animate();
			if ( !FFCore.system_suspend[susptEWEAPONSCRIPTS] ) FFCore.eweaponScriptEngine();
			#if LOGGAMELOOP > 0
			al_trace("game_loop is setting: %s\n", "checkhero=true()\n");
			#endif
			
			#if LOGGAMELOOP > 0
			al_trace("game_loop is calling: %s\n", "clear_script_one_frame_conditions()\n");
			#endif
			
			if ( !FFCore.system_suspend[susptONEFRAMECONDS] )  clear_script_one_frame_conditions(); //clears npc->HitBy[] for this frame: the timing on this may need adjustment. 
			
			if ( get_bit(quest_rules, qr_OLD_ITEMDATA_SCRIPT_TIMING) && !FFCore.system_suspend[susptITEMSCRIPTENGINE] )
				FFCore.itemScriptEngine(); //run before lweapon scripts
			if ( !FFCore.system_suspend[susptHERO] )
			{
				for(int32_t i = 0; i < (gofast ? 8 : 1); i++)
				{
					#if LOGGAMELOOP > 0
					al_trace("game_loop is at: %s\n", "if(Hero.animate(0)\n");
					#endif
					if(Hero.animate(0))
					{
						if(!Quit)
						{
							//set a B item hack
							//Bwpn = Bweapon(Bpos);
							Quit = qGAMEOVER;
						}
						
						return;
					}
					if(GameFlags & GAMEFLAG_RESET_GAME_LOOP) break; //break the for()
				}
				if(GameFlags & GAMEFLAG_RESET_GAME_LOOP) continue; //continue the game_loop while(true)
			}
			if ( !get_bit(quest_rules, qr_OLD_ITEMDATA_SCRIPT_TIMING) && !FFCore.system_suspend[susptITEMSCRIPTENGINE] )
				FFCore.itemScriptEngine(); //run before lweapon scripts
			
			//FFCore.itemScriptEngine(); //run before lweapon scripts
			#if LOGGAMELOOP > 0
			al_trace("game_loop is calling: %s\n", "do_magic_casting()\n");
			#endif
			Hero.cleanupByrna(); //Prevent sfx glitches with Cane of Byrna if it fails to initialise; ported from 2.53. -Z
			if ( !FFCore.system_suspend[susptMAGICCAST] ) do_magic_casting();
			#if LOGGAMELOOP > 0
			al_trace("game_loop is calling: %s\n", "Lwpns.animate()\n");
			#endif
			//perhaps add sprite.waitdraw, and call sprite script here too?
			//FFCore.lweaponScriptEngine();
			if ( !FFCore.system_suspend[susptLWEAPONS] ) Lwpns.animate();
			
			//FFCore.lweaponScriptEngine();
			#if LOGGAMELOOP > 0
			al_trace("game_loop is calling: %s\n", "FFCore.itemScriptEngine())\n");
			#endif
			
			#if LOGGAMELOOP > 0
			al_trace("game_loop is calling: %s\n", "decorations.animate()\n");
			#endif
			if ( !FFCore.system_suspend[susptDECORATIONS] ) decorations.animate();
			#if LOGGAMELOOP > 0
			al_trace("game_loop is calling: %s\n", "particles.animate()\n");
			#endif
			if ( !FFCore.system_suspend[susptPARTICLES] ) particles.animate();
			#if LOGGAMELOOP > 0
			al_trace("game_loop is calling: %s\n", "update_hookshot()\n");
			#endif
			if ( !FFCore.system_suspend[susptHOOKSHOT] ) update_hookshot();
			
			if(conveyclk<=0)
			{
				conveyclk=3;
			}
			
			--conveyclk;
			#if LOGGAMELOOP > 0
			al_trace("game_loop is calling: %s\n", "check_collisions()\n");
			#endif
			if ( !FFCore.system_suspend[susptCOLLISIONS] ) check_collisions();
			#if LOGGAMELOOP > 0
			al_trace("game_loop is calling: %s\n", "dryuplake()\n");
			#endif
			if ( !FFCore.system_suspend[susptLAKES] ) dryuplake();
			#if LOGGAMELOOP > 0
			al_trace("game_loop is calling: %s\n", "cycle_palette()\n");
			#endif
			if ( !FFCore.system_suspend[susptPALCYCLE] ) cycle_palette();
		}
		else if(freezemsg)
		{
			for(int32_t i=0; i<guys.Count(); i++)
		{
				if(((enemy*)guys.spr(i))->ignore_msg_freeze())
			{
					if ( !FFCore.system_suspend[susptGUYS] ) guys.spr(i)->animate(i);
			}
			}
		}
		#if LOGGAMELOOP > 0
		al_trace("game_loop at: %s\n", "if(global_wait)\n");
		#endif
		if( !FFCore.system_suspend[susptGLOBALGAME] && (global_wait & (1<<GLOBAL_SCRIPT_GAME)) )
		{
			ZScriptVersion::RunScript(SCRIPT_GLOBAL, GLOBAL_SCRIPT_GAME, GLOBAL_SCRIPT_GAME);
			global_wait &= ~(1<<GLOBAL_SCRIPT_GAME);
		}
		if ( !FFCore.system_suspend[susptHEROACTIVE] && player_waitdraw && FFCore.getQuestHeaderInfo(vZelda) >= 0x255 )
		{
			ZScriptVersion::RunScript(SCRIPT_PLAYER, SCRIPT_PLAYER_ACTIVE, SCRIPT_PLAYER_ACTIVE);
			player_waitdraw = false;
		}
		if ( !FFCore.system_suspend[susptDMAPSCRIPT] && dmap_waitdraw && FFCore.getQuestHeaderInfo(vZelda) >= 0x255 )
		{
			ZScriptVersion::RunScript(SCRIPT_DMAP, DMaps[currdmap].script,currdmap);
			dmap_waitdraw = false;
		}
		if ( (!( FFCore.system_suspend[susptDMAPSCRIPT] )) && passive_subscreen_waitdraw && FFCore.getQuestHeaderInfo(vZelda) >= 0x255 )
		{
			ZScriptVersion::RunScript(SCRIPT_PASSIVESUBSCREEN, DMaps[currdmap].passive_sub_script,currdmap);
			passive_subscreen_waitdraw = false;
		}
		
		
		if ( !FFCore.system_suspend[susptSCREENSCRIPTS] && tmpscr->script != 0 && tmpscr->doscript && tmpscr->screen_waitdraw && FFCore.getQuestHeaderInfo(vZelda) >= 0x255 )
		{
		ZScriptVersion::RunScript(SCRIPT_SCREEN, tmpscr->script, 0);  
		tmpscr->screen_waitdraw = 0;	    
		}
		
		for ( int32_t q = 0; q < 32; ++q )
		{
		//Z_scripterrlog("tmpscr->ffcswaitdraw is: %d\n", tmpscr->ffcswaitdraw);
		if ( tmpscr->ffcswaitdraw&(1<<q) )
		{
			//Z_scripterrlog("FFC (%d) called Waitdraw()\n", q);
			if(tmpscr->ffscript[q] != 0 && !FFCore.system_suspend[susptFFCSCRIPTS] )
			{
				ZScriptVersion::RunScript(SCRIPT_FFC, tmpscr->ffscript[q], q);
				tmpscr->ffcswaitdraw &= ~(1<<q);
			}
		}
		}
		
		if ( !FFCore.system_suspend[susptCOMBOSCRIPTS] && FFCore.getQuestHeaderInfo(vZelda) >= 0x255 )
		{
			FFCore.combo_script_engine(false, true);    
		}
		
		//Waitdraw for item scripts. 
		if ( !FFCore.system_suspend[susptITEMSCRIPTENGINE] ) FFCore.itemScriptEngineOnWaitdraw();
		
		//Sprite scripts on Waitdraw in order of : npc, ewpn, lwpn, itemsprite
		if ( !FFCore.system_suspend[susptNPCSCRIPTS] ) FFCore.npcScriptEngineOnWaitdraw();
		if ( !FFCore.system_suspend[susptEWEAPONSCRIPTS] ) FFCore.eweaponScriptEngineOnWaitdraw();
		if ( !FFCore.system_suspend[susptLWEAPONSCRIPTS] ) FFCore.lweaponScriptEngineOnWaitdraw();
		if ( !FFCore.system_suspend[susptITEMSPRITESCRIPTS] ) FFCore.itemSpriteScriptEngineOnWaitdraw();
		
		
		
		
		
		#if LOGGAMELOOP > 0
		al_trace("game_loop is calling: %s\n", "draw_screen()\n");
		#endif
		if ( !FFCore.system_suspend[susptSCREENDRAW] ) draw_screen(tmpscr);
		
		//clear Hero's last hits 
		//for ( int32_t q = 0; q < 4; q++ ) Hero.sethitHeroUID(q, 0); //Clearing these here makes checking them fail both before and after waitdraw. 
		#if LOGGAMELOOP > 0
		al_trace("game_loop is at: %s\n", "if(linkedmsgclk)\n");
		#endif
		if(linkedmsgclk==1)
		{
		#if LOGGAMELOOP > 0
		al_trace("game_loop is calling: %s\n", "if(wpnsbuf[iwMore].newtile!=0)\n");
		#endif
			if(wpnsbuf[iwMore].newtile!=0)
			{
				putweapon(framebuf,zinit.msg_more_x, message_more_y(), wPhantom, 4, up, lens_hint_weapon[wPhantom][0], lens_hint_weapon[wPhantom][1],-1);
			}
		}
		
		if(!freeze)
		{
		#if LOGGAMELOOP > 0
		al_trace("game_loop is calling: %s\n", "putintro()\n");
		#endif
			putintro();
		}
		
		if(dmapmsgclk>0)
		{
			Hero.Freeze();
			
			if(dmapmsgclk<=50)
			{
				--dmapmsgclk;
			}
		}
		
		if(dmapmsgclk==1)
		{
			Hero.finishedmsg();
			dmapmsgclk=0;
			introclk=72;
			clear_bitmap(msg_bg_display_buf);
			set_clip_state(msg_bg_display_buf, 1);
			clear_bitmap(msg_txt_display_buf);
			set_clip_state(msg_txt_display_buf, 1);
			clear_bitmap(msg_portrait_display_buf);
			set_clip_state(msg_portrait_display_buf, 1);
			//    clear_bitmap(pricesdisplaybuf);
		}
		
		if(!freeze)
		{
			if(introclk==0 || (introclk>=72 && dmapmsgclk==0))
			{
				putmsg();
				
				if(msgstr)
				{
					set_clip_state(msg_bg_display_buf, 0);
					blit(msg_bg_bmp_buf, msg_bg_display_buf, 0, 0, msg_xpos, msg_ypos, msg_w+16, msg_h+16);
					set_clip_state(msg_txt_display_buf, 0);
					if(get_bit(quest_rules,qr_OLD_STRING_EDITOR_MARGINS)!=0)
					{
						blit(msg_txt_bmp_buf, msg_txt_display_buf, 0, 0, msg_xpos, msg_ypos, msg_w+16, msg_h+16);
						masked_blit(msg_menu_bmp_buf, msg_txt_display_buf, 0, 0, msg_xpos, msg_ypos, msg_w+16, msg_h+16);
					}
					else
					{
						blit(msg_txt_bmp_buf, msg_txt_display_buf, msg_margins[left], msg_margins[up], msg_xpos+msg_margins[left], msg_ypos+msg_margins[up], msg_w-msg_margins[left]-msg_margins[right], msg_h-msg_margins[up]-msg_margins[down]);
						masked_blit(msg_menu_bmp_buf, msg_txt_display_buf, msg_margins[left], msg_margins[up], msg_xpos+msg_margins[left], msg_ypos+msg_margins[up], msg_w-msg_margins[left]-msg_margins[right], msg_h-msg_margins[up]-msg_margins[down]);
					}
					set_clip_state(msg_portrait_display_buf, 0);
					blit(msg_portrait_bmp_buf, msg_portrait_display_buf, 0, 0, prt_x, prt_y, prt_tw*16, prt_th*16);
				}
			}
			#if LOGGAMELOOP > 0
		al_trace("game_loop is calling: %s\n", "do_dcounters()\n");
		#endif
			do_dcounters();
			
		#if LOGGAMELOOP > 0
		al_trace("game_loop is calling: %s\n", "if(!freezemsg && current_item(itype_heartring))\n");
		#endif
			if(!freezemsg && current_item(itype_heartring))
			{
				int32_t itemid = current_item_id(itype_heartring);
				int32_t fskip = itemsbuf[itemid].misc2;
				
				if(fskip == 0 || frame % fskip == 0)
					game->set_life(zc_min(game->get_life() + itemsbuf[itemid].misc1, game->get_maxlife()));
			}
			#if LOGGAMELOOP > 0
		al_trace("game_loop is calling: %s\n", "if(!freezemsg && current_item(itype_magicring))\n");
		#endif
			if(!freezemsg && current_item(itype_magicring))
			{
				int32_t itemid = current_item_id(itype_magicring);
				int32_t fskip = itemsbuf[itemid].misc2;
				
				if(fskip == 0 || frame % fskip == 0)
				{
					game->set_magic(zc_min(game->get_magic() + itemsbuf[itemid].misc1, game->get_maxmagic()));
				}
			}
			#if LOGGAMELOOP > 0
		al_trace("game_loop is calling: %s\n", "if(!freezemsg && current_item(itype_wallet))\n");
		#endif
			if(!freezemsg && current_item(itype_wallet))
			{
				int32_t itemid = current_item_id(itype_wallet);
				int32_t fskip = itemsbuf[itemid].misc2;
				
				if(fskip == 0 || frame % fskip == 0)
				{
					game->set_rupies(zc_min(game->get_rupies() + itemsbuf[itemid].misc1, game->get_maxcounter(1)));
				}
			}
			#if LOGGAMELOOP > 0
		al_trace("game_loop is calling: %s\n", "if(!freezemsg && current_item(itype_bombbag))\n");
		#endif
			if(!freezemsg && current_item(itype_bombbag))
			{
				int32_t itemid = current_item_id(itype_bombbag);
				
				if(itemsbuf[itemid].misc1)
				{
					int32_t fskip = itemsbuf[itemid].misc2;
					
					if(fskip == 0 || frame % fskip == 0)
					{
						game->set_bombs(zc_min(game->get_bombs() + itemsbuf[itemid].misc1, game->get_maxbombs()));
					}
					
					if(itemsbuf[itemid].flags & ITEM_FLAG1)
					{
						int32_t ratio = zinit.bomb_ratio;
						
						fskip = itemsbuf[itemid].misc2 * ratio;
						
						if(fskip == 0 || frame % fskip == 0)
						{
							game->set_sbombs(zc_min(game->get_sbombs() + zc_max(itemsbuf[itemid].misc1 / ratio, 1), game->get_maxbombs() / ratio));
						}
					}
				}
			}
			#if LOGGAMELOOP > 0
		al_trace("game_loop is calling: %s\n", "if(!freezemsg && current_item(itype_quiver))\n");
		#endif
			if(!freezemsg && current_item(itype_quiver) && game->get_arrows() != game->get_maxarrows())
			{
				int32_t itemid = current_item_id(itype_quiver);
				int32_t fskip = itemsbuf[itemid].misc2;
				
				if(fskip == 0 || frame % fskip == 0)
				{
					game->set_arrows(zc_min(game->get_arrows() + itemsbuf[itemid].misc1, game->get_maxarrows()));
				}
			}
			#if LOGGAMELOOP > 0
		al_trace("game_loop is calling: %s\n", "if(lensclk)\n");
		#endif
			if(lensclk && !FFCore.system_suspend[susptLENS])
			{
				draw_lens_over();
				--lensclk;
			}
			#if LOGGAMELOOP > 0
		al_trace("game_loop is calling: %s\n", "if(quakeclk)\n");
		#endif
			// Earthquake!
			if(quakeclk>0 && !FFCore.system_suspend[susptQUAKE] )
			{
				playing_field_offset=56+((int32_t)(sin((double)(--quakeclk*2-frame))*4));
			}
			else
			{
				playing_field_offset=56;
			}
			
		if ( previous_DMap != currdmap )
		{
			FFCore.initZScriptDMapScripts();
			FFCore.initZScriptActiveSubscreenScript();
			previous_DMap = currdmap;
		}
			// Other effects in zc_sys.cpp
		}
		
		//  putpixel(framebuf, walkflagx, walkflagy+playing_field_offset, vc(int32_t(zc_oldrand()%16)));
		break;
	}
}

void runDrunkRNG(){
	//Runs the RNG for drunk for each control which makes use of drunk toggling. 
	//Index 0-10 refer to control_state[0]-[9], while index 11 is used for `DrunkrMbtn()`/`DrunkcMbtn()`, which do not use control_states[]
	for(int32_t i = 0; i<sizeof(drunk_toggle_state); i++){
		if((!(frame%((zc_oldrand()%100)+1)))&&(zc_oldrand()%MAXDRUNKCLOCK<Hero.DrunkClock())){
			drunk_toggle_state[i] = (zc_oldrand()%2)?true:false;
		} else {
			drunk_toggle_state[i] = false;
		}
	}
}

int32_t get_currdmap()
{
    return currdmap;
}

int32_t get_dlevel()
{
    return dlevel;
}

int32_t get_currscr()
{
    return currscr;
}

int32_t get_currmap()
{
    return currmap;
}

int32_t get_homescr()
{
    return homescr;
}

int32_t get_bmaps(int32_t si)
{
    return game->bmaps[si];
}

bool no_subscreen()
{
    return (tmpscr->flags3&fNOSUBSCR)!=0;
}

bool isMonochrome(){
	return monochrome;
}

bool isUserTinted()
{
	return palette_user_tinted;
}
void isUserTinted(bool state)
{
	palette_user_tinted = state;
}

void setMonochrome(bool v){
	if ( v && (!monochrome || lastMonoPreset) ) { //lastMonoPreset check to allow overwriting a mono preset with setMonochrome's greyscale
		if(isUserTinted()){ // If a user tint is active, disable it and restore RAMpal
			memcpy(RAMpal, tempgreypal, PAL_SIZE*sizeof(RGB));
			isUserTinted(false);
		} else if(lastMonoPreset){ // If a monochrome preset was loaded, restore RAMpal, and disable it
			memcpy(RAMpal, tempgreypal, PAL_SIZE*sizeof(RGB));
			lastMonoPreset = 0;
		} else { // else back up RAMpal to tempgreypal
			memcpy(tempgreypal, RAMpal, PAL_SIZE*sizeof(RGB));
		}
		if(get_bit(quest_rules,qr_FADE)) {
		for(int32_t i=CSET(0); i < CSET(15); i++)
		{
			int32_t g = zc_min((RAMpal[i].r*42 + RAMpal[i].g*75 + RAMpal[i].b*14) >> 7, 63);
			g = (g >> 1) + 32;
			RAMpal[i] = _RGB(g,g,g);
		}
    
		}
		else
		{
			// this is awkward. NES Z1 converts colors based on the global
			// NES palette. Something like RAMpal[i] = NESpal( reverse_NESpal(RAMpal[i]) & 0x30 );
			for(int32_t i=CSET(0); i < CSET(15); i++)
			{
				RAMpal[i] = NESpal(reverse_NESpal(RAMpal[i]) & 0x30);
			}
		} 
		refreshpal = true;
		monochrome = true; 
	}
	
	else if ( !v && monochrome && !lastMonoPreset ) {
		memcpy(RAMpal, tempgreypal, PAL_SIZE*sizeof(RGB));
		refreshpal = true;
		monochrome = false;
	}
}

enum { colourNONE, colourGREY, colourRED, colourGREEN, colourBLUE, colourVIOLET, colourTEAL, colourAMBER, colourCYAN };
enum { baseUNIFORM, baseDISTRIBUTED = 1000 };
enum { tint_r, tint_g, tint_b, tint_bool_dist};

int16_t lastMonoPreset = 0; // The current Monochrome preset loaded
int16_t lastCustomTint[4] = {0,0,0,0}; // The current custom tint information. 0/1/2: R/G/B, 3: Base

void shiftColour(int32_t rshift, int32_t gshift, int32_t bshift, int32_t base)
{
	for(int32_t i=0; i <= 0xEF; i++)
	{
		if(base==baseUNIFORM){//Recolor the palette to uniform greyscale before tinting
			int32_t grey = (RAMpal[i].r+RAMpal[i].g+RAMpal[i].b)/3;
			RAMpal[i] = _RGB(grey,grey,grey);
		} else if(base==baseDISTRIBUTED){//Recolor the palette to distributed greyscale before tinting
			int32_t grey = 0.299*RAMpal[i].r + 0.587*RAMpal[i].g + 0.114*RAMpal[i].b;
			RAMpal[i] = _RGB(grey,grey,grey);
		}
		//Bit-shifting negatives throws errors. If negative, shift in the other direction.
		if(rshift>=0){
			RAMpal[i].r = zc_min(RAMpal[i].r >> rshift,63);
		} else {
			RAMpal[i].r = zc_min(RAMpal[i].r << -rshift,63);
		}
		if(gshift>=0){
			RAMpal[i].g = zc_min(RAMpal[i].g >> gshift,63);
		} else {
			RAMpal[i].g = zc_min(RAMpal[i].g << -gshift,63);
		}
		if(bshift>=0){
			RAMpal[i].b = zc_min(RAMpal[i].b >> bshift,63);
		} else {
			RAMpal[i].b = zc_min(RAMpal[i].b << -bshift,63);
		}
	}
}

void setMonochromatic(int32_t mode)
{
	int32_t base = mode < baseDISTRIBUTED ? baseUNIFORM : baseDISTRIBUTED; //distributed is an additive flag adding 10
	int32_t colour_mode = mode - base;
	if(isUserTinted()){
		memcpy(RAMpal, tempgreypal, PAL_SIZE*sizeof(RGB));
		isUserTinted(false); //Disable custom tint, override with monochrome
	}
	lastMonoPreset = mode;
	if (colour_mode <= 0 && monochrome ) //restore
	{
		memcpy(RAMpal, tempgreypal, PAL_SIZE*sizeof(RGB));
		refreshpal = true;
		monochrome = false; 
	}
	else if ( colour_mode ) 
	{ 
		//If a preset was already active, restore RAMpal before continuing; else, back up RAMpal.
		if ( monochrome ) {
			memcpy(RAMpal, tempgreypal, PAL_SIZE*sizeof(RGB));
		} else {
			memcpy(tempgreypal, RAMpal, PAL_SIZE*sizeof(RGB));
		}
		switch(colour_mode)
		{
			case colourGREY:
				shiftColour(0,0,0,base);
				break;
			case colourRED:
				shiftColour(0,4,4,base);
				break;
			case colourGREEN:
				shiftColour(4,0,4,base);
				break;
			case colourBLUE:
				shiftColour(4,4,0,base);
				break;
			case colourVIOLET:
				shiftColour(1,4,0,base);
				break;
			case colourTEAL:
				shiftColour(4,1,0,base);
				break;
			case colourAMBER:
				shiftColour(0,1,4,base);
				break;
			case colourCYAN:
				addColour(-63,-6,-2,base);
				break;
			default: shiftColour(0,0,0,base);
				break;
		}

		refreshpal = true;
		monochrome = true; 
	}
}

void addColour(int32_t radd, int32_t gadd, int32_t badd, int32_t base)
{
	for(int32_t i=0; i <= 0xEF; i++)
	{
		if(base==baseUNIFORM){//Recolor the palette to uniform greyscale before tinting
			int32_t grey = (RAMpal[i].r+RAMpal[i].g+RAMpal[i].b)/3;
			RAMpal[i] = _RGB(grey,grey,grey);
		} else if(base==baseDISTRIBUTED){//Recolor the palette to distributed greyscale before tinting
			int32_t grey = 0.299*RAMpal[i].r + 0.587*RAMpal[i].g + 0.114*RAMpal[i].b;
			RAMpal[i] = _RGB(grey,grey,grey);
		}
		//Add the r/g/b adds to the r/g/b values, clamping between 0 and 63.
		RAMpal[i].r = vbound(RAMpal[i].r + radd,0,63);
		RAMpal[i].g = vbound(RAMpal[i].g + gadd,0,63);
		RAMpal[i].b = vbound(RAMpal[i].b + badd,0,63);
	}
}

void doGFXMonohue(int32_t _r, int32_t _g, int32_t _b, bool m)
{
	if(monochrome) { // If a mono preset or greyscale was active, disable it and restore RAMpal before continuing.
		memcpy(RAMpal, tempgreypal, PAL_SIZE*sizeof(RGB));
		monochrome = false;
		lastMonoPreset = 0;
	}
	if(isUserTinted()) { // A tint already is active. Tint should then cascade.
		//Restore RAMpal
		memcpy(RAMpal, tempgreypal, PAL_SIZE*sizeof(RGB));
		//Add the current tint parameters to the previous ones
		lastCustomTint[0] = vbound(lastCustomTint[0]+_r,-32768,32767);
		lastCustomTint[1] = vbound(lastCustomTint[1]+_g,-32768,32767);
		lastCustomTint[2] = vbound(lastCustomTint[2]+_b,-32768,32767);
		//Force the base to the new base; bases do NOT cascade, they override.
		lastCustomTint[3] = m?baseDISTRIBUTED:baseUNIFORM;
		//Color with the new args
		addColour(lastCustomTint[0],lastCustomTint[1],lastCustomTint[2],lastCustomTint[3]);
	} else { // No tint is active.
		memcpy(tempgreypal, RAMpal, PAL_SIZE*sizeof(RGB));//Back up RAMpal into tempgreypal
		lastCustomTint[0] = _r;
		lastCustomTint[1] = _g;
		lastCustomTint[2] = _b;
		lastCustomTint[3] = m?baseDISTRIBUTED:baseUNIFORM;
		addColour(_r,_g,_b,m?baseDISTRIBUTED:baseUNIFORM);
	}
	lastMonoPreset = 0; // Clear mono preset to use tint instead
	isUserTinted(true);
	refreshpal = true;
}

void doTint(int32_t _r, int32_t _g, int32_t _b)
{
	if(monochrome) { // If a mono preset or greyscale was active, disable it and restore RAMpal before continuing.
		memcpy(RAMpal, tempgreypal, PAL_SIZE*sizeof(RGB));
		monochrome = false;
		lastMonoPreset = 0;
	}
	if(isUserTinted()) { // A tint already is active. Tint should then cascade.
		//Restore RAMpal
		memcpy(RAMpal, tempgreypal, PAL_SIZE*sizeof(RGB));
		//Add the current tint parameters to the previous ones
		lastCustomTint[0] = vbound(lastCustomTint[0]+_r,-32768,32767);
		lastCustomTint[1] = vbound(lastCustomTint[1]+_g,-32768,32767);
		lastCustomTint[2] = vbound(lastCustomTint[2]+_b,-32768,32767);
		//Force the base to the new base; bases do NOT cascade, they override.
		lastCustomTint[3] = -1;
		//Color with the new args
		addColour(lastCustomTint[0],lastCustomTint[1],lastCustomTint[2],lastCustomTint[3]);
	} else { // No tint is active
		memcpy(tempgreypal, RAMpal, PAL_SIZE*sizeof(RGB));//Back up RAMpal into tempgreypal
		lastCustomTint[0] = _r;
		lastCustomTint[1] = _g;
		lastCustomTint[2] = _b;
		lastCustomTint[3] = -1;
		addColour(_r,_g,_b,-1);
	}
	lastMonoPreset = 0; // Clear mono preset to use tint instead
	isUserTinted(true);
	refreshpal = true;
}

void doClearTint()
{
	//If a color mode was active, restore RAMpal from the backup
	if(monochrome || isUserTinted()){
		memcpy(RAMpal, tempgreypal, PAL_SIZE*sizeof(RGB));
		//Disable the booleans
		monochrome = false;
		isUserTinted(false);
		//Clear the storage
		lastMonoPreset = 0;
		refreshpal = true;
	}
}

void restoreTint(){
	if(isUserTinted()){
		memcpy(RAMpal, tempgreypal, PAL_SIZE*sizeof(RGB));
		addColour(lastCustomTint[0],lastCustomTint[1],lastCustomTint[2],lastCustomTint[3]);
		refreshpal = true;
	}
}

void restoreMonoPreset(){
	if(monochrome && lastMonoPreset){
		memcpy(RAMpal, tempgreypal, PAL_SIZE*sizeof(RGB));
		setMonochromatic(lastMonoPreset);
		refreshpal = true;
	}
}

void refreshTints()
{
	if(isMonochrome() && !lastMonoPreset)
	{
		setMonochrome(false);
		setMonochrome(true);
	}
	restoreTint();
	restoreMonoPreset();
}

int32_t getTint(int32_t color)
{
	return lastCustomTint[color];
}

void doDarkroomCircle(int32_t cx, int32_t cy, byte glowRad,BITMAP* dest,BITMAP* transdest)
{
	if(!glowRad) return;
	//Default bitmap handling
	if(!dest) dest = darkscr_bmp_curscr;
	if(dest == darkscr_bmp_scrollscr) transdest = darkscr_bmp_scrollscr_trans;
	else if(!transdest || dest == darkscr_bmp_curscr) transdest = darkscr_bmp_curscr_trans;
	//
	int32_t ditherRad = glowRad + (int32_t)(glowRad * (game->get_dither_perc()/(double)100.0));
	int32_t transRad = glowRad + (int32_t)(glowRad * (game->get_transdark_perc()/(double)100.0));
	dithercircfill(dest, cx, cy, ditherRad, 0, game->get_dither_type(), game->get_dither_arg());
	circlefill(dest, cx, cy, zc_max(glowRad,transRad), 0);
	dithercircfill(transdest, cx, cy, ditherRad, 0, game->get_dither_type(), game->get_dither_arg());
	circlefill(transdest, cx, cy, glowRad, 0);
}

void doDarkroomCone(int32_t sx, int32_t sy, byte glowRad, int32_t dir, BITMAP* dest,BITMAP* transdest)
{
	if(!glowRad) return;
	//Default bitmap handling
	if(!dest) dest = darkscr_bmp_curscr;
	if(dest == darkscr_bmp_scrollscr) transdest = darkscr_bmp_scrollscr_trans;
	else if(!transdest || dest == darkscr_bmp_curscr) transdest = darkscr_bmp_curscr_trans;
	//
	int32_t ditherDiff = (int32_t)(glowRad * (game->get_dither_perc()/(double)100.0));
	int32_t transDiff = (int32_t)(glowRad * (game->get_transdark_perc()/(double)100.0));
	int32_t ditherRad = glowRad + 2*ditherDiff;
	int32_t transRad = glowRad + 2*transDiff;
	
	double xs = 0, ys = 0;
	int32_t d = NORMAL_DIR(dir);
	if(d<0) return;
	switch(d)
	{
		case up: case l_up: case r_up: ys=1; break;
		case down: case l_down: case r_down: ys=-1; break;
	}
	switch(d)
	{
		case left: case l_up: case l_down: xs=1; break;
		case right: case r_up: case r_down: xs=-1; break;
	}
	if(d&4) {xs*=0.75; ys*=0.75;}
	ditherLampCone(dest, sx+(xs*ditherDiff), sy+(ys*ditherDiff), ditherRad, d, 0, game->get_dither_type(), game->get_dither_arg());
	if(glowRad>transRad) transDiff = 0;
	lampcone(dest, sx+(xs*transDiff), sy+(ys*transDiff), zc_max(glowRad,transRad), d, 0);
	
	ditherLampCone(transdest, sx+(xs*ditherDiff), sy+(ys*ditherDiff), ditherRad, d, 0, game->get_dither_type(), game->get_dither_arg());
	lampcone(transdest, sx, sy, glowRad, d, 0);
}

/**************************/
/********** Main **********/
/**************************/

bool is_zquest()
{
    return false;
}

bool screenIsScrolling()
{
    return screenscrolling;
}

int32_t isFullScreen()
{
    return !is_windowed_mode();
}

class TB_Handler //Dear Santa: please kill Easter bunny. I've been a good boy.
{
public:

    TB_Handler() {}
    ~TB_Handler() {}
    
    bool CanEnable() const
    {
        if(is_windowed_mode() && can_triplebuffer_in_windowed_mode == FALSE)
        {
            triplebuffer_not_available = true;
            return false;
        }
        
        return (disable_triplebuffer == FALSE);
    }
    bool GFX_can_triple_buffer() const
    {
        if(!CanEnable())
        {
            triplebuffer_not_available = true;
            return false;
        }
        
        triplebuffer_not_available = false;
        
        if(!(gfx_capabilities & GFX_CAN_TRIPLE_BUFFER)) enable_triple_buffer();
        
        if(!(gfx_capabilities & GFX_CAN_TRIPLE_BUFFER)) triplebuffer_not_available = true;
        
        return !triplebuffer_not_available;
    }
    void Destroy() const
    {
        if(disable_triplebuffer != FALSE || triplebuffer_not_available) return;
        
        for(int32_t i=0; i<3; i++)
            if(tb_page[i])
                destroy_bitmap(tb_page[i]);
    }
    void Create() const
    {
        if(!CanEnable())
        {
            triplebuffer_not_available = true;
            return;
        }
        
        for(int32_t i=0; i<3; ++i)
        {
            tb_page[i]=create_video_bitmap(SCREEN_W, SCREEN_H);
            
            if(!tb_page[i])
            {
                triplebuffer_not_available = true;
                break;
            }
        }
        
        Clear();
    }
    void Clear() const
    {
        for(int32_t i=0; i<3; i++)
            clear_bitmap(tb_page[i]);
    }
}
static Triplebuffer;

bool setGraphicsMode(bool windowed)
{
    int32_t type=windowed ? GFX_AUTODETECT_WINDOWED : GFX_AUTODETECT_FULLSCREEN;
    return set_gfx_mode(type, resx, resy, 0, 0)==0;
}

int32_t onFullscreen()
{
    if(jwin_alert3(
			(is_windowed_mode()) ? "Fullscreen Warning" : "Change to Windowed Mode", 
			(is_windowed_mode()) ? "Some video chipsets/drivers do not support 8-bit native fullscreen" : "Proceeding will drop from Fullscreen to Windowed Mode", 
			(is_windowed_mode()) ? "We strongly advise saving your game before shifting from windowed to fullscreen!": "Do you wish to shift from Fullscreen to Windowed mode?",
			(is_windowed_mode()) ? "Do you wish to continue to fullscreen mode?" : NULL,
		 "&Yes", 
		"&No", 
		NULL, 
		'y', 
		'n', 
		0, 
		lfont) == 1)	
    {
	    PALETTE oldpal;
	    get_palette(oldpal);
	    
	    show_mouse(NULL);
	    bool windowed=is_windowed_mode()!=0;
	    
	    // these will become ultra corrupted no matter what.
	    Triplebuffer.Destroy();
	    
	    bool success=setGraphicsMode(!windowed);
	    if(success)
		fullscreen=!fullscreen;
	    else
	    {
		// Try to restore the previous mode, then...
		success=setGraphicsMode(windowed);
		if(!success)
		{
		    Z_message("Failed to set video mode.\n");
		    Z_message(allegro_error);
		    exit(1);
		}
	    }
	    
	    /* ZC will crash going from fullscreen to windowed mode if triple buffer is left unchecked. -Gleeok  */
	    if(Triplebuffer.GFX_can_triple_buffer())
	    {
		Triplebuffer.Create();
		Z_message("Triplebuffer enabled \n");
	    }
	    else
		Z_message("Triplebuffer disabled \n");
	    
	    //Everything set?
	    Z_message("gfx mode set at -%d %dbpp %d x %d \n", is_windowed_mode(), get_color_depth(), resx, resy);
	    
	    set_palette(oldpal);
	    gui_mouse_focus=0;
	    show_mouse(screen);
	    switch_type = pause_in_background ? SWITCH_PAUSE : SWITCH_BACKGROUND;
	    set_display_switch_mode(fullscreen?SWITCH_BACKAMNESIA:switch_type);
	//	set_display_switch_callback(SWITCH_OUT, switch_out_callback);/
	//	set_display_switch_callback(SWITCH_IN,switch_in_callback);

	    return D_REDRAW;
    }
    else return D_O_K;
}

int32_t main(int32_t argc, char* argv[])
{
	bool onlyInstance=true;
	memset(itemscriptInitialised, 0, sizeof(itemscriptInitialised));
//	refresh_select_screen = 0;
	memset(modulepath, 0, sizeof(modulepath));
	FFCore.init_combo_doscript();

	memset(zc_builddate,0,80);
	memset(zc_aboutstr,0,80);

	sprintf(zc_builddate,"Build Date: %s %s, %d at @ %s %s", dayextension(BUILDTM_DAY).c_str(), (char*)months[BUILDTM_MONTH], BUILDTM_YEAR, __TIME__, __TIMEZONE__);
	sprintf(zc_aboutstr,"%s (%s), Version %s", ZC_PLAYER_NAME, PROJECT_NAME, ZC_PLAYER_V);
	

	Z_title("%s, v.%s %s",ZC_PLAYER_NAME, ZC_PLAYER_V, ALPHA_VER_STR);
	
	if(used_switch(argc, argv, "-standalone"))
	{
		standalone_mode=true;
		
		int32_t arg=used_switch(argc, argv, "-standalone");
		
		if(arg==argc-1)
		{
			Z_error_fatal("-standalone requires a quest file, e.g.\n" \
					"  -standalone MyQuest.qst\n" \
					"  -standalone \"Name with spaces.qst\"");
			exit(1);
		}
		
		standalone_quest=argv[arg+1];
		
		if(stricmp(standalone_quest, "1st.qst")==0 ||
		  stricmp(standalone_quest, "2nd.qst")==0 ||
		  stricmp(standalone_quest, "3rd.qst")==0 ||
		  stricmp(standalone_quest, "4th.qst")==0 ||
		  stricmp(standalone_quest, "5th.qst")==0)
		{
			Z_error_fatal("Standalone mode can only be used with custom quests.");
			exit(1);
		}
		
		regulate_path(standalone_quest);
	}
	
	//turn on MSVC memory checks
	//this should be interesting...
	
//  InitCrtDebug();

	// Before anything else, let's register our custom trace handler:
	register_trace_handler(zc_trace_handler);
	
	// allocate quest data buffers
#ifdef _WIN32
	if(used_switch(argc, argv, "-console") || used_switch(argc, argv, "-con"))
	{
		DebugConsole::Open();
		zconsole = true;
	}
#endif
	memrequested += 4096;
	Z_message("Allocating quest path buffers (%s)...", byte_conversion2(4096,memrequested,-1,-1));
	qstdir = (char*)zc_malloc(2048);
	qstpath = (char*)zc_malloc(2048);
	
	if(!qstdir || !qstpath)
	{
		Z_error_fatal("Allocation error");
		quit_game();
	}
	
	qstdir[0] = 0;
	qstpath[0] = 0;
	
#ifdef ALLEGRO_MACOSX
	sprintf(qstdir, "../../../");
	sprintf(qstpath, "../../../");
#endif
	Z_message("OK\n");
	
	if(!get_qst_buffers())
	{
		Z_error_fatal("Error");
		quit_game();
	}
	
	Z_message("Initializing Allegro... ");
	if(allegro_init() != 0)
	{
		Z_error_fatal("Failed Init!");
		quit_game();
	}
	
	three_finger_flag=false;
	
	register_bitmap_file_type("GIF",  load_gif, save_gif);
	jpgalleg_init();
	loadpng_init();
	
	// set and load game configurations
	set_config_file("zc.cfg");
	
	if(exists("zc.cfg") != 0)
	{
		load_game_configs();
	}
	else
	{
		load_game_configs();
		save_game_configs();
	}
	
#ifndef ALLEGRO_MACOSX // Should be done on Mac, too, but I haven't gotten that working
	if(!is_only_instance("zc.lck"))
	{
		if(used_switch(argc, argv, "-multiple") || zc_get_config("zeldadx","multiple_instances",0))
			onlyInstance=false;
		else
			exit(1);
	}
#endif
	
	//Set up MODULES: This must occur before trying to load the default quests, as the 
	//data for quest names and so forth is set by the MODULE file!
	//strcpy(moduledata.module_name,get_config_string("ZCMODULE","current_module", moduledata.module_name));
	//al_trace("Before zcm.init, the current module is: %s\n", moduledata.module_name)
	if ( !(zcm.init(true)) ) 
	{
		exit(1);    
	}
	
#ifdef _WIN32
	
	//launch debug console if requested.
	if(use_debug_console != FALSE)
	{
		DebugConsole::Open();
		zconsole = true;
	}
	if ( zscript_debugger )
	{
		FFCore.ZScriptConsole(true);
	}

#else //Unix

	if(zscript_debugger)
	{ // Let's try making a console for Linux -Z
		int32_t termflags = 0;
		termflags |= O_RDWR; //Open the device for both reading and writing.
		//termflags |= O_NOCTTY; //Do not make this device the controlling terminal for the process.
		pt = posix_openpt(termflags);
		if (pt == -1)
		{
			Z_error_fatal("Could not open pseudo terminal; error number: %d.\n", errno);
			use_debug_console = 0; goto no_lx_console;
		}
		ptname = ptsname(pt);
		if (!ptname)
		{
			Z_error_fatal("Could not get pseudo terminal device name.\n");
			close(pt);
			use_debug_console = 0; goto no_lx_console;
		}

		if (unlockpt(pt) == -1)
		{
			Z_error_fatal("Could not get pseudo terminal device name.\n");
			close(pt);
			use_debug_console = 0; goto no_lx_console;
		}

		lxconsole_oss << "xterm -S" << (strrchr(ptname, '/')+1) << "/" << pt << " &";
		system(lxconsole_oss.str().c_str());

		int32_t xterm_fd = open(ptname,termflags); //This also needs the O_NOCTTY flag. See: https://man7.org/linux/man-pages/man3/open.3p.html
		{
			char c = 0; int32_t tries = 10000; 
			do 
			{
				read(xterm_fd, &c, 1); 
				--tries;
			} while (c!='\n' && tries > 0);
		}

		if (dup2(pt, 1) <0)
		{
			Z_error_fatal("Could not redirect standard output.\n");
			close(pt);
			use_debug_console = 0; goto no_lx_console;
		}
		if (dup2(pt, 2) <0)
		{
			Z_error_fatal("Could not redirect standard error output.\n");
			close(pt);
			use_debug_console = 0; goto no_lx_console;
		}
	} //this is in a block because I want it in a block. -Z
	else
	{
		al_trace("Linux console disabled by user.\n");
	}
	
	no_lx_console:
	{
		//Z_error_fatal("Could not open Linux console.\n");
	}
	
	
	std::cout << "\n       _____   ____                  __ \n";
	std::cout << "      /__  /  / __ \\__  _____  _____/ /_\n";
	std::cout << "        / /  / / / / / / / _ \\/ ___/ __/\n";
	std::cout << "       / /__/ /_/ / /_/ /  __(__  ) /_ \n";
	std::cout << "      /____/\\___\\_\\__,_/\\___/____/\\__/\n\n";
	
	std::cout << "Quest Data Logging & ZScript Debug Console\n";
	std::cout << "ZConsole for Linux\n\n";
	
	if ( FFCore.getQuestHeaderInfo(vZelda) > 0 )
	{
		printf("Quest Made in ZC Version %x, Build %d\n", FFCore.getQuestHeaderInfo(vZelda), FFCore.getQuestHeaderInfo(vBuild));
	}
	else
	{
		printf("%s, Version %s\n", ZC_PLAYER_NAME, ZC_PLAYER_V);
	}
#endif
	
	if(install_timer() < 0)
	{
		Z_error_fatal(allegro_error);
		quit_game();
	}
	
	if(install_keyboard() < 0)
	{
		Z_error_fatal(allegro_error);
		quit_game();
	}
	
	if(install_mouse() < 0)
	{
		Z_error_fatal(allegro_error);
		quit_game();
	}
	
	if(install_joystick(JOY_TYPE_AUTODETECT) < 0)
	{
		Z_error_fatal(allegro_error);
		quit_game();
	}
	
	//set_keyboard_rate(1000,160);
	
	LOCK_VARIABLE(logic_counter);
	LOCK_FUNCTION(update_logic_counter);
	install_int_ex(update_logic_counter, BPS_TO_TIMER(60));
	
#ifdef _SCRIPT_COUNTER
	LOCK_VARIABLE(script_counter);
	LOCK_FUNCTION(update_script_counter);
	install_int_ex(update_script_counter, 1);
#endif
	LOCK_VARIABLE(myvsync);
	LOCK_FUNCTION(myvsync_callback);
	
	bool timerfail = false;
	if(install_int_ex(myvsync_callback,BPS_TO_TIMER(60)))
		timerfail = true;
	
	if(!timerfail && !Z_init_timers())
		timerfail = true;
	
	if(timerfail)
	{
		Z_error_fatal("Couldn't Allocate Timers");
		quit_game();
	}
	
	Z_message("OK\n");
	
	// check for the included quest files
	if(!standalone_mode)
	{
		Z_message("Checking Files... ");
		
		char path[2048];
		
		for ( byte q = 0; q < moduledata.max_quest_files; q++ )
		{
			append_filename(path, qstdir, moduledata.quests[q], 2048);
			if(!exists(moduledata.quests[q]) && !exists(path))
			{
				Z_error_fatal("%s not found.\n", moduledata.quests[q]);
				quit_game();
			}
		}
		Z_message("OK\n");
	}
	
	// allocate bitmap buffers
	Z_message("Allocating bitmap buffers... ");
	
	//Turns out color depth can be critical. -Gleeok
	if(used_switch(argc,argv,"-0bit")) set_color_depth(desktop_color_depth());
	else if(used_switch(argc,argv,"-15bit")) set_color_depth(15);
	else if(used_switch(argc,argv,"-16bit")) set_color_depth(16);
	else if(used_switch(argc,argv,"-24bit")) set_color_depth(24);
	else if(used_switch(argc,argv,"-32bit")) set_color_depth(32);
	else
	{
		//command-line switches takes priority
		switch(zc_color_depth)
		{
			case 0:
				set_color_depth(desktop_color_depth());
				//setGraphicsMode(fullscreen);
				break;
				
			case 8:
				set_color_depth(8);
				//setGraphicsMode(fullscreen);
				break;
				
			case 15:
				set_color_depth(15);
				//setGraphicsMode(fullscreen);
				break;
				
			case 16:
				set_color_depth(16);
				//setGraphicsMode(fullscreen);
				break;
				
			case 24:
				set_color_depth(24);
				//setGraphicsMode(fullscreen);
				break;
				
			case 32:
				set_color_depth(32);
				//setGraphicsMode(fullscreen);
				break;
				
			default:
				zc_color_depth = 8; //invalid configuration, set to default in config file.
				set_color_depth(8);
				break;
		}
	}
	
	//set_color_depth(32);
	//set_color_conversion(COLORCONV_24_TO_8);
	framebuf  = create_bitmap_ex(8,256,224);
	temp_buf  = create_bitmap_ex(8,256,224);
	scrollbuf = create_bitmap_ex(8,512,406);
	screen2   = create_bitmap_ex(8,320,240);
	tmp_scr   = create_bitmap_ex(8,320,240);
	tmp_bmp   = create_bitmap_ex(8,32,32);
	fps_undo  = create_bitmap_ex(8,64,16);
	prim_bmp  = create_bitmap_ex(8,512,512);
	msg_bg_display_buf = create_bitmap_ex(8,256, 176);
	msg_txt_display_buf = create_bitmap_ex(8,256, 176);
	msg_bg_bmp_buf = create_bitmap_ex(8, 512+16, 512+16);
	msg_txt_bmp_buf = create_bitmap_ex(8, 512+16, 512+16);
	msg_menu_bmp_buf = create_bitmap_ex(8, 512+16, 512+16);
	msg_portrait_bmp_buf = create_bitmap_ex(8, 256, 256);
	msg_portrait_display_buf = create_bitmap_ex(8, 256, 256);
	pricesdisplaybuf = create_bitmap_ex(8,256, 176);
	script_menu_buf = create_bitmap_ex(8,256,224);
	f6_menu_buf = create_bitmap_ex(8,256,224);
	darkscr_bmp_curscr = create_bitmap_ex(8, 256, 176);
	darkscr_bmp_curscr_trans = create_bitmap_ex(8, 256, 176);
	darkscr_bmp_scrollscr = create_bitmap_ex(8, 256, 176);
	darkscr_bmp_scrollscr_trans = create_bitmap_ex(8, 256, 176);
	lightbeam_bmp = create_bitmap_ex(8, 256, 176);
	
	if(!framebuf || !scrollbuf || !tmp_bmp || !fps_undo || !tmp_scr
			|| !screen2 || !msg_txt_display_buf || !msg_bg_display_buf || !pricesdisplaybuf
			|| !script_menu_buf || !f6_menu_buf)
	{
		Z_error_fatal("Error");
		quit_game();
	}
	
	clear_bitmap(lightbeam_bmp);
	clear_bitmap(scrollbuf);
	clear_bitmap(framebuf);
	clear_bitmap(msg_bg_display_buf);
	set_clip_state(msg_bg_display_buf, 1);
	clear_bitmap(msg_txt_display_buf);
	set_clip_state(msg_txt_display_buf, 1);
	clear_bitmap(msg_portrait_display_buf);
	set_clip_state(msg_portrait_display_buf, 1);
	clear_bitmap(pricesdisplaybuf);
	set_clip_state(pricesdisplaybuf, 1);
	Z_message("OK\n");
	
	Z_message("Initializing music... ");
	zcmusic_init();
	Z_message("OK\n");
	
	//  int32_t mode = VidMode;                                       // from config file
	int32_t tempmode=GFX_AUTODETECT;
	int32_t res_arg = used_switch(argc,argv,"-res");
	
	if(used_switch(argc,argv,"-v0")) Throttlefps=false;
	
	if(used_switch(argc,argv,"-v1")) Throttlefps=true;
	
	resolve_password(zeldapwd);
	debug_enabled = used_switch(argc,argv,"-d") && !strcmp(get_config_string("zeldadx","debug",""),zeldapwd);
	set_debug(debug_enabled);
	
	skipicon = standalone_mode || used_switch(argc,argv,"-quickload") || zc_get_config("zeldadx","skip_icons",0);
	
	int32_t load_save=0;
	
	load_save = used_switch(argc,argv,"-load");
	load_save = load_save?(argc>load_save+1)?atoi(argv[load_save+1]):0:0;
	if(!load_save)
	{
		load_save = zc_get_config("zeldadx","quickload_slot",0);
	}
	if(!load_save)
	{
		if(used_switch(argc,argv,"-loadlast") || zc_get_config("zeldadx","quickload_last",0))
			load_save = loadlast;
	}
			
	slot_arg = used_switch(argc,argv,"-slot");
	
	if(argc <= (slot_arg+1))
	{
		slot_arg = 0;
	}
	else
	{
		slot_arg2 = vbound(atoi(argv[slot_arg+1]), 1, MAXSAVES);
	}
	
	if(standalone_mode)
	{
		load_save=1;
		slot_arg=1;
		slot_arg2=1;
	}
	
	int32_t fast_start = debug_enabled || used_switch(argc,argv,"-fast") || (!standalone_mode && (load_save || (slot_arg && (argc>(slot_arg+1)))));
	skip_title = used_switch(argc, argv, "-notitle") > 0 || zc_get_config("zeldadx","skip_title",1);
	int32_t save_arg = used_switch(argc,argv,"-savefile");
	
	int32_t checked_epilepsy = zc_get_config("zeldadx","checked_epilepsy",0);
	/*
	if ( !strcmp(get_config_string("zeldadx","debug",""),"") )
	{
		for ( int32_t q = 0; q < 1024; ++q ) { save_file_name[q] = 0; }
			strcpy(save_file_name,"zc.sav");
		SAVE_FILE = (char *)save_file_name;  
	}
	/*else*/ //if ( strcmp(get_config_string("zeldadx","debug","")) )
	{	    
		for ( int32_t q = 0; q < 1024; ++q ) { save_file_name[q] = 0; }
			strcpy(save_file_name,get_config_string("SAVEFILE","save_filename","zc.sav"));
		SAVE_FILE = (char *)save_file_name;
	}
	//al_trace("Current save file is: %s\n", save_file_name);
	
	if(save_arg && (argc>(save_arg+1)))
	{
		SAVE_FILE = (char *)zc_malloc(2048);
		sprintf(SAVE_FILE, "%s", argv[save_arg+1]);
		
		regulate_path(SAVE_FILE);
	}
	
	// load the data files
	resolve_password(datapwd);
	//setPackfilePassword(datapwd);
	packfile_password(datapwd);
	
	Z_message("Loading data files:\n");
	set_color_conversion(COLORCONV_NONE);
	
	sprintf(zeldadat_sig,"Zelda.Dat %s Build %d",VerStr(ZELDADAT_VERSION), ZELDADAT_BUILD);
	sprintf(sfxdat_sig,"SFX.Dat %s Build %d",VerStr(SFXDAT_VERSION), SFXDAT_BUILD);
	sprintf(fontsdat_sig,"Fonts.Dat %s Build %d",VerStr(FONTSDAT_VERSION), FONTSDAT_BUILD);
	
	packfile_password(""); // Temporary measure. -L
	Z_message("Zelda.Dat...");
	
	if((data=load_datafile(moduledata.datafiles[zelda_dat]))==NULL) 
	{
		Z_error_fatal("failed");
		quit_game();
	}
	
	if(strncmp((char*)data[0].dat,zeldadat_sig,24))
	{
		Z_error_fatal("\nIncompatible version of zelda.dat.\nPlease upgrade to %s Build %d",VerStr(ZELDADAT_VERSION), ZELDADAT_BUILD);
		quit_game();
	}
	
	Z_message("OK\n");
	packfile_password(datapwd); // Temporary measure. -L
	
	Z_message("Fonts.Dat...");
	
	if((fontsdata=load_datafile(moduledata.datafiles[fonts_dat]))==NULL)
	{
		Z_error_fatal("failed");
		quit_game();
	}
	
	if(strncmp((char*)fontsdata[0].dat,fontsdat_sig,24))
	{
		Z_error_fatal("\nIncompatible version of fonts.dat.\nPlease upgrade to %s Build %d",VerStr(FONTSDAT_VERSION), FONTSDAT_BUILD);
		quit_game();
	}
	
	Z_message("OK\n");
	
	//setPackfilePassword(NULL);
	packfile_password(NULL);
	
	Z_message("SFX.Dat...");
	
	if((sfxdata=load_datafile(moduledata.datafiles[sfx_dat]))==NULL)
	{
		Z_error_fatal("failed");
		quit_game();
	}
	
	if(strncmp((char*)sfxdata[0].dat,sfxdat_sig,22) || sfxdata[Z35].type != DAT_ID('S', 'A', 'M', 'P'))
	{
		Z_error_fatal("\nIncompatible version of sfx.dat.\nPlease upgrade to %s Build %d",VerStr(SFXDAT_VERSION), SFXDAT_BUILD);
		quit_game();
	}
	
	Z_message("OK\n");
	
	mididata = (DATAFILE*)data[ZC_MIDI].dat;
	
	set_uformat(U_ASCII);
	initFonts();
	
	for(int32_t i=0; i<4; i++)
	{
		for(int32_t j=0; j<MAXSUBSCREENITEMS; j++)
		{
			memset(&custom_subscreen[i].objects[j],0,sizeof(subscreen_object));
		}
	}
	
	for(int32_t i=0; i<WAV_COUNT; i++)
	{
		customsfxdata[i].data=NULL;
		sfx_string[i] = new char[36];
	}
	
	for(int32_t i=0; i<WAV_COUNT>>3; i++)
	{
		customsfxflag[i] = 0;
	}
	
	for(int32_t i=0; i<WPNCNT; i++)
	{
		weapon_string[i] = new char[64];
	}
	
	for(int32_t i=0; i<ITEMCNT; i++)
	{
		item_string[i] = new char[64];
	}
	
	for(int32_t i=0; i<eMAXGUYS; i++)
	{
		guy_string[i] = new char[64];
	}
	
	for(int32_t i=0; i<NUMSCRIPTFFC; i++)
	{
		ffscripts[i] = new script_data();
	}
	
	for(int32_t i=0; i<NUMSCRIPTITEM; i++)
	{
		itemscripts[i] = new script_data();
	}
	
	for(int32_t i=0; i<NUMSCRIPTGUYS; i++)
	{
		guyscripts[i] = new script_data();
	}
	
	for(int32_t i=0; i<NUMSCRIPTWEAPONS; i++)
	{
		wpnscripts[i] = new script_data();
	}
	
	for(int32_t i=0; i<NUMSCRIPTSCREEN; i++)
	{
		screenscripts[i] = new script_data();
	}
	
	for(int32_t i=0; i<NUMSCRIPTGLOBAL; i++)
	{
		globalscripts[i] = new script_data();
	}
	
	for(int32_t i=0; i<NUMSCRIPTPLAYER; i++)
	{
		playerscripts[i] = new script_data();
	}
	
	 for(int32_t i=0; i<NUMSCRIPTWEAPONS; i++)
	{
		lwpnscripts[i] = new script_data();
	}
	 for(int32_t i=0; i<NUMSCRIPTWEAPONS; i++)
	{
		ewpnscripts[i] = new script_data();
	}
	
	 for(int32_t i=0; i<NUMSCRIPTSDMAP; i++)
	{
		dmapscripts[i] = new script_data();
	}
	for(int32_t i=0; i<NUMSCRIPTSITEMSPRITE; i++)
	{
		itemspritescripts[i] = new script_data();
	}
	for(int32_t i=0; i<NUMSCRIPTSCOMBODATA; i++)
	{
		comboscripts[i] = new script_data();
	}
	
	//script drawing bitmap allocation
	zscriptDrawingRenderTarget = new ZScriptDrawingRenderTarget();
	
	// initialize sound driver
	Z_message("Initializing sound driver... ");
	
	if(used_switch(argc,argv,"-s") || used_switch(argc,argv,"-nosound") || zc_get_config("zeldadx","nosound",0))
	{
		Z_message("skipped\n");
	}
	else
	{
		if(install_sound(DIGI_AUTODETECT,MIDI_AUTODETECT,NULL))
		{
			//      Z_error_fatal(allegro_error);
			Z_message("Sound driver not available.  Sound disabled.\n");
		}
		else
		{
			Z_message("OK\n");
		}
	}
	
	Z_init_sound();
	
	
	// CD player
	
	/*
	  if(used_switch(argc,argv,"-cd"))
	  {
	  printf("Initializing CD player... ");
	  if(cd_init())
	  Z_error_fatal("Error");
	  printf("OK\n");
	  useCD = true;
	  }
	  */
	
	//use only page flipping
	if(used_switch(argc,argv,"-doublebuffer"))
	{
		disable_triplebuffer = 1;
		Z_message("used switch: -doublebuffer\n");
	}
	
	//allow video bitmaps in windowed mode
	if(used_switch(argc,argv,"-triplebuffer"))
	{
		can_triplebuffer_in_windowed_mode = 1;
		Z_message("used switch: -triplebuffer\n");
	}
	
	const int32_t wait_ms_on_set_graphics = 20; //formerly 250. -Gleeok
	
	// quick quit
	if(used_switch(argc,argv,"-q"))
	{
		printf("-q switch used, quitting program.\n");
		//restore user volume settings
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
		show_saving(screen);
		save_savedgames();
		save_game_configs();
		Triplebuffer.Destroy();
		set_gfx_mode(GFX_TEXT,80,25,0,0);
		//rest(250); // ???
		//  if(useCD)
		//    cd_exit();
		quit_game();
		Z_message("Armageddon Games web site: http://www.armageddongames.com\n");
		Z_message("Zelda Classic web site: http://www.zeldaclassic.com\n");
		Z_message("Zelda Classic wiki: http://www.shardstorm.com/ZCwiki/\n");
			
		__zc_debug_malloc_free_print_memory_leaks(); //this won't do anything without debug_malloc_logging defined.
		skipcont = 0;
		if(forceExit) //fix for the allegro at_exit() hang.
			exit(0);
			
		allegro_exit();
		return 0;
	}
	
	// set video mode
	
	if(res_arg && (argc>(res_arg+2)))
	{
		resx = atoi(argv[res_arg+1]);
		resy = atoi(argv[res_arg+2]);
		bool old_sbig = (argc>(res_arg+3))? stricmp(argv[res_arg+3],"big")==0 : 0;
		bool old_sbig2 = (argc>(res_arg+3))? stricmp(argv[res_arg+3],"big2")==0 : 0;
		
		//mode = GFX_AUTODETECT;
	}
	
	if(resx>=640 && resy>=480)
	{
		is_large=true;
	}
	
	//request_refresh_rate(60);
	
	//is the config file wrong (not zc.cfg!) here? -Z
	if(used_switch(argc,argv,"-fullscreen") ||
			(!used_switch(argc, argv, "-windowed") && zc_get_config("zeldadx","fullscreen",0)==1))
	{
		al_trace("Used switch: -fullscreen\n");
		tempmode = GFX_AUTODETECT_FULLSCREEN;
	}
	else if(used_switch(argc,argv,"-windowed") || zc_get_config("zeldadx","fullscreen",0)==0)
	{
		al_trace("Used switch: -windowed\n");
		tempmode=GFX_AUTODETECT_WINDOWED;
	}
	
	//set scale
	if(resx < 256) resx = 256;
	
	if(resy < 240) resy = 240;
	
	int32_t noborder = (resx % 256);
	
	screen_scale = zc_max(zc_min(resx / (noborder?320:256), resy / 240), 1);
	
	if(!game_vid_mode(tempmode, wait_ms_on_set_graphics))
	{
		//what we need here is not rightousness but madness!!!
		
#define TRY_SET_VID_MODE(scale) \
	Z_message("Unable to set gfx mode at -%d %dbpp %d x %d \n", tempmode, get_color_depth(), resx, resy); \
	screen_scale=scale; \
	resx=320*scale; \
	resy=240*scale
		
		TRY_SET_VID_MODE(2);
		
		if(!game_vid_mode(tempmode, wait_ms_on_set_graphics))
		{
			TRY_SET_VID_MODE(1);
			
			if(!game_vid_mode(tempmode, wait_ms_on_set_graphics))
			{
				if(tempmode != GFX_AUTODETECT_WINDOWED)
				{
					tempmode=GFX_AUTODETECT_WINDOWED;
					al_trace("-fullscreen not supported by your video driver! setting -windowed switch\n");
					TRY_SET_VID_MODE(2);
					
					if(!game_vid_mode(tempmode, wait_ms_on_set_graphics))
					{
						TRY_SET_VID_MODE(1);
						
						if(!game_vid_mode(tempmode, wait_ms_on_set_graphics))
						{
							Z_message("Unable to set gfx mode at -%d %dbpp %d x %d \n", tempmode, get_color_depth(), resx, resy);
							al_trace("Fatal Error...Zelda Classic could not be initialized. Have a nice day :) \n");
							Z_error_fatal(allegro_error);
							quit_game();
						}
						else Z_message("set gfx mode succsessful at -%d %dbpp %d x %d \n", tempmode, get_color_depth(), resx, resy);
					}
					else Z_message("set gfx mode succsessful at -%d %dbpp %d x %d \n", tempmode, get_color_depth(), resx, resy);
				}
				else
				{
					al_trace("Fatal Error: could not create a window for Zelda Classic.\n");
					Z_error_fatal(allegro_error);
					quit_game();
				}
			}
			else Z_message("set gfx mode succsessful at -%d %dbpp %d x %d \n", tempmode, get_color_depth(), resx, resy);
		}
		else Z_message("set gfx mode succsessful at -%d %dbpp %d x %d \n", tempmode, get_color_depth(), resx, resy);
	}
	else
	{
		Z_message("set gfx mode succsessful at -%d %dbpp %d x %d \n", tempmode, get_color_depth(), resx, resy);
	}
	
	sbig = (screen_scale > 1);
	switch_type = pause_in_background ? SWITCH_PAUSE : SWITCH_BACKGROUND;
	set_display_switch_mode(is_windowed_mode()?SWITCH_PAUSE:switch_type);
	zq_screen_w = resx;
	zq_screen_h = resy;
	
	hw_screen = screen;
	hw_palette = &RAMpal;
	screen = create_bitmap_ex(8, resx, resy);
	clear_to_color(screen, BLACK);
	
	if(Triplebuffer.GFX_can_triple_buffer())
	{
		Triplebuffer.Create();
	}
	
	Z_message("Triplebuffer %savailable\n", triplebuffer_not_available?"not ":"");
	
	
	set_close_button_callback((void (*)()) hit_close_button);
	set_window_title("Zelda Classic");
	
	
	fix_dialogs();
	gui_mouse_focus = FALSE;
	position_mouse(resx-16,resy-16);
	
	if(!onlyInstance)
	{
		clear_to_color(screen,BLACK);
		system_pal();
		int32_t ret=jwin_alert3("Multiple Instances",
							"Another instance of ZC is already running.",
							"Running multiple instances may cause your",
							"save file to be deleted. Continue anyway?",
							"&No","&Yes", 0, 'n', 'y', 0, lfont);
		if(ret!=2)
		{
			if(forceExit)
				exit(0);
				
			allegro_exit();
			return 0;
		}
	}
	
	if(!checked_epilepsy)
	{
		clear_to_color(screen,BLACK);
		system_pal();
		if(jwin_alert("EPILEPSY Options",
			  "Do you desire epilepsy protection?",
			  "This will reduce the intensity of flashing effects",
			  "and reduce the amplitude of wavy screen effects.",
			  "No","Yes",13,27,lfont)!=1)
		{
			epilepsyFlashReduction = 1;
		}
		set_config_int("zeldadx","checked_epilepsy",1);
		save_game_configs();
		checked_epilepsy = 1;
	}
	
	//set switching/focus mode -Z
	set_display_switch_mode(is_windowed_mode()?(pause_in_background ? SWITCH_PAUSE : SWITCH_BACKGROUND):SWITCH_BACKAMNESIA);
	
	int32_t test_arg = used_switch(argc,argv,"-test");
	zqtesting_mode = test_arg > 0;
	if(zqtesting_mode)
	{
		clearConsole();
		Z_message("Initializing test mode...\n");
		if(test_arg+3 >= argc)
		{
			Z_error_fatal( "-test missing parameters:\n"
				"-test \"quest_file_path\" test_dmap test_screen\n" );
			exit(1);
		}
		bool error = false;
		strcpy(testingqst_name, argv[test_arg+1]);
		int32_t dm = atoi(argv[test_arg+2]);
		int32_t scr = atoi(argv[test_arg+3]);
		int32_t retsqr = (test_arg+4 >= argc) ? 0 : atoi(argv[test_arg+4]);
		if(!fileexists(testingqst_name))
		{
			Z_error_fatal( "-test invalid parameter: 'quest_file_path' was '%s',"
				" but that file does not exist!\n", testingqst_name);
			error = true;
		}
		if(unsigned(dm) >= MAXDMAPS)
		{
			Z_error_fatal( "-test invalid parameter: 'test_dmap' was '%d'."
				" Must be '0 <= test_dmap < %d'\n", dm, MAXDMAPS);
			error = true;
		}
		if(unsigned(scr) >= 0x80)
		{
			Z_error_fatal( "-test invalid parameter: 'test_screen' was '%d'."
				" Must be '0 <= test_screen < 128'\n", scr);
			error = true;
		}
		if(unsigned(retsqr) > 3) retsqr = 0;
		
		if(error)
		{
			Z_error_fatal("Failed '-test \"%s\" %d %d'\n", testingqst_name, dm, scr);
			exit(1);
		}
		testingqst_dmap = (uint16_t)dm;
		testingqst_screen = (uint8_t)scr;
		testingqst_retsqr = (uint8_t)retsqr;
	}
	
	//clearConsole();
	init_saves();
	if(!zqtesting_mode)
	{
		// load saved games
		zprint2("Loading Saved Games\n");
		if(load_savedgames() != 0)
		{
			Z_error_fatal("Insufficient memory");
			quit_game();
		}
		zprint2("Finished Loading Saved Games\n");
	}
#ifdef _WIN32
	// Nothing for them to do on other platforms
	set_display_switch_callback(SWITCH_IN,switch_in_callback);
	set_display_switch_callback(SWITCH_OUT,switch_out_callback);
#endif
	
	// AG logo
	if(!(zqtesting_mode||fast_start||zc_get_config("zeldadx","skip_logo",1)))
	{
		set_volume(240,-1);
		aglogo(tmp_scr, scrollbuf, resx, resy);
		master_volume(digi_volume,midi_volume);
	}
	
	// play the game
	fix_menu();
	reset_items(true, &QHeader);
	
	clear_to_color(screen,BLACK);
	Quit = (fast_start||skip_title) ? qQUIT : qRESET;
	
	rgb_map = &rgb_table;
	
	// set up an initial game save slot (for the list_saves function)
	game = new gamedata;
	game->Clear();
	
	hangcount = zc_get_config("ZSCRIPT","ZASM_Hangcount",1000);
	
#ifdef _WIN32
	
	if(use_win32_proc != FALSE)
	{
		Z_message("Config file warning: \"zc_win_proc_fix\" enabled switch found. This can cause crashes on some computers.\n");
		
		if(win32data.zcSetCustomCallbackProc(win_get_window()) != 0)
		{
			use_win32_proc = FALSE;
		}
	}
	
#endif
	
	
	if(zqtesting_mode)
	{
		currgame = 0;
		saves[0].Clear();
		saves[0].set_continue_dmap(testingqst_dmap);
		saves[0].set_continue_scrn(testingqst_screen);
		strcpy(saves[0].qstpath, testingqst_name);
		saves[0].set_quest(0xFF);
		saves[0].set_name("Hero");
		clearConsole();
		Z_message("Test mode: \"%s\", %d, %d\n", testingqst_name, testingqst_dmap, testingqst_screen);
	}
	while(Quit!=qEXIT)
	{
		// this is here to continually fix the keyboard repeat
		set_keyboard_rate(250,33);
		toogam = false;
		ignoreSideview=false;
		if(zqtesting_mode)
		{
			int32_t q = Quit;
			Quit = 0;
			if(q==qCONT)
				cont_game();
			else init_game();
			Quit = 0;
		}
		else titlescreen(load_save);
		
		load_save=0;
		setup_combo_animations();
		setup_combo_animations2();
		
		while(Quit<=0)
		{
#ifdef _WIN32
		
			if(use_win32_proc != FALSE)
			{
				win32data.Update(0);
			}
			
#endif
			game_loop();
			
			//Perpetual item Script:
			FFCore.newScriptEngine();
			
			FFCore.runF6Engine();
		
			//clear Hero's last hits 
			//for ( int32_t q = 0; q < 4; q++ ) Hero.sethitHeroUID(q, 0);
			//clearing this here makes it impossible 
			//to read before or after waitdraw in scripts. 
		}
		
		tmpscr->flags3=0;
		Playing=Paused=false;
		//Clear active script array ownership
		FFCore.deallocateAllArrays(SCRIPT_GLOBAL, GLOBAL_SCRIPT_GAME);
		FFCore.deallocateAllArrays(SCRIPT_PLAYER, SCRIPT_PLAYER_ACTIVE);
		switch(Quit)
		{
			case qSAVE:
			case qQUIT:
			case qGAMEOVER:
			case qRELOAD:
				//set a B item hack
				//Bwpn = Bweapon(Bpos);
				//game->bwpn = Bpos;
				//directItemB = directItem;
			case qCONT:
			case qSAVECONT:
			{
				playing_field_offset=56; // Fixes an issue with Hero being drawn wrong when quakeclk>0
				show_subscreen_dmap_dots=true;
				show_subscreen_numbers=true;
				show_subscreen_items=true;
				show_subscreen_life=true;
				show_ff_scripts=false;
				introclk=intropos=0;
				for ( int32_t q = 0; q < 256; q++ ) runningItemScripts[q] = 0; //Clear scripts that were running before. 

				initZScriptGlobalScript(GLOBAL_SCRIPT_END);
				FFCore.initZScriptHeroScripts(); //Should we not be calling this AFTER running the exit script!!
				FFCore.initZScriptDMapScripts(); //Should we not be calling this AFTER running the exit script!!
				FFCore.initZScriptItemScripts(); //Should we not be calling this AFTER running the exit script!!
				FFCore.initZScriptActiveSubscreenScript();
				FFCore.init_combo_doscript(); //clear running combo script data
				//Run Global script OnExit
				ZScriptVersion::RunScript(SCRIPT_GLOBAL, GLOBAL_SCRIPT_END, GLOBAL_SCRIPT_END);
			   
				if(!skipcont&&!get_bit(quest_rules,qr_NOCONTINUE)) game_over(get_bit(quest_rules,qr_NOSAVE));
				
				if(Quit==qSAVE)
				{
					save_game(false);
				}
				else if(Quit==qSAVECONT)
				{
					save_game(false);
					Quit = qCONT;
				}
				
				skipcont = 0;
				
				//restore user volume settings
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
			}
			break;
			
			case qWON:
			{
				show_subscreen_dmap_dots=true;
				show_subscreen_numbers=true;
				show_subscreen_items=true;
				show_subscreen_life=true;
				for ( int32_t q = 0; q < 256; q++ ) runningItemScripts[q] = 0; //Clear scripts that were running before. 

				initZScriptGlobalScript(GLOBAL_SCRIPT_END);
				FFCore.initZScriptHeroScripts(); //get ready for the onWin script
				FFCore.initZScriptDMapScripts();
				FFCore.initZScriptItemScripts();
				FFCore.initZScriptActiveSubscreenScript();
				FFCore.init_combo_doscript();
				//Run global script OnExit
				//ZScriptVersion::RunScript(SCRIPT_PLAYER, SCRIPT_PLAYER_WIN, SCRIPT_PLAYER_WIN); //runs in ending()
				//while(player_doscript) advanceframe(true); //Not safe. The script can run for only one frame. 
				//We need a special routine for win and death player scripts. Otherwise, they work. 
				ZScriptVersion::RunScript(SCRIPT_GLOBAL, GLOBAL_SCRIPT_END, GLOBAL_SCRIPT_END);
			
			
			
				ending();
			}
			break;
			
			case qINCQST:
			{
				Hero.setDontDraw(true);
				//Hero.setCharging(0);//don't have the sword out during the ending. 
				//Hero.setSwordClk(0);
				show_subscreen_dmap_dots=true;
				show_subscreen_numbers=true;
				show_subscreen_items=true;
				show_subscreen_life=true;
		
				initZScriptGlobalRAM();
				ZScriptVersion::RunScript(SCRIPT_GLOBAL, GLOBAL_SCRIPT_END);
				ending_scripted();
			}
			break;
		}
		FFCore.deallocateAllArrays(SCRIPT_GLOBAL, GLOBAL_SCRIPT_END);
		//Restore original palette before exiting for any reason!
		setMonochrome(false);
		doClearTint();
		Hero.setDontDraw(0);
		if(Quit != qCONT)
		{
			memset(disabledKeys, 0, sizeof(disabledKeys));
			memset(disable_control, 0, sizeof(disable_control));
			FFCore.user_files_init(); //Clear open FILE*!
			FFCore.user_dirs_init(); //Clear open FLIST*!
			FFCore.user_bitmaps_init(); //Clear open bitmaps
		}
		//Deallocate ALL ZScript arrays on ANY exit.
		FFCore.deallocateAllArrays();
		GameFlags = 0; //Clear game flags on ANY exit
		kill_sfx();
		music_stop();
		clear_to_color(screen,BLACK);
	}
	
	// clean up
	
	music_stop();
	kill_sfx();
	
	//restore user volume settings
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
	show_saving(screen);
	save_savedgames();
	save_game_configs();
	Triplebuffer.Destroy();
	set_gfx_mode(GFX_TEXT,80,25,0,0);
	//rest(250); // ???
	//  if(useCD)
	//    cd_exit();
	quit_game();
	Z_message("Armageddon Games web site: http://www.armageddongames.com\n");
	Z_message("Zelda Classic web site: http://www.zeldaclassic.com\n");
	Z_message("Zelda Classic wiki: http://www.shardstorm.com/ZCwiki/\n");
	
	__zc_debug_malloc_free_print_memory_leaks(); //this won't do anything without debug_malloc_logging defined.
	skipcont = 0;
	
	zscript_coloured_console.kill();
	coloured_console.kill();
	if(forceExit) //fix for the allegro at_exit() hang.
		exit(0);
		
	allegro_exit();
	return 0;
}
END_OF_MAIN()


void remove_installed_timers()
{
    al_trace("Removing timers. \n");
    remove_int(update_logic_counter);
    Z_remove_timers();
#ifdef _SCRIPT_COUNTER
    remove_int(update_script_counter);
#endif
    
}


void delete_everything_else() //blarg.
{
    delete_combo_aliases();
    reset_subscr_items();
    delete_selectors();
    Sitems.clear();
    
}

void quit_game()
{
	script_drawing_commands.Dispose(); //for allegro bitmaps
	
	remove_installed_timers();
	delete_everything_else();
	
	al_trace("Freeing Data: \n");
	
	if(game) delete game;
	
	if(data) unload_datafile(data);
	
	if(fontsdata) unload_datafile(fontsdata);
	
	if(sfxdata) unload_datafile(sfxdata);
	
	//if(mididata) unload_datafile(mididata);
	//  if(mappic)
	//    destroy_bitmap(mappic);
	
	al_trace("Bitmaps... \n");
	destroy_bitmap(framebuf);
	destroy_bitmap(scrollbuf);
	destroy_bitmap(tmp_scr);
	destroy_bitmap(screen2);
	destroy_bitmap(tmp_bmp);
	destroy_bitmap(fps_undo);
	destroy_bitmap(prim_bmp);
	set_clip_state(msg_bg_display_buf, 1);
	destroy_bitmap(msg_bg_display_buf);
	set_clip_state(msg_txt_display_buf, 1);
	destroy_bitmap(msg_txt_display_buf);
	set_clip_state(msg_portrait_display_buf, 1);
	destroy_bitmap(msg_portrait_display_buf);
	destroy_bitmap(msg_txt_bmp_buf);
	destroy_bitmap(msg_menu_bmp_buf);
	destroy_bitmap(msg_bg_bmp_buf);
	destroy_bitmap(msg_portrait_bmp_buf);
	set_clip_state(pricesdisplaybuf, 1);
	destroy_bitmap(pricesdisplaybuf);
	destroy_bitmap(zcmouse[0]);
	destroy_bitmap(zcmouse[1]);
	destroy_bitmap(zcmouse[2]);
	destroy_bitmap(zcmouse[3]);
	destroy_bitmap(script_menu_buf);
	destroy_bitmap(f6_menu_buf);
	destroy_bitmap(darkscr_bmp_curscr);
	destroy_bitmap(darkscr_bmp_curscr_trans);
	destroy_bitmap(darkscr_bmp_scrollscr);
	destroy_bitmap(darkscr_bmp_scrollscr_trans);
	destroy_bitmap(lightbeam_bmp);
	
	al_trace("Subscreens... \n");
	
	for(int32_t i=0; i<4; i++)
	{
		for(int32_t j=0; j<MAXSUBSCREENITEMS; j++)
		{
			switch(custom_subscreen[i].objects[j].type)
			{
			case ssoTEXT:
			case ssoTEXTBOX:
			case ssoCURRENTITEMTEXT:
			case ssoCURRENTITEMCLASSTEXT:
				if(custom_subscreen[i].objects[j].dp1 != NULL) delete[](char *)custom_subscreen[i].objects[j].dp1;
			}
		}
	}
	
	al_trace("SFX... \n");
	zcmusic_exit();
	
	for(int32_t i=0; i<WAV_COUNT; i++)
	{
		delete [] sfx_string[i];
		
		if(customsfxdata[i].data!=NULL)
		{
//      delete [] customsfxdata[i].data;
			zc_free(customsfxdata[i].data);
		}
	}
	
	al_trace("Misc... \n");
	
	for(int32_t i=0; i<WPNCNT; i++)
	{
		delete [] weapon_string[i];
	}
	
	for(int32_t i=0; i<ITEMCNT; i++)
	{
		delete [] item_string[i];
	}
	
	for(int32_t i=0; i<eMAXGUYS; i++)
	{
		delete [] guy_string[i];
	}
	
	al_trace("Script buffers... \n");
	
	for(int32_t i=0; i<NUMSCRIPTFFC; i++)
	{
		if(ffscripts[i]!=NULL) delete ffscripts[i];
		ffscripts[i] = NULL;
	}
	for(int32_t i=0; i<NUMSCRIPTITEM; i++)
	{
		if(itemscripts[i]!=NULL) delete itemscripts[i];
		itemscripts[i] = NULL;
	}
	for(int32_t i=0; i<NUMSCRIPTGUYS; i++)
	{
		if(guyscripts[i]!=NULL) delete guyscripts[i];
		guyscripts[i] = NULL;
	}
	for(int32_t i=0; i<NUMSCRIPTWEAPONS; i++)
	{
		if(ewpnscripts[i]!=NULL) delete ewpnscripts[i];
		ewpnscripts[i] = NULL;
	}
	for(int32_t i=0; i<NUMSCRIPTWEAPONS; i++)
	{
		if(lwpnscripts[i]!=NULL) delete lwpnscripts[i];
		lwpnscripts[i] = NULL;
	}
	for(int32_t i=0; i<NUMSCRIPTSCREEN; i++)
	{
		if(screenscripts[i]!=NULL) delete screenscripts[i];
		screenscripts[i] = NULL;
	}
	for(int32_t i=0; i<NUMSCRIPTGLOBAL; i++)
	{
		if(globalscripts[i]!=NULL) delete globalscripts[i];
		globalscripts[i] = NULL;
	}
	for(int32_t i=0; i<NUMSCRIPTPLAYER; i++)
	{
		if(playerscripts[i]!=NULL) delete playerscripts[i];
		playerscripts[i] = NULL;
	}
	for(int32_t i=0; i<NUMSCRIPTSDMAP; i++)
	{
		if(dmapscripts[i]!=NULL) delete dmapscripts[i];
		dmapscripts[i] = NULL;
	}
	for(int32_t i=0; i<NUMSCRIPTSITEMSPRITE; i++)
	{
		if(itemspritescripts[i]!=NULL) delete itemspritescripts[i];
		itemspritescripts[i] = NULL;
	}
	for(int32_t i=0; i<NUMSCRIPTSCOMBODATA; i++)
	{
		if(comboscripts[i]!=NULL) delete comboscripts[i];
		comboscripts[i] = NULL;
	}
	
	delete zscriptDrawingRenderTarget;
	
	//for(int32_t i=0; i<map_count*MAPSCRS; i++)
	//{
	//if(TheMaps[i].data != NULL) delete [] TheMaps[i].data;
	//if(TheMaps[i].sflag != NULL) delete [] TheMaps[i].sflag;
	//if(TheMaps[i].cset != NULL) delete [] TheMaps[i].cset;
	//}
	al_trace("Screen Data... \n");
	
	al_trace("Deleting quest buffers... \n");
	del_qst_buffers();
	
	if(qstdir) zc_free(qstdir);
	
	if(qstpath) zc_free(qstpath);
	
	//if(TheMaps != NULL) zc_free(TheMaps);
	//if(ZCMaps != NULL) zc_free(ZCMaps);
	//  dumb_exit();
}

bool isSideViewGravity(int32_t t)
{
	return (((tmpscr[t].flags7 & fSIDEVIEW)!=0) != (DMaps[currdmap].sideview));
}

bool isSideViewHero(int32_t t)
{
	return !ignoreSideview && isSideViewGravity(t);
}


int32_t d_timer_proc(int32_t, DIALOG *, int32_t)
{
    return D_O_K;
}






/////////////////////////////////////////////////
// zc_malloc
/////////////////////////////////////////////////

//Want Logging:
//Set this to 1 to allow zc_malloc/zc_free to track pointers and
//write logging data to allegro.log
#define ZC_DEBUG_MALLOC_WANT_LOGGING_INFO 0


#include "vectorset.h"

#if (defined(NDEBUG) || !defined(_DEBUG)) && (ZC_DEBUG_MALLOC_ENABLED) && (ZC_DEBUG_MALLOC_WANT_LOGGING_INFO) //this is not fun with debug
#define ZC_WANT_DETAILED_MALLOC_LOGGING 1
#endif


#if ZC_WANT_DETAILED_MALLOC_LOGGING
size_t totalBytesAllocated = 0;
typedef vectorset<void*> debug_malloc_pool_type;
debug_malloc_pool_type debug_zc_malloc_allocated_pool;
#endif

void* __zc_debug_malloc(size_t numBytes, const char* file, int32_t line)
{
#if ZC_WANT_DETAILED_MALLOC_LOGGING
    static bool zcDbgMallocInit = false;
    
    if(!zcDbgMallocInit)
    {
        zcDbgMallocInit = true;
        debug_zc_malloc_allocated_pool.reserve(1 << 17);
        //yeah. completely ridiculous... there's no reason zc should ever need this many..
        //BUT it does... go figure
    }
    
    totalBytesAllocated += numBytes;
    
    //char buf[1024];
    //sprintf(buf, "%i : %s, line %i, %u bytes allocated.\n", 0, file, line, numBytes);
    //al_trace("%s", buf);
    
    al_trace("info: %i : %s, line %i, %u bytes, pool size %u, total %u,",
             0,
             file,
             line,
             numBytes,
             debug_zc_malloc_allocated_pool.size(),
             totalBytesAllocated / 1024
            );
#endif
            
    ZC_MALLOC_ALWAYS_ASSERT(numBytes != 0);
    void* p = malloc(numBytes);
    
#if ZC_WANT_DETAILED_MALLOC_LOGGING
    al_trace("at address %x\n", (int32_t)p);
    
    if(!p)
        al_trace("____________ ERROR: __zc_debug_malloc: returned null. out of memory.\n");
        
    debug_malloc_pool_type::insert_iterator_type it = debug_zc_malloc_allocated_pool.insert(p);
    
    if(!it.second)
        al_trace("____________ ERROR: malloc returned identical address to one in use... No way Jose!\n");
        
#endif
        
    return p;
}


void __zc_debug_free(void* p, const char* file, int32_t line)
{
    ZC_MALLOC_ALWAYS_ASSERT(p != 0);
    
#if ZC_WANT_DETAILED_MALLOC_LOGGING
    al_trace("alloc info: %i : %s line %i, freeing memory at address %x\n", 0, file, line, (int32_t)p);
    
    size_t numErased = debug_zc_malloc_allocated_pool.erase(p);
    
    if(numErased == 0)
        al_trace("____________ ERROR: __zc_debug_free: no known ptr to memory exists. ..attempting to free it anyways.\n");
        
#endif
        
    free(p);
}


void __zc_debug_malloc_free_print_memory_leaks()
{
#if ZC_WANT_DETAILED_MALLOC_LOGGING
    al_trace("LOGGING INFO FROM debug_zc_malloc_allocated_pool:\n");
    
    for(debug_malloc_pool_type::iterator it = debug_zc_malloc_allocated_pool.begin();
            it != debug_zc_malloc_allocated_pool.end();
            ++it
       )
    {
        al_trace("block at address %x.\n", (int32_t)*it);
    }
    
#endif
}


void __zc_always_assert(bool e, const char* expression, const char* file, int32_t line)
{
    if(!e)
    {
        //for best results set a breakpoint in here.
        char buf[1024];
        sprintf(buf, "ASSERTION FAILED! : %s, %s line %i\n", expression, file, line);
        
        al_trace("%s", buf);
        set_gfx_mode(GFX_TEXT, 0, 0, 0, 0);
        allegro_message("%s", buf);
        //exit(-1); //flashing lights are probably enough.
    }
}


/*** end of zelda.cc ***/

