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
EPN(AudioCallback)
EPN(CommandKeymaps)
EPN(PixelMetadata)
EPN(DearImguiTest)
EPN(6502Debugger)
EPN(Parasite6502Debugger)
EPN(MemoryDebugger1)
EPN(MemoryDebugger2)
EPN(MemoryDebugger3)
EPN(MemoryDebugger4)
EPN(ExtMemoryDebugger1)
EPN(ExtMemoryDebugger2)
EPN(ExtMemoryDebugger3)
EPN(ExtMemoryDebugger4)
EPN(DisassemblyDebugger1)
EPN(DisassemblyDebugger2)
EPN(DisassemblyDebugger3)
EPN(DisassemblyDebugger4)
EPN(CRTCDebugger)
EPN(VideoULADebugger)
EPN(SystemVIADebugger)
EPN(UserVIADebugger)
EPN(NVRAMDebugger)
EPN(SavedStates)
EPN(BeebLink)
EPN(SN76489Debugger)
EPN(PagingDebugger)
EPN(BreakpointsDebugger)
EPN(StackDebugger)
EPN(ParasiteStackDebugger)
EPN(ParasiteMemoryDebugger1)
EPN(ParasiteMemoryDebugger2)
EPN(ParasiteMemoryDebugger3)
EPN(ParasiteMemoryDebugger4)
EPN(ParasiteDisassemblyDebugger1)
EPN(ParasiteDisassemblyDebugger2)
EPN(ParasiteDisassemblyDebugger3)
EPN(ParasiteDisassemblyDebugger4)
EPN(TubeDebugger)
EPN(ADCDebugger)
EPN(DigitalJoystickDebugger)
EPN(ImGuiDebug)
EPN(KeyboardDebug)
EPN(SystemDebug)
EPN(MouseDebug)
EPN(WD1770Debug)
EPN(DiskDriveDebug)
EPN(HardDiskDebug)
EPN(SCSIDebug)
EPN(SerialDebug) //55
EPN(SymbolGroupManagement)
EPN(PagingBrowserDebugger)
EPN(SymbolGroupBrowser)
EPN(MutexStats)
EPN(ElectronULADebug)
EPN(TapeDebug)
EPN(Plus1Debug)

// must be last
EQPN(MaxValue)
EEND_SERIALIZABLE("34abe885e1c2a1664dab3872c713c56a0b8c87e5")
#undef ENAME

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#define ENAME BeebWindowLEDsPopupMode
EBEGIN()
EPN(Off)
EPN(On)
EPN(Auto)
EEND_SERIALIZABLE("ea3f8a879ffaa46027129285d97e4d58f9dee4be")
#undef ENAME

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#define ENAME BeebWindowLaunchType
EBEGIN()
EPN(Unknown)

// using the command line, or double click from Explorer/Finder
EPN(UseExistingProcess)

// file drag-and-drop onto window
EPN(DragAndDrop)
EEND()
#undef ENAME

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// This is debugger specific, but it's always compiled in so it won't get lost
// when using a non-debugging version.
#define ENAME DebuggerSyntax
EBEGIN()
EPN(BBCBASIC)
EPN(Assembler)
EPN(C)
EEND()
#undef ENAME

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#ifdef IMGUI_ENABLE_TEST_ENGINE

#ifdef None
// X.h nonsense.
#undef None
#endif

#define ENAME DearImGuiTestEngineState
EBEGIN_DERIVED(uint8_t)
EPN(None)
EPN(Initialised)
EPN(Done)
EEND()
#undef ENAME
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#define ENAME CaptureBackBufferState
EBEGIN_DERIVED(uint8_t)
EPN(Idle)
EPN(Requested)
EPN(Capturing)
EPN(GotResult)
EEND()
#undef ENAME
