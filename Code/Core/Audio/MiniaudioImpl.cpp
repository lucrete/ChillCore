// Single translation unit that compiles miniaudio's implementation. The
// header section of stb_vorbis is included first so miniaudio's Vorbis
// decoding backend (MA_HAS_VORBIS) activates. stb_vorbis.c itself is
// compiled separately as its own translation unit and provides the
// implementation symbols at link time.

#define STB_VORBIS_HEADER_ONLY
#include "stb_vorbis.c"

#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"
