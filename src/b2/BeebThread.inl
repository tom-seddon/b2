//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// For event type XXX, "payload" refers to the XXXMessagePayload struct
// defined in the .cpp.

#define ENAME BeebThreadMessageType
EBEGIN()

EPN(None)
EPN(Stop)
EPN(KeyState)
EPN(KeySymState)
EPN(CloneWindow)
//EPN(CloneThisThread)
EPN(HardReset)
EPN(SetSpeedLimiting)
EPN(LoadDisc)
//EPN(SetDiscImageNameAndLoadMethod)
EPN(SetPaused)
//EPN(GoToTimelineNode)
EPN(LoadState)
EPN(SaveState)
EPN(StartReplay)
//EPN(SaveAndReplayFrom)
//EPN(SaveAndVideoFrom)
//EPN(LoadLastState)
EPN(CancelReplay)
EPN(StartPaste)
EPN(StopPaste)
EPN(StartCopy)
EPN(StopCopy)
EPN(Timing)
EPN(Custom)

#if BBCMICRO_TRACE
EPN(StartTrace)
EPN(StopTrace)
#endif

#if BBCMICRO_TURBO_DISC
EPN(SetTurboDisc)
#endif

#if BBCMICRO_DEBUGGER
EPN(SetByte)
EPN(DebugWakeUp)
EPN(SetBytes)
EPN(SetExtByte)
EPN(DebugAsyncCall)
#endif

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

#define ENAME BeebThreadTimelineState
EBEGIN()
EPN(None)
EPN(Record)
EPN(Replay)
EEND()
#undef ENAME
