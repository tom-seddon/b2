//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#define ENAME BeebWindowDisplayAlignment
EBEGIN()
EPN(Min)
EPN(Centre)
EPN(Max)
EEND()
#undef ENAME

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#define ENAME BeebWindowDiscType
EBEGIN()
EPN(ImageFile)
EPN(65LinkFolder)
EEND()
#undef ENAME

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// These values are saved by name into the config file, so their names
// need to remain consistent. Additionally, they're always defined,
// even when the corresponding feature is #if'd out, so that settings
// from one build don't get lost after running another.

// There are two types of window - popup, and panel. Popups manipulate
// or display some kind of global state, so there's only ever one of
// each type displayed. Panels on the other hand have local state, and
// so there may be multiple instance of each type of panel visible.

#define ENAME BeebWindowPopupType
EBEGIN()
EPN(Keymaps)
EPN(Options)
EPN(Messages)
EPN(Timeline)
EPN(Configs)
EPN(Trace)
EPN(NVRAM)
EPN(AudioCallback)
EPN(CommandContextStack)
EPN(CommandKeymaps)
EPN(PixelMetadata)
EPN(DearImguiTest)
EPN(6502Debugger)
EPN(MemoryDebugger)
EPN(DisassemblyDebugger)
EPN(CRTCDebugger)
EPN(VideoULADebugger)

// must be last
EQPN(MaxValue)
EEND()
#undef ENAME

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// Might relax this later... it's only an implementation detail. But
// it makes things very convenient.
static_assert(BeebWindowPopupType_MaxValue<=64,"BeebWindowPopupType values must fit in a 64-bit bitfield");

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
