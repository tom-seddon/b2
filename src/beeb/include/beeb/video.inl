//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#define ENAME VideoDataType
EBEGIN()
// Bitmap modes, blank/cursor-only areas
EPNV(Bitmap16MHz, 0)

// Video NuLA attribute modes
EPN(Bitmap12MHz)

// Teletext - same output scaling as 12MHz modes, but a different encoding, to
// get the higher-resolution-looking characters. (This should really be done
// at the TVOutput end... maybe one day...)
EPN(Teletext)

// Same encoding as Teletext, but with no scaling. Used for matching output to
// unscaled pixel perfect BBC Micro screen grabs.
//
// (The emulator never generates this. The automated tests replace Teletext
// units with TeletextUnscaled units when appropriate.)
EPN(TeletextUnscaled)

EEND()
#undef ENAME

// Ensure a memset(x,0,sizeof *x) (or similar) produces blank pixels.
static_assert(VideoDataType_Bitmap16MHz == 0, "");

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#define ENAME VideoDataUnitFlag
EBEGIN_DERIVED(uint8_t)
EMETA_SIZE_BITS(4)

// VSync is on.
EPN_BIT_FLAG(VSync, 0)

// HSync is on.
EPN_BIT_FLAG(HSync, 1)
EEND()
#undef ENAME

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if VIDEO_TRACK_METADATA
#define ENAME VideoDataUnitMetadataFlag
EBEGIN_DERIVED(uint8_t)
EPN_BIT_FLAG(HasAddress, 0)
EPN_BIT_FLAG(OddCycle, 1) //needs renaming...
EPN_BIT_FLAG(HasValue, 2)
EPN_BIT_FLAG(6845Raster0, 3)
EPN_BIT_FLAG(6845DISPEN, 4)
EPN_BIT_FLAG(6845CUDISP, 5)

// If the HasAddress flag isn't set, the HasCRTCAddress flag is ignored.
EPN_BIT_FLAG(HasCRTCAddress, 6)
EEND()
#undef ENAME
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
