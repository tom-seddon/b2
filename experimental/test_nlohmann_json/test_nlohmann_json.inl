#define ENAME ROMType
EBEGIN()
EPN(None)
EPN(Type1)
EPN(Type2)
EEND_SERIALIZABLE("e9ca08ab8513d455dda83045089f31f0d6bc8420")
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
EEND_SERIALIZABLE("333a02fdac64506d524ac8dbda56e4f5fbb00f21")
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

EEND_SERIALIZABLE("37559ebfc25c71b62ff0787fd93aaf4dcdc3a200")
#undef ENAME

#define ENAME PCKeyModifier
EBEGIN_DERIVED(uint32_t)
EPN_BIT_FLAG(Shift, 24)
EPN_BIT_FLAG(Ctrl, 25)
EPN_BIT_FLAG(Alt, 26)
EPN_BIT_FLAG(Gui, 27)
EPN_BIT_FLAG(AltGr, 28)

// not sure if I'm going to bother to support this, since it's
// effectively got 3 states (on/off/don't care)
EPN_BIT_FLAG(NumLock, 29)

// these are actual masks
EQPNV(Begin, 1 << 24)
EQPNV(End, 1 << 30)
// Don't use 1<<30 - it's SDLK_SCANCODE_MASK
EEND()
#undef ENAME

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#define STRUCT_NAME StructTest1
STRUCT_BEGIN()
STRUCT_FIELD(name, std::string)
STRUCT_END()
#undef STRUCT_NAME
