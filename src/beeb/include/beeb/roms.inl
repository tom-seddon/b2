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
EPN(MOS511i_ADFS)
EPN(MOS511i_BASIC4)
EPN(MOS511i_UTILS)
EPN(MOS511i_MOS)
EPN(MOS511i_ARABIC)
EPN(MOS511i_INTERNATIONAL)
EPN(Electron_MOS)
//Plus1 maps to the AP6 ROM - I didn't realise there'd be a good reason to have multiple types of Plus 1 ROM, so I gave this one an overly generic name
EPN(Plus1)
EPN(Plus3ADFS)
EPN(AcornPlus1) //this is the original Acorn Plus 1 ROM
EEND_SERIALIZABLE("7fde8cfcefb5c9a90ed445ec5fea2cf207e9e144")
#undef ENAME

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// Named after the corresponding enums in the MAME source: https://github.com/mamedev/mame/blob/master/src/devices/bus/bbc/rom/pal.cpp
//
// TODO: should really be SidewaysROMType or ROMMapperType or something.
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
EPN(Trilogy)
EPN(MO2)

//must be last
EPN(Count)
EEND_SERIALIZABLE("c79697cc36c8a2d4f69efe484391551626b6922e")
#undef ENAME

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#define ENAME OSROMType
EBEGIN_DERIVED(uint8_t)
EPN(16KB)
EPN(Compact)
EPN(MegaROM)

// Oops. It was a mistake to have the bank number included in this enum.
EPN(MultiOSBank0)
EPN(MultiOSBank1)
EPN(MultiOSBank2)
EPN(MultiOSBank3)

//must be last
EPN(Count)
EEND_SERIALIZABLE("28770d1b69487585451c44ed6cf9ee4ca3f67e26")
#undef ENAME

// MultiOSBank values must be contiguous.
static_assert(OSROMType_MultiOSBank1 == OSROMType_MultiOSBank0 + 1);
static_assert(OSROMType_MultiOSBank2 == OSROMType_MultiOSBank0 + 2);
static_assert(OSROMType_MultiOSBank3 == OSROMType_MultiOSBank0 + 3);
