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
//
// When it would make sense to have multiple copies of a particular
// popup, that popup just has N numbered entries. This is a bit lame,
// and should really be fixed, but it's not a pressing concern just
// yet...

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
EPN(MemoryDebugger1)
EPN(MemoryDebugger2)
EPN(MemoryDebugger3)
EPN(MemoryDebugger4)
EPN(ExtMemoryDebugger1)
EPN(ExtMemoryDebugger2)
EPN(ExtMemoryDebugger3)
EPN(ExtMemoryDebugger4)
EPN(DisassemblyDebugger)
EPN(CRTCDebugger)
EPN(VideoULADebugger)
EPN(SystemVIADebugger)
EPN(UserVIADebugger)
EPN(NVRAMDebugger)

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
