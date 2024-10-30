#ifndef HEADER_B9662BEB223148FC8148B4BF707D4B3D // -*- mode:c++ -*-
#define HEADER_B9662BEB223148FC8148B4BF707D4B3D

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// Global constants/defines/etc.

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if BUILD_TYPE_Debug

#define BBCMICRO_TRACE 1

#elif BUILD_TYPE_RelWithDebInfo

#define BBCMICRO_TRACE 1

#elif BUILD_TYPE_Final

#define BBCMICRO_TRACE 0

#else
#error unexpected build type
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// Number of disc drives.
#define NUM_DRIVES (2)

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#define TRACK_VIDEO_LATENCY 0

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if BUILD_TYPE_Final

#else

#define VIDEO_TRACK_METADATA 1

#define BBCMICRO_DEBUGGER 1

#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if BUILD_TYPE_Final

// The 1770 logging stuff is always stripped out in this mode.

#else

// if true, dump sector contents on completion of the corresponding
// type of command
#define WD1770_DUMP_WRITTEN_SECTOR 0
#define WD1770_DUMP_READ_SECTOR 0

// if true, log each byte read to/written from the data register
// during the corresponding type of command
#define WD1770_VERBOSE_READ_SECTOR 0
#define WD1770_VERBOSE_WRITE_SECTOR 0
#define WD1770_VERBOSE_WRITES 0

// if true, log every state change
#define WD1770_VERBOSE_STATE_CHANGES 0

#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// Ensure a couple of the debugging flags are always defined.
#ifndef BBCMICRO_DEBUGGER
#define BBCMICRO_DEBUGGER 0
#endif

#ifndef VIDEO_TRACK_METADATA
#define VIDEO_TRACK_METADATA 0
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static constexpr char ADDRESS_SUFFIX_SEPARATOR = '`';

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// It's safe to cast the TV_TEXTURE_xxx values to int/unsigned.

// 736 accommodates Boffin - though the play area part is 720, so
// presumably that's a safe area. (I suspect it didn't work on all
// TVs... on my TV, with RGB SCART input, the Boffin screen isn't
// centred. There's a gap on the right, and a missing bit on the left.
// Looks like it's 2 x 6845 columns too far to the left. With UHF
// input though it is roughly in the right place.)
//
// MOS CRTC values produce a centred image on a TV texture of width
// 720. Sadly that means there are 2 columns missing at the edge of
// the Boffin screen... so the TV texture is currently 736 wide, and
// modes are a bit off centre. Doesn't look like there are any
// settings that will work for everything, at least not without some
// hacks.
//
// (None of the modes are centred on my real M128 - though they're off
// centre differently from in the emulator, so obviously something's
// still wrong.)

static const int TV_TEXTURE_WIDTH = 736;

static const int TV_TEXTURE_HEIGHT = 288 * 2;

static_assert(TV_TEXTURE_WIDTH % 8 == 0, "");

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// Slightly pointless struct that distinguishes uint64_t values that count
// cycles from other types.
//
// There are CYCLES_PER_SECOND cycles per second: some integer power of 2
// multiple of 2 MHz.
//
// In the interests of better debug build performance and reduced debugging
// hassle, there are quite deliberately no overloaded operators (etc.) for this
// type. It's an 8-byte struct with a uint64_t in it, that's got a short name so
// it's easy to type.
struct CycleCount {
    uint64_t n;
};

static_assert(sizeof(CycleCount) == 8, "CycleCount is the wrong size");

// Generates constanst for shifting between units: one to do a left shift from
// the smaller unit to the larger, and one to do a right shift in the other
// direction.
#define SHIFT_CONSTANTS(LARGER, SMALLER, N) LSHIFT_##SMALLER##_TO_##LARGER UNUSED = (N), RSHIFT_##LARGER##_TO_##SMALLER UNUSED = (N)

static constexpr uint64_t SHIFT_CONSTANTS(CYCLE_COUNT, 4MHZ, 0);
static constexpr uint64_t SHIFT_CONSTANTS(CYCLE_COUNT, 2MHZ, 1);
static constexpr uint64_t CYCLES_PER_SECOND = 2000000ull << LSHIFT_2MHZ_TO_CYCLE_COUNT;

// defined in BBCMicro_Update.cpp - not the best place for it, but all the
// related logic is in one file.
uint64_t Get3MHzCycleCount(CycleCount n);

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#endif
