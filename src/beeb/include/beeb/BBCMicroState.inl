//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if BBCMICRO_ENABLE_DISC_DRIVE_SOUND

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

#endif

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

// Set if the state is a clone: a simple value copy of an existing state,
// possibly itself a clone.
//
// Clones are simple copies, so the following apply:
//
// - the various ROM and RAM shared_ptr'd buffers refer to the same buffers as
//   the original state. They may be out of sync relative to the rest of the
//   hardware state!
// - any hardware Handler or Trace pointers (etc.) are not valid
//
// This means that a cloned state is kind of useless! They're there purely for
// the debugger to use for updating its UI without having to have each window
// take a lock (or have the debugger copy all of BBC memory).
//
// The potential discrepancy between RAM contents and hardware state doesn't
// matter; when the BBC is running, it's impossible to tell, and when it's
// stopped, the two are actually in sync.
//
// (Any actual modification to the BBCMicro state is done via BeebThread
// messages or with a BBCMicro pointer obtained from
// BeebThread::LockMutableBeeb.)
EPNV(Clone, 1 << 3)

// If set, ADJI inserted, available via IFJ.
EPNV(ADJI, 1 << 4)

// If ADJI bit set, there's a 2-bit value encoding the base address.
EQPNV(ADJIDIPSwitchesShift, 5)

// If set, BeebLink.
EPNV(BeebLink, 1 << 7)
EEND()
#undef ENAME

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
