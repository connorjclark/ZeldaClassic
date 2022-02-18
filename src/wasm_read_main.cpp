#include "qst.h"
#include <allegro.h>
#include <allegro/system.h>
#include <emscripten.h>

#include <fstream>

void check_cpu(void)
{
  cpu_family = 0;
  cpu_model = 0;
  cpu_capabilities = 0;
}

int filesize(const char* filename)
{
    std::ifstream in(filename, std::ifstream::ate | std::ifstream::binary);
    int length = in.tellg();
    printf("file length %s %d\n", filename, length);
    return length;
}

// std::string printSomeBytes(std::string filename)
// {
//   std::ifstream f;

//   f.open(filename, std::ios::binary);

//   if (f.fail()) {
//     //error
//   }

//   std::vector<unsigned char> bytes;

//   while (!f.eof())
//   {
//     unsigned char byte;

//     f >> byte;

//     if (f.fail())
//     {
//       //error
//       break;
//     }

//     bytes.push_back(byte);
//   }

//   f.close();

//   printf("len %d\n", bytes.size());
//   for (int i = 0; i < 10; i++) {
//     printf("%d %d\n", i, bytes[i]);
//   }
//   printf("...\n");
//   for (int i = 0; i < 10; i++) {
//     int j = bytes.size() - 10 + i;
//     printf("%d %d\n", j, bytes[j]);
//   }

//   return "ok";

//     // std::fstream fs;
//     // fs.open (filename, std::fstream::in | std::fstream::binary);
//     // if (fs) {
//     //     fs.close();
//     //     return "File '" + filename + "' exists!";
//     // } else {
//     //     return "File '" + filename + "' does NOT exist!";
//     // }
// }

std::string readf(std::string filename) {
  std::ifstream ifs(filename);
  std::string content( (std::istreambuf_iterator<char>(ifs) ),
                       (std::istreambuf_iterator<char>()    ) );
  return content;
}

int ex_main(void)
{
   /* you should always do this at the start of Allegro programs */
   if (allegro_init() != 0)
      return 1;

   /* set up the keyboard handler */
   install_keyboard(); 

   /* set a graphics mode sized 320x200 */
   if (set_gfx_mode(GFX_AUTODETECT, 320, 200, 0, 0) != 0) {
      if (set_gfx_mode(GFX_SAFE, 320, 200, 0, 0) != 0) {
	 set_gfx_mode(GFX_TEXT, 0, 0, 0, 0);
	 allegro_message("Unable to set any graphic mode\n%s\n", allegro_error);
	 return 1;
      }
   }

   /* set the color palette */
   set_palette(desktop_palette);

   /* clear the screen to white */
   clear_to_color(screen, makecol(255, 255, 255));

   /* you don't need to do this, but on some platforms (eg. Windows) things
    * will be drawn more quickly if you always acquire the screen before
    * trying to draw onto it.
    */
   acquire_screen();

   /* write some text to the screen with black letters and transparent background */
   textout_centre_ex(screen, font, "Hello, world!", SCREEN_W/2, SCREEN_H/2, makecol(0,0,0), -1);

   /* you must always release bitmaps before calling any input functions */
   release_screen();

   /* wait for a key press */
   readkey();

   return 0;
}

extern char *weapon_string[];
extern char *item_string[];
extern char *sfx_string[];
extern char *guy_string[];
extern zcmodule moduledata;
extern ZModule zcm; //modules

void init() {
  // done in zelda.cpp / zquest.cpp main, need to do here too

  resolve_password(zeldapwd);
  resolve_password(datapwd);

  for(int i=0; i<WAV_COUNT; i++)
  {
    customsfxdata[i].data=NULL;
    sfx_string[i] = new char[36];
    memset(sfx_string[i], 0, 36);
  }

  for(int i=0; i<WPNCNT; i++)
  {
    weapon_string[i] = new char[64];
    memset(weapon_string[i], 0, 64);
  }
  
  for(int i=0; i<ITEMCNT; i++)
  {
    item_string[i] = new char[64];
    memset(item_string[i], 0, 64);
  }
  
  for(int i=0; i<eMAXGUYS; i++)
  {
    guy_string[i] = new char[64];
    memset(guy_string[i], 0, 64);
  }
  
  // for(int i=0; i<NUMSCRIPTFFC; i++)
  // {
  //     ffscripts[i] = new script_data();
  // }

  // TODO: `allegro_init` probably won't work without huge effort.
  //       Need to use allegro5 ...
  if ((errno=allegro_init()) != 0)
  {
    printf("Failed allegro_init %d!\n", errno);
    exit(1);
  }

  // if ( !(zcm.init(true)) ) 
  // {
	//   exit(1);
  // }
  // zcm.load(false);

  // printf("load datafile: %s\n", moduledata.datafiles[sfx_dat]);
  // if((sfxdata=load_datafile(moduledata.datafiles[sfx_dat]))==NULL)
  // {
  //   printf("failed\n");
  //   exit(1);
  // }

  get_qst_buffers();
}

int get_qst_buffers();

extern "C" {

EMSCRIPTEN_KEEPALIVE
const char* read_qst_file() {
  ex_main();
  return "";
  init();

  zquestheader QHeader;
  miscQdata QMisc;

  byte skip_flags[4];
  for (int i=0; i<4; ++i) {
    skip_flags[i]=0;
  }

  printf("starting...\n");

  const char* qstpath = "/quests/1st.qst";
  byte printmetadata = 0;
  bool compressed = true;
  bool encrypted = true;
  int ret = loadquest(
    qstpath, &QHeader, &QMisc, tunes+ZC_MIDI_COUNT, false,
    compressed, encrypted, true, skip_flags, printmetadata);

  printf("done\n");
  printf("title: %s\n", QHeader.title);
  printf("author: %s\n", QHeader.author);

  return QHeader.title;
}

}
