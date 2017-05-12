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
// When !(pixels[0]&0x80): the 8 bytes are the TV palette indexes for
// the 8 pixels.
//
// Otherwise: check (BeebControlPixel)pixels[0]:
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
    uint8_t pixel0;
    uint8_t colours[2];
    uint8_t data0,data1;
};
#endif

union VideoDataHalfUnit {
    uint8_t pixels[8];
    uint64_t value;
#if BBCMICRO_FINER_TELETEXT
    VideoDataTeletextHalfUnit teletext;
#endif
};

CHECK_SIZEOF(VideoDataHalfUnit,8);

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
