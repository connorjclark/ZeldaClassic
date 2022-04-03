#include <stddef.h>
#include "zcmusic.h"
#include "zsys.h"
#include "zc_malloc.h"
#include <SDL2/SDL_mixer.h>

#ifdef __EMSCRIPTEN__
#include "emscripten_utils.h"
#endif

int32_t zcmusic_bufsz = 64;

void zcm_extract_name(char *path, char *name, int32_t type)
{
  int32_t l = (int32_t)strlen(path);
  int32_t i = l;

  while (i > 0 && path[i - 1] != '/' && path[i - 1] != '\\')
    --i;

  int32_t n = 0;

  if (type == FILENAME8__)
  {
    while (i < l && n < 8 && path[i] != '.')
      name[n++] = path[i++];
  }
  else if (type == FILENAME8_3)
  {
    while (i < l && n < 12)
      name[n++] = path[i++];
  }
  else
  {
    while (i < l)
      name[n++] = path[i++];
  }

  name[n] = 0;
}

bool zcmusic_init(int32_t flags)
{
  if (Mix_OpenAudioDevice(22050, MIX_DEFAULT_FORMAT, MIX_DEFAULT_CHANNELS, 0, NULL, 0) < 0)
  {
    al_trace("Mix_OpenAudioDevice error: %s\n", Mix_GetError());
    return false;
  }

  return true;
}
bool zcmusic_poll(int32_t flags) { return false; }
void zcmusic_exit() {}

ZCMUSIC const *zcmusic_load_file(char *filename)
{
#ifdef __EMSCRIPTEN__
  if (strncmp("/_quests/", filename, strlen("/_quests/")) == 0)
  {
    em_fetch_file(filename);
  }
#endif

  Mix_Music *mus = Mix_LoadMUS(filename);
  if (!mus)
  {
    al_trace("Mix_LoadMUS error: %s\n", Mix_GetError());
    return NULL;
  }

  ZCMUSIC *music = (ZCMUSIC *)zc_malloc(sizeof(ZCMUSIC));
  zcm_extract_name(filename, music->filename, FILENAMEALL);
  music->filename[255] = '\0';
  music->track = 0;
  music->playing = 0;
  music->position = 0;
  music->mus = mus;

  return music;
}
ZCMUSIC const *zcmusic_load_file_ex(char *filename) { return NULL; }
bool zcmusic_play(ZCMUSIC *zcm, int32_t vol)
{
  if (Mix_PlayMusic(zcm->mus, -1) < 0)
  {
    zcm->playing = 0;
    al_trace("Mix_PlayMusic error: %s\n", Mix_GetError());
    return 1;
  }

  zcm->playing = 1;
}
bool zcmusic_pause(ZCMUSIC *zcm, int32_t pause)
{
  Mix_PauseMusic();
  zcm->playing = -1;
  return true;
}
bool zcmusic_stop(ZCMUSIC *zcm)
{
  Mix_HaltMusic();
  zcm->playing = 0;
  return true;
}
void zcmusic_unload_file(ZCMUSIC *&zcm)
{
  if (zcm == NULL)
    return;

  Mix_FreeMusic(zcm->mus);
  zc_free(zcm);
}
int32_t zcmusic_get_tracks(ZCMUSIC *zcm) { return 0; }
int32_t zcmusic_change_track(ZCMUSIC *zcm, int32_t tracknum) { return 0; }
int32_t zcmusic_get_curpos(ZCMUSIC *zcm) { return 0; }
void zcmusic_set_curpos(ZCMUSIC *zcm, int32_t value) {}
void zcmusic_set_speed(ZCMUSIC *zcm, int32_t value) {}
