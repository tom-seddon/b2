#define ENAME ROMType
EBEGIN()
EPN(None)
EPN(Type1)
EPN(Type2)
EEND()
#undef ENAME

#define ENAME StandardROM
EBEGIN()
EPN(None)
EPN(OS12)
EPN(BPlusMOS)
EPN(BASIC2)
EPN(Acorn1770DFS)
EPN(WatfordDDFS_DDB2)
EPN(WatfordDDFS_DDB3)
EPN(OpusDDOS)
EPN(OpusChallenger)
EPN(MOS320_ADFS)
EPN(MOS320_BASIC4)
EPN(MOS320_DFS)
EPN(MOS320_EDIT)
EPN(MOS320_MOS)
EPN(MOS320_TERMINAL)
EPN(MOS320_VIEW)
EPN(MOS320_VIEWSHEET)
EPN(MOS350_ADFS)
EPN(MOS350_BASIC4)
EPN(MOS350_DFS)
EPN(MOS350_EDIT)
EPN(MOS350_MOS)
EPN(MOS350_TERMINAL)
EPN(MOS350_VIEW)
EPN(MOS350_VIEWSHEET)
EPN(MasterTurboParasite)
EPN(TUBE110)
EPN(MOS500_ADFS)
EPN(MOS500_BASIC4)
EPN(MOS500_UTILS)
EPN(MOS500_MOS)
EPN(MOS510_ADFS)
EPN(MOS510_BASIC4)
EPN(MOS510_UTILS)
EPN(MOS510_MOS)
EPN(MOSI510C_ADFS)
EPN(MOSI510C_BASIC4)
EPN(MOSI510C_UTILS)
EPN(MOSI510C_MOS)
EEND()
#undef ENAME

#define ENAME TraceOutputFlags
EBEGIN_DERIVED(uint32_t)
// If set, include register names in output (takes up more columns...)
EPNV(RegisterNames, 1)

// If set, include cycles in output
EPNV(Cycles, 2)

// If Cycles flag also set, include absolute cycle counts rather than relative
EPNV(AbsoluteCycles, 4)

// Extra ROM mapper verbosity
EPNV(ROMMapper, 8)

EEND()
#undef ENAME

#define ENAME PCKeyModifier
EBEGIN()
EPNV(Shift, 1 << 24)
EPNV(Ctrl, 1 << 25)
EPNV(Alt, 1 << 26)
EPNV(Gui, 1 << 27)
EPNV(AltGr, 1 << 28)

// not sure if I'm going to bother to support this, since it's
// effectively got 3 states (on/off/don't care)
EPNV(NumLock, 1 << 29)

EQPNV(Begin, 1 << 24)
EQPNV(End, 1 << 30)
// Don't use 1<<30 - it's SDLK_SCANCODE_MASK
EEND()
#undef ENAME
