//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// Feature flags indicate that a given BeebConfig depends on some new feature
// that has been added. If the config appears to have been saved by an older
// version of b2, any stock BeebConfigs that use the new feature are
// automatically added, meaning the new stuff is easily accessible.
//
// (BeebConfigs that use known features are not added. Delete a stock config,
// and it won't come back automatically.)

#define ENAME BeebConfigFeatureFlag
EBEGIN_DERIVED(uint32_t)
EPN_BIT_FLAG(MasterTurbo, 0)
EPN_BIT_FLAG(6502SecondProcessor, 1)
EPN_BIT_FLAG(MasterCompact, 2)
EPN_BIT_FLAG(OlivettiPC128S, 3)
EPN_BIT_FLAG(MasterCompactArabic, 4)
EPN_BIT_FLAG(Electron, 5)
EEND_SERIALIZABLE("4a7000195d36d5ba8c4db0e8279672848fa7a374")
#undef ENAME

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#define ENAME BeebConfigNVRAMType
EBEGIN()
// Unknown means guess based on the config's type id
EPN(Unknown)
EPN(None)
EPN(Master128)
EPN(MasterCompact)
EPN(MasterCompactInternational) //covers both PC 128 S and MOS 5.11i Arabic Compact
EEND_SERIALIZABLE("03f63e3598a4af2cb102df7321f3914f94cbf44e")
#undef ENAME

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
