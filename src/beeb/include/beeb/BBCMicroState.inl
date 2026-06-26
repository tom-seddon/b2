//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// These are the samples that come with MAME.
//
// Should this enum go somewhere else??

#define ENAME DiscDriveSound
EBEGIN_DERIVED(uint8_t)
EPN(Seek2ms)
EPN(Seek6ms)
EPN(Seek12ms)
EPN(Seek20ms)
EPN(SpinEmpty)
EPN(SpinEnd)
EPN(SpinLoaded)
EPN(SpinStartEmpty)
EPN(SpinStartLoaded)
// A single 30 ms seek (with a bit of echo)
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

// Delaying before pressing the fake keypress. This seems to be necessary to ensure the fake keypress never gets lost - possibly a race condition of some kind depending on the timing of the events? So tedious to actually debug properly that I didn't bother
EPN(DelayBeforeStartKey)

// Wait for the fake keypress to prod OSRDCH into action, and send a DELETE as the first char to get rid of it.
EPN(WaitForFirstOSRDCH)

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

// Compact only - if set, has serial upgrade fitted. (B/B+/Master 128 always
// have the serial upgrade fitted.)
EPNV(Serial, 1 << 7)

// If set, has SCSI interface available via XFJ.
EPNV(SCSI, 1 << 8)

// If set, has MMFS (Memory-Mapped Filing System) interface available via XFJ.
EPNV(MMFS, 1 << 9)
EPNV(MMFSDebug, 1 << 10)

#if BBCMICRO_DEBUGGER
// If set, has extra debugging hardware.
EPNV(ExtraDebuggingHardware, 1 << 11)
#endif

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

#if BBCMICRO_DEBUGGER
#define ENAME DebugReadMMIOResult
EBEGIN_DERIVED(uint8_t)
// No debug handler was set for this address.
EPN(Unset)

// The null debug handler was set for this address.
EPN(Unmapped)

// Value read.
EPN(GotValue)
EEND()
#undef ENAME
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#define ENAME BBCMicroCPURunState
EBEGIN_DERIVED(uint8_t)
// CPU is running.
EPN(Running)

// CPU is (briefly) waiting to sync up to the 1 MHz clock.
EPN(1MHzAccess)

// Additional Electron-only state for the 1 MHz case. Things don't happen in quite the same order, so there's an additional delay needed.
//
// TODO: surely it must be possible to shuffle the update around so that this isn't necessary...
EPN(Electron1MHzAccess2)

// Electron CPU is waiting for the ULA to read from RAM. There's 2 states, depending on how the clocks are aligned. The logic is not the same as the BBC B 1 MHz access case.
EPN(ElectronRAMAccess1)
EPN(ElectronRAMAccess2)
EEND()
#undef ENAME
