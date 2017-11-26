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
// When type.x==0, the 8 words are the 12 bpp colour
// data for the 8 pixels.
//
// <pre>
//   f   e   d   c   b   a   9   8   7   6   5   4   3   2   1   0
// +---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+
// | 0   0   0   0 |      red      |     green     |      blue     |
// +---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+
// </pre>
//
// Otherwise: the type is (VideoDataType)type.x:
//
// Teletext: teletext data for this column over two half-scanlines.
//
// HSync: hsync is active. Ignore pixels[1...].
//
// VSync: vsync is active. Ignore pixels[1...].

struct VideoDataBitmapPixel {
    uint16_t b:4;
    uint16_t g:4;
    uint16_t r:4;
    uint16_t x:4;
};

CHECK_SIZEOF(VideoDataBitmapPixel,2);

#if VIDEO_TRACK_METADATA
struct VideoDataUnitMetadata {
    uint16_t addr;
};

static const VideoDataUnitMetadata NULL_VIDEO_METADATA={
    0xffff,//addr
};

#endif

struct VideoDataTeletextUnit {
    VideoDataBitmapPixel type;
    uint8_t colours[2];
    uint8_t data0,data1;
#if VIDEO_TRACK_METADATA
    VideoDataUnitMetadata metadata;
#endif
};

struct VideoDataBitmapUnit {
    VideoDataBitmapPixel pixels[8];
#if VIDEO_TRACK_METADATA
    VideoDataUnitMetadata metadata;
#endif
};

#include <shared/pshpack1.h>
union VideoDataUnit {
    VideoDataBitmapPixel type;
    VideoDataBitmapUnit bitmap;
    VideoDataTeletextUnit teletext;
    uint64_t values[2];
};
#include <shared/poppack.h>

#if VIDEO_TRACK_METADATA
CHECK_SIZEOF(VideoDataUnit,18);
#else
CHECK_SIZEOF(VideoDataUnit,16);
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#endif
