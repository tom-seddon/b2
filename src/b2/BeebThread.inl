//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// For event type XXX, "payload" refers to the XXXMessagePayload struct
// defined in the .cpp.

#define ENAME BeebThreadMessageType
EBEGIN()

EPN(None)

// Stop thread.
//
// (void)
EPN(Stop)

// Key was pressed or released.
//
// (BeebKey)u32 - key in question
// (bool)data.u64 - key state
EPN(KeyState)

EPN(KeySymState)

// Clone self into new window.
//
// (BeebWindowInitArguments *)data.ptr - init arguments for the new
// window
EPN(CloneWindow)

// Clone self into existing window. Save this thread's state and load
// the resulting state into another thread. (Not sure "clone" is
// really the best name for that though...?)
//
// (payload)data.ptr
EPN(CloneThisThread)

// Hard reset with the current config.
EPN(HardReset)

// Change speed limiting.
//
// (bool)data.u64 - state
EPN(SetSpeedLimiting)

// Set disc image.
//
// (payload)data.ptr
EPN(LoadDisc)

// Set name and load method for a loaded disc image. (The disc
// contents don't change.)
//
// (payload)data.ptr
EPN(SetDiscImageNameAndLoadMethod)

// Set paused state.
//
// (bool)data.u64 - pause state
EPN(SetPaused)

// Goes to the given spot in the timeline and continues from there.
//
// data.u64 - node ID
EPN(GoToTimelineNode)

// Replaces the 
EPN(LoadState)

// Saves current state.
//
// (void)
EPN(SaveState)

#if BBCMICRO_TRACE
// Start tracing.
//
// u32 - BeebThreadStartTraceMode
EPN(StartTrace)
EPN(StopTrace)
#endif

#if BBCMICRO_TURBO_DISC
// Set turbo disc mode.
//
// u64 - state
EPN(SetTurboDisc)
#endif

// Start replaying.
//
// (payload)data.ptr
EPN(Replay)

// Save current state, then replay from the given state to the new
// one.
//
// (payload)data.ptr
EPN(SaveAndReplayFrom)

// Save current state, start recording video of replay from given
// state to new state.
//
// This doesn't affect the receiver - video recording runs as a job,
// and emulation continues.
//
// (payload)data.ptr
EPN(SaveAndVideoFrom)

EPN(LoadLastState)

EPN(CancelReplay)

// Paste text.
//
// (payload)data.ptr
EPN(StartPaste)
EPN(StopPaste)

// Start copying.
//
// (payload)data.ptr
EPN(StartCopy)

// Stop copying. Enqueue callback that will be called with the text
// copied.
//
// (payload)data.ptr.
EPN(StopCopy)

EPN(Timing)

EPN(Custom)

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
//
// Debugger stuff...
//
#if BBCMICRO_DEBUGGER

// Set byte in memory.
//
// u32 - address
// data.u64 - value
EPN(SetByte)

// Wake thread up when emulator is being resumed. The thread could
// have gone to sleep.
EPN(DebugWakeUp)

// Set multiple bytes.
//
// (payload)data.ptr
EPN(SetBytes)

#endif
//
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

EEND()
#undef ENAME

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#define ENAME BeebThreadSyntheticEventType
EBEGIN()
EPN(SoundClockTimer)
EEND()
#undef ENAME

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if BBCMICRO_TRACE
#define ENAME BeebThreadStartTraceCondition
EBEGIN()
EPN(Immediate)
EPN(NextKeypress)
EEND()
#undef ENAME
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if BBCMICRO_TRACE
#define ENAME BeebThreadStopTraceCondition
EBEGIN()
EPN(ByRequest)
EPN(OSWORD0)
EEND()
#undef ENAME
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#define ENAME BeebThreadReplaceFlag
EBEGIN()

// If set, take current options/state and apply them to the new
// BBCMicro. Use this flag when the replace is due to a user action.
//
// Otherwise, take options from the new BBCMicro and overwrite the
// user settings/state with that. Use this flag when the replace is
// due to replaying events.
//
// (This only applies to options that affect reproducability - key
// state, turbo disc, etc.)
EPNV(ApplyPCState,1<<0)

// Do the hold-down-SHIFT autoboot thing.
EPNV(Autoboot,1<<1)

// If set, keep current discs. Used when changing config.
EPNV(KeepCurrentDiscs,1<<2)

EEND()
#undef ENAME

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
