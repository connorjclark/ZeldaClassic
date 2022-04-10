//--------------------------------------------------------
//  Zelda Classic
//  by Jeremy Craner, 1999-2000
//
//  maps.cc
//
//  Map and screen scrolling stuff for zelda.cc
//
//--------------------------------------------------------

#ifndef _MAPS_H_
#define _MAPS_H_
#include "zdefs.h"

#define DRIEDLAKE ((tmpscr->flags7 & fWHISTLEWATER) && (whistleclk>=88))
int32_t COMBOPOS(int32_t x, int32_t y);
int32_t COMBOX(int32_t pos);
int32_t COMBOY(int32_t pos);

extern bool triggered_screen_secrets;

void debugging_box(int32_t x1, int32_t y1, int32_t x2, int32_t y2);
void clear_dmap(word i);
void clear_dmaps();
int32_t count_dmaps();
int32_t isdungeon(int32_t dmap = -1, int32_t scr = -1);
bool canPermSecret(int32_t dmap = -1, int32_t scr = -1);
int32_t MAPCOMBO(int32_t x,int32_t y);
int32_t MAPCOMBOzq(int32_t x,int32_t y);
int32_t MAPFFCOMBO(int32_t x,int32_t y);
int32_t MAPCSET(int32_t x,int32_t y);
int32_t MAPFLAG(int32_t x,int32_t y);
int32_t MAPCOMBOFLAG(int32_t x,int32_t y);
int32_t MAPFFCOMBOFLAG(int32_t x,int32_t y);
int32_t COMBOTYPE(int32_t x,int32_t y);
int32_t FFCOMBOTYPE(int32_t x, int32_t y);
int32_t FFORCOMBO(int32_t x, int32_t y);
int32_t FFORCOMBOTYPE(int32_t x, int32_t y);
int32_t FFORCOMBO_L(int32_t layer, int32_t x, int32_t y);
int32_t FFORCOMBOTYPE_L(int32_t layer, int32_t x, int32_t y);
int32_t MAPCOMBO2(int32_t layer,int32_t x,int32_t y);
int32_t MAPCOMBO3(int32_t map, int32_t screen, int32_t layer,int32_t x,int32_t y, bool secrets = false);
int32_t MAPCOMBO3(int32_t map, int32_t screen, int32_t layer,int32_t pos, bool secrets = false);
int32_t MAPCSET2(int32_t layer,int32_t x,int32_t y);
int32_t MAPFLAG2(int32_t layer,int32_t x,int32_t y);
int32_t MAPCOMBOFLAG2(int32_t layer,int32_t x,int32_t y);
int32_t COMBOTYPE2(int32_t layer,int32_t x,int32_t y);

bool HASFLAG(int32_t flag, int32_t layer, int32_t pos);
bool HASFLAG_ANY(int32_t flag, int32_t pos);

//specific layers 1 to 6 -Z
int32_t MAPCOMBOL(int32_t layer,int32_t x,int32_t y);
int32_t MAPCSETL(int32_t layer,int32_t x,int32_t y);
int32_t MAPFLAGL(int32_t layer,int32_t x,int32_t y);
int32_t COMBOTYPEL(int32_t layer,int32_t x,int32_t y);
int32_t MAPCOMBOFLAGL(int32_t layer,int32_t x,int32_t y);

