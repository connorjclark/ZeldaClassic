#include "play_midi.h"
#include "midi.h"

#ifdef __EMSCRIPTEN__
#include <SDL2/SDL_mixer.h>
#endif

MIDI *current_midi = NULL;
Mix_Music *current_mus = NULL;
bool has_opened_audio;

int play_midi_em(MIDI *midi, int32_t loop)
{
  if (!midi)
  {
    if (current_mus)
    {
      Mix_HaltMusic();
      current_midi = NULL;
      midi_pos = 0;
    }
    return 0;
  }

  if (current_midi == midi)
  {
    return 0;
  }

  if (!has_opened_audio)
  {
    // if (Mix_OpenAudio(22050, MIX_DEFAULT_FORMAT, MIX_DEFAULT_CHANNELS, 0) < 0)
    if (Mix_OpenAudioDevice(22050, MIX_DEFAULT_FORMAT, MIX_DEFAULT_CHANNELS, 0, NULL, 0) < 0)
    {
      al_trace("Mix_OpenAudioDevice error: %s\n", Mix_GetError());
      return 1;
    }
    has_opened_audio = true;
  }

  if (save_midi("/tmp/midi.mid", midi))
  {
    al_trace("save_midi error\n");
    return 1;
  }

  if (current_mus)
  {
    Mix_FreeMusic(current_mus);
    current_mus = NULL;
  }

  current_mus = Mix_LoadMUS("/tmp/midi.mid");
  if (!current_mus)
  {
    al_trace("Mix_LoadMUS error: %s\n", Mix_GetError());
    return 1;
  }

  if (Mix_PlayMusic(current_mus, loop == 0 ? 1 : -1) < 0)
  {
    al_trace("Mix_PlayMusic error: %s\n", Mix_GetError());
    return 1;
  }

  midi_pos = 1;
  current_midi = midi;
  return 0;
}

int zc_play_midi(MIDI *midi, int loop)
{
#ifdef __EMSCRIPTEN__
  return play_midi_em(midi, loop);
#else
  return play_midi(midi, loop);
#endif
}

void zc_midi_pause()
{
#ifdef __EMSCRIPTEN__
  Mix_PauseMusic();
#else
  midi_pause();
#endif
}

void zc_midi_resume()
{
#ifdef __EMSCRIPTEN__
  Mix_ResumeMusic();
#else
  midi_resume();
#endif
}

void zc_stop_midi()
{
  zc_play_midi(NULL, false);
}

void zc_set_volume(int digi_volume, int midi_volume)
{
#ifdef __EMSCRIPTEN__
  Mix_VolumeMusic(midi_volume / 2);
#else
  set_volume(digi_volume, midi_volume);
#endif
}
