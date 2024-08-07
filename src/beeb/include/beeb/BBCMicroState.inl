//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// These are the samples that come with MAME.
//
// Should this enum go somewhere else??

#define ENAME DiscDriveSound
EBEGIN()
EPN(Seek2ms)
EPN(Seek6ms)
EPN(Seek12ms)
EPN(Seek20ms)
EPN(SpinEmpty)
EPN(SpinEnd)
EPN(SpinLoaded)
EPN(SpinStartEmpty)
EPN(SpinStartLoaded)
EPN(Step)
EPN(EndValue)
EEND()
#undef ENAME

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#define ENAME BBCMicroPasteState
EBEGIN()
// No pasting. Hack flags paste bit must be reset.
EPN(None)

// Wait for the fake keypress that hopefully prods OSRDCH into action.
EPN(Wait)

// Delete the fake keypress with a DELETE.
EPN(Delete)

// Paste the string.
EPN(Paste)
EEND()
#undef ENAME

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// TODO if this were a struct with bit fields, that would simplify things...

#define ENAME BBCMicroInitFlag
EBEGIN()
// Set if the video ULA is in fact a Video NuLA.
EPNV(VideoNuLA, 1 << 0)

// Set if the ExtMem is present.
EPNV(ExtMem, 1 << 1)

// Set if the power-on brr... tone should sound when doing a power-on reset
// (this ended up here because it's convenient, not because it makes sense).
EPNV(PowerOnTone, 1 << 2)

EPNV(Mouse, 1 << 3)

// If set, ADJI inserted, available via IFJ.
EPNV(ADJI, 1 << 4)

// If ADJI bit set, there's a 2-bit value encoding the base address.
EQPNV(ADJIDIPSwitchesShift, 5)

EEND()
#undef ENAME

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#define ENAME BBCMicroMouseButton
EBEGIN_DERIVED(uint8_t)
EPNV(Left, 1 << 0)
EPNV(Middle, 1 << 1)
EPNV(Right, 1 << 2)
EEND()
#undef ENAME

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
