#include "qst.h"
// #include <allegro.h>
#include <allegro/system.h>
#include <emscripten.h>

#include <fstream>


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
  // if ((errno=allegro_init()) != 0)
  // {
  //   printf("Failed allegro_init %d!\n", errno);
  //   exit(1);
  // }

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
