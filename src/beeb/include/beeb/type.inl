//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#define ENAME BBCMicroTypeID
EBEGIN()
EPN(B)
EPN(BPlus)
EPN(Master) // this should be Master128, but it's saved into b2.json... oops
EPN(MasterCompact)
EEND()
#undef ENAME

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#define ENAME DiscDriveType
EBEGIN()
EPN(90mm)
EPN(133mm)
EEND()
#undef ENAME

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#define ENAME PagingFlags
EBEGIN()
// Corresponds to Master 128 ACCCON TST bit. 1 = read ROM at $fc00...$feff, 0 =
// read I/O at $fc00...$feff. (Writes to this area always go to I/O.)
EPNV(ROMIO, 1 << 0)

// Corresponds to Master 128 ACCCON IFJ bit. 1 = FRED+JIM access cartridge, 0 =
// FRED+JIM access external connectors.
EPNV(IFJ, 1 << 1)

// Set if display comes from shadow RAM rather than main RAM.
EPNV(DisplayShadow, 1 << 2)

EEND()
#undef ENAME

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if BBCMICRO_DEBUGGER

// This name has ended up a bit of a misnomer - it should really be
// BBCMicroDebugStateOverride, or something. It's a set of flags that indicate
// which aspects of the system may get overridden for debugging purposes.
//
// Flags not supported by the current setup should be treated as no-ops.

#define ENAME BBCMicroDebugStateOverride
EBEGIN_DERIVED(uint32_t)
EPNV(ROM, 15)
EPNV(OverrideROM, 1 << 4)
EPNV(ANDY, 1 << 5)
EPNV(OverrideANDY, 1 << 6)
EPNV(HAZEL, 1 << 7)
EPNV(OverrideHAZEL, 1 << 8)
EPNV(Shadow, 1 << 9)
EPNV(OverrideShadow, 1 << 10)
EPNV(OS, 1 << 11)
EPNV(OverrideOS, 1 << 12)
EPNV(ParasiteROM, 1 << 13)
EPNV(OverrideParasiteROM, 1 << 14)

// This flag is special: it doesn't have a separate Override flag, and is itself
// the override flag, since it's only a property of the debugger's view of the
// system. It's always assumed to be clear (so the debugger views the host), but
// can be set to view the parasite instead.
EPNV(Parasite, 1 << 15)

EPNV(OverrideMapperRegion, 1 << 16)
EPNV(MapperRegionShift, 17)
EPNV(MapperRegionMask, 7)
//next free bit is 20

EEND()
#undef ENAME
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
