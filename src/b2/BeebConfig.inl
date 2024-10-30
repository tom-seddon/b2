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
EPNV(MasterTurbo, 1 << 0)
EPNV(6502SecondProcessor, 1 << 1)
EPNV(MasterCompact, 1 << 2)
EPNV(OlivettiPC128S, 1 << 3)
EEND()
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
EPN(PC128S)
EEND()
#undef ENAME

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
