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
// Extra data is squeezed into the `x' bits of each pixel. This
// is a bit tiresome, but it simplifies the Video ULA code quite a lot to have
// each pixel as a copyable value struct, rather than packed as 12 bits in a
// bitfield or whatever. (Trying to also keep the -O0 build from being too
// disastrous - though PCs are so fast these days that I don't seem to have
// to bother too much.)
//
// X bits encoding:
//
// pixels[0] - unit type
// pixels[1] - flags (combination of VideoDataUnitFlag)
//
// Teletext encoding (when pixels[0].x==VideoDataType_Teletext)
//
// pixels[0].rgb - background
// pixels[1].rgb - foreground
// pixels[2]. gb - scanline 0 data
// pixels[3]. gb - scanline 1 data

// <pre>
//   f   e   d   c   b   a   9   8   7   6   5   4   3   2   1   0
// +---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+
// |       x       |      red      |     green     |      blue     |
// +---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+
// </pre>
struct VideoDataPixelBits {
    uint16_t b:4;
    uint16_t g:4;
    uint16_t r:4;
    uint16_t x:4;
};

union VideoDataPixel {
    VideoDataPixelBits bits;
    uint16_t all;
};
CHECK_SIZEOF(VideoDataPixel,2);

union VideoDataUnitPixels {
    VideoDataPixel pixels[8];
    uint64_t values[2];
};

#if VIDEO_TRACK_METADATA
struct VideoDataUnitMetadata {
    uint8_t flags=0;//combination of VideoDataUnitMetadataFlag
    uint8_t value=0;
    uint16_t address={};
};
#endif

struct VideoDataUnit {
    VideoDataUnitPixels pixels;
#if VIDEO_TRACK_METADATA
    VideoDataUnitMetadata metadata;
#endif
};

#if VIDEO_TRACK_METADATA
CHECK_SIZEOF(VideoDataUnit,24);
#else
CHECK_SIZEOF(VideoDataUnit,16);
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#endif
