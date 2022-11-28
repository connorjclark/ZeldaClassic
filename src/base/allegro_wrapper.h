// Wraps <allegro.h> with some temporary redefines to avert conflicts.

#ifndef ALLEGRO_H

// Grab GLIBC constants used below.
#  ifdef ALLEGRO_UNIX
#    include <features.h>
#  endif

#define ALLEGRO_NO_FIX_ALIASES
#  include <allegro.h>

// https://www.allegro.cc/forums/thread/613716
#ifdef ALLEGRO_LEGACY_MSVC
   #include <limits.h>
   #ifdef PATH_MAX
      #undef PATH_MAX
   #endif
   #define PATH_MAX MAX_PATH
#endif

#endif