int32_t getFFCAt(int32_t x, int32_t y);
void eventlog_mapflags();
void setmapflag(int32_t mi2, int32_t flag);
void setmapflag(int32_t flag = 32); // 32 = mSPECIALITEM
void unsetmapflag(int32_t mi2, int32_t flag, bool anyflag=false);
void unsetmapflag(int32_t flag = 32);
bool getmapflag(int32_t flag = 32); // 32 = mSPECIALITEM
int32_t WARPCODE(int32_t dmap,int32_t scr,int32_t dw);
void update_combo_cycling();
bool isSVLadder(int32_t x, int32_t y);
bool isSVPlatform(int32_t x, int32_t y);
bool checkSVLadderPlatform(int32_t x, int32_t y);
bool iswater(int32_t combo);
int32_t iswaterex(int32_t combo, int32_t map, int32_t screen, int32_t layer, int32_t x, int32_t y, bool secrets = true, bool fullcheck = false, bool LayerCheck = true, bool ShallowCheck = false, bool hero = true);
int32_t iswaterexzq(int32_t combo, int32_t map, int32_t screen, int32_t layer, int32_t x, int32_t y, bool secrets = true, bool fullcheck = false, bool LayerCheck = true);
bool iswater_type(int32_t type);
bool ispitfall(int32_t combo);
bool ispitfall_type(int32_t type);
bool ispitfall(int32_t x, int32_t y);
int32_t getpitfall(int32_t x, int32_t y);
bool isGrassType(int32_t type);
bool isFlowersType(int32_t type);
bool isBushType(int32_t type);
bool isGenericType(int32_t type);
bool isSlashType(int32_t type);
bool isCuttableNextType(int32_t type);
bool isTouchyType(int32_t type);
bool isCuttableType(int32_t type);
bool isCuttableItemType(int32_t type);
bool isstepable(int32_t combo);                                 //can use ladder on it
bool isHSComboFlagType(int32_t type);
bool isHSGrabbable(newcombo const& cmb);
bool isSwitchHookable(newcombo const& cmb);
int32_t check_hshot(int32_t layer, int32_t x, int32_t y, bool switchhook);
bool ishookshottable(int32_t bx, int32_t by);
bool ishookshottable(int32_t map, int32_t screen, int32_t bx, int32_t by);
bool hiddenstair(int32_t tmp, bool redraw);                      // tmp = index of tmpscr[]
bool hiddenstair2(mapscr *s, bool redraw);                      
bool remove_screenstatecombos2(mapscr *s, mapscr *t, int32_t what1, int32_t what2);
bool remove_lockblocks(int32_t tmp);                // tmp = index of tmpscr[]
bool remove_bosslockblocks(int32_t tmp);            // tmp = index of tmpscr[]
bool remove_chests(int32_t tmp);                    // tmp = index of tmpscr[]
bool remove_lockedchests(int32_t tmp);              // tmp = index of tmpscr[]
bool remove_bosschests(int32_t tmp);                // tmp = index of tmpscr[]
bool overheadcombos(mapscr *s);
void delete_fireball_shooter(mapscr *s, int32_t i);
void hidden_entrance(int32_t tmp,bool refresh, bool high16only=false,int32_t single=-1);
void hidden_entrance2(mapscr *s, mapscr *t, bool high16only=false,int32_t single=-1);
void update_freeform_combos();
bool findentrance(int32_t x, int32_t y, int32_t flag, bool setflag);
bool hitcombo(int32_t x, int32_t y, int32_t combotype);
bool hitflag(int32_t x, int32_t y, int32_t flagtype);
int32_t nextscr(int32_t dir);
void bombdoor(int32_t x,int32_t y);
void do_scrolling_layer(BITMAP *bmp, int32_t type, int32_t layer, mapscr* basescr, int32_t x, int32_t y, bool scrolling, int32_t tempscreen);
void do_layer(BITMAP *bmp, int32_t type, int32_t layer, mapscr* basescr, int32_t x, int32_t y, int32_t tempscreen, bool scrolling = false, bool drawprimitives=false);
void put_walkflags(BITMAP *dest,int32_t x,int32_t y,int32_t xofs,int32_t yofs, word cmbdat,int32_t lyr);
void do_walkflags(BITMAP *dest,mapscr* layer,int32_t x, int32_t y, int32_t tempscreen);
void do_effectflags(BITMAP *dest,mapscr* layer,int32_t x, int32_t y, int32_t tempscreen);
void do_primitives(BITMAP *bmp, int32_t type, mapscr *layer, int32_t x, int32_t y);
void do_script_draws(BITMAP *bmp, mapscr *layer, int32_t x, int32_t y, bool hideLayer7 = false);
void calc_darkroom_combos(bool scrolling = false);
void draw_screen(mapscr* this_screen, bool showhero=true);
void put_door(BITMAP *dest,int32_t t,int32_t pos,int32_t side,int32_t type,bool redraw,bool even_walls=false);
void over_door(BITMAP *dest,int32_t t, int32_t pos,int32_t side);
void putdoor(BITMAP *dest,int32_t t,int32_t side,int32_t door,bool redraw=true,bool even_walls=false);
void showbombeddoor(BITMAP *dest, int32_t side);
void openshutters();
void loadscr2(int32_t tmp,int32_t scr,int32_t);
void loadscr(int32_t tmp,int32_t destdmap,int32_t scr,int32_t ldir,bool overlay);
void putscr(BITMAP* dest,int32_t x,int32_t y,mapscr* screen);
void putscrdoors(BITMAP *dest,int32_t x,int32_t y,mapscr* screen);
bool _walkflag(int32_t x,int32_t y,int32_t cnt);
bool _walkflag(int32_t x,int32_t y,int32_t cnt,zfix const& switchblockstate);
bool _effectflag(int32_t x,int32_t y,int32_t cnt, int32_t layer = -1);
bool _walkflag(int32_t x,int32_t y,int32_t cnt, mapscr* m);
bool _walkflag(int32_t x,int32_t y,int32_t cnt, mapscr* m, mapscr* s1, mapscr* s2);
bool _walkflag_layer(int32_t x,int32_t y,int32_t cnt, mapscr* m);
bool _effectflag_layer(int32_t x,int32_t y,int32_t cnt, mapscr* m);
bool water_walkflag(int32_t x,int32_t y,int32_t cnt);
bool hit_walkflag(int32_t x,int32_t y,int32_t cnt);
void map_bkgsfx(bool on);
void toggle_switches(dword flags, bool entry);
void toggle_switches(dword flags, bool entry, mapscr* m, mapscr* t);

//
void doDarkroomCircle(int32_t cx, int32_t cy, byte glowRad,BITMAP* dest=NULL,BITMAP* transdest=NULL);
void doDarkroomCone(int32_t sx, int32_t sy, byte glowRad, int32_t dir, BITMAP* dest=NULL,BITMAP* transdest=NULL);

//extern FONT *lfont;
/****  View Map  ****/
extern int32_t mapres;
bool displayOnMap(int32_t x, int32_t y);
void ViewMap();
int32_t onViewMap();

//extern bool FuckIAlreadyDrewThatAlready[ 7 ];
#endif

/*** end of maps.cc ***/

