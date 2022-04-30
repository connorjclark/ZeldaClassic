#include <stddef.h>
#include "zcmusic.h"
#include "zsys.h"
#include "zc_malloc.h"
#include "mutex.h"
#include <SDL2/SDL_mixer.h>
#include <gme.h>

#ifdef __EMSCRIPTEN__
#include "emscripten_utils.h"
#endif

int32_t zcmusic_bufsz = 64;
static std::vector<ZCMUSIC*> playlist;
mutex playlistmutex;

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
  mutex_init(&playlistmutex);

  if (Mix_OpenAudioDevice(22050, MIX_DEFAULT_FORMAT, MIX_DEFAULT_CHANNELS, 0, NULL, 0) < 0)
  {
    al_trace("Mix_OpenAudioDevice error: %s\n", Mix_GetError());
    return false;
  }

  return true;
}
bool zcmusic_poll(int32_t flags) {
  mutex_lock(&playlistmutex);

  std::vector<ZCMUSIC*>::iterator b = playlist.begin();
  while(b != playlist.end())
  {
      switch((*b)->playing)
      {
      case ZCM_STOPPED:
          // if it has stopped, remove it from playlist;
          b = playlist.erase(b);
          break;
      case ZCM_PLAYING:
          (*b)->position++;
          break;
      }
      b++;
  }

  mutex_unlock(&playlistmutex);

  return true;
}
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
  music->playing = ZCM_STOPPED;
  music->position = 0;
  music->mus = mus;

  return music;
}
ZCMUSIC const *zcmusic_load_file_ex(char *filename) { return NULL; }
bool zcmusic_play(ZCMUSIC *zcm, int32_t vol)
{
  if (Mix_PlayMusic(zcm->mus, -1) < 0)
  {
    zcm->playing = ZCM_STOPPED;
    al_trace("Mix_PlayMusic error: %s\n", Mix_GetError());
    return 1;
  }

  // In case it was paused.
  Mix_ResumeMusic();
  zcm->playing = ZCM_PLAYING;

  mutex_lock(&playlistmutex);
  playlist.push_back(zcm);
  mutex_unlock(&playlistmutex);

  return 0;
}
bool zcmusic_pause(ZCMUSIC *zcm, int32_t pause)
{
  if (zcm == NULL) return false;

  mutex_lock(&playlistmutex);

  if (pause == ZCM_TOGGLE) {
    pause = (zcm->playing == ZCM_PAUSED) ? ZCM_RESUME : ZCM_PAUSE;
  }

  if (pause == ZCM_RESUME) {
    Mix_ResumeMusic();
    zcm->playing = ZCM_PLAYING;
  } else if (pause == ZCM_PAUSE) {
    Mix_PauseMusic();
    zcm->playing = ZCM_PAUSED;
  }

  mutex_unlock(&playlistmutex);
  return true;
}
bool zcmusic_stop(ZCMUSIC *zcm)
{
  if (zcm == NULL) return false;

  mutex_lock(&playlistmutex);

  Mix_HaltMusic();
  zcm->playing = 0;

  mutex_unlock(&playlistmutex);
  return true;
}
void zcmusic_unload_file(ZCMUSIC *&zcm)
{
  if (zcm == NULL)
    return;

  mutex_lock(&playlistmutex);

  for (auto it = playlist.begin(); it != playlist.end(); it++) {
    if (*it == zcm) {
      it = playlist.erase(it);
      break;
    }
  }

  Mix_FreeMusic(zcm->mus);
  zcm->mus = NULL;
  zc_free(zcm);
  zcm = NULL;

  mutex_unlock(&playlistmutex);
}
int32_t zcmusic_get_tracks(ZCMUSIC *zcm) {
  if (zcm == NULL)
    return 0;

  int result = Mix_GetNumTracks(zcm->mus);
  if (result == -1) return 0;
  return result;
}
std::string zcmusic_get_track_name(ZCMUSIC *zcm, int track) {
  if (zcm == NULL)
    return "";

  // return Mix_GetMusicTitleTag(zcm->mus, track);
  return Mix_GetMusicTitleTag(zcm->mus);
}
int32_t zcmusic_change_track(ZCMUSIC *zcm, int32_t tracknum) {
  Mix_StartTrack(tracknum);
  return 0;
}
int32_t zcmusic_get_curpos(ZCMUSIC *zcm) { return 0; }
void zcmusic_set_curpos(ZCMUSIC *zcm, int32_t value) {}
void zcmusic_set_speed(ZCMUSIC *zcm, int32_t value) {}
