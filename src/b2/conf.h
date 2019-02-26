#ifndef HEADER_068D76EBAC274785B70753E8DD2FB9B8// -*- mode:c++ -*-
#define HEADER_068D76EBAC274785B70753E8DD2FB9B8

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#include <beeb/conf.h>

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// Number of vblank tick count records to keep. These are stored in a
// couple of places.
static const size_t NUM_VBLANK_RECORDS=250;

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if BUILD_TYPE_Debug

#define ENABLE_IMGUI_DEMO 1
#define ENABLE_DEBUG_MENU 1
#define ENABLE_IMGUI_TEST 1

#elif BUILD_TYPE_RelWithDebInfo

#define ENABLE_IMGUI_DEMO 1
#define ENABLE_DEBUG_MENU 1
#define ENABLE_IMGUI_TEST 1

#elif BUILD_TYPE_Final

#define ENABLE_IMGUI_DEMO 0
#define ENABLE_DEBUG_MENU 0
#define ENABLE_IMGUI_TEST 0

#else
#error unexpected build type
#endif

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

static const int TV_TEXTURE_WIDTH=736;

static const int TV_TEXTURE_HEIGHT=288*2;

static_assert(TV_TEXTURE_WIDTH%8==0,"");

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// these aren't meaningfully tweakable - they're just used in more
// than one place.

#define AUDIO_FORMAT (AUDIO_F32SYS)
#define AUDIO_NUM_CHANNELS (1)

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static const float MAX_DB=0.f;
static const float MIN_DB=-72.f;

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// Number of frames to render when creating an ordinary thumbnail. The
// first may be a partial one, and the second will be complete.
static const size_t NUM_THUMBNAIL_RENDER_FRAMES=2;

// Number of frames to render when creating a thumbnail starting from
// a cold boot. Takes longer due to memory clear, ROM init, etc.
static const size_t NUM_BOOTUP_THUMBNAIL_RENDER_FRAMES=11;

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if BBCMICRO_DEBUGGER
#define HTTP_SERVER 1
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#endif
