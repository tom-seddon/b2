//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

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

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// Named after the corresponding enums in the MAME source: https://github.com/mamedev/mame/blob/master/src/devices/bus/bbc/rom/pal.cpp
#define ENAME ROMType
EBEGIN_DERIVED(uint8_t)
EPN(16KB)
EPN(CCIWORD)
EPN(CCIBASE)
EPN(CCISPELL)
EPN(PALQST)
EPN(PALWAP)
EPN(PALTED)
EPN(ABEP)
EPN(ABE)

//must be last
EPN(Count)
EEND()
#undef ENAME

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
