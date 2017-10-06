#ifndef HEADER_8E403EACE2EB4BCD97FDF5A12083CE23
#define HEADER_8E403EACE2EB4BCD97FDF5A12083CE23

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#include "conf.h"

#include <shared/enum_decl.h>
#include "video.inl"
#include <shared/enum_end.h>

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// Holds 0.5us of video output data: 8 Mode 0/3 pixels, 4 Mode 1/4/6
// pixels, 2 Mode 2/5 pixels, 1 "Mode 8" pixel, 0.5 Mode 7 glyphs.
//
// When (type&0x8000)==0, the 8 words are the 12 bpp colour
// data for the 8 pixels.
//
// <pre>
//   f   e   d   c   b   a   9   8   7   6   5   4   3   2   1   0
// +---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+
// | 0   0   0   0 |      red      |     green     |      blue     |
// +---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+
// </pre>
//
// Otherwise: the type is (BeebControlPixel)type:
//
#if BBCMICRO_FINER_TELETEXT
// Teletext: teletext data for this column over two half-scanlines.
#endif
//
// HSync: hsync is active. Ignore pixels[1...].
//
// VSync: vsync is active. Ignore pixels[1...].

#if BBCMICRO_FINER_TELETEXT
struct VideoDataTeletextHalfUnit {
    uint16_t type;
    uint8_t colours[2];
    uint8_t data0,data1;
};
#endif

struct VideoDataBitmapHalfUnit {
    uint16_t pixels[8];
};

union VideoDataHalfUnit {
    uint16_t type;
    VideoDataBitmapHalfUnit bitmap;
#if BBCMICRO_FINER_TELETEXT
    VideoDataTeletextHalfUnit teletext;
#endif
};

CHECK_SIZEOF(VideoDataHalfUnit,16);

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// Holds 1.0us of video output data.
struct VideoDataUnit {
    VideoDataHalfUnit a,b;
};
typedef struct VideoDataUnit VideoDataUnit;

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#endif
