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
// Set if display comes from shadow RAM rather than main RAM.
EPNV(DisplayShadow, 1 << 0)
EEND()
#undef ENAME

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#define ENAME HostIOFlag
EBEGIN_DERIVED(uint8_t)
// These 3 bit assignments are not arbitrary - they match the bit ordering in
// Master 128 ACCCON.
EPNV(ITU, 1 << 0) //set for internal Tube
EPNV(IFJ, 1 << 1) //set for internal FRED/JIM
EPNV(TST, 1 << 2) //set for ROM visible at $fc00...$feff

// The inverted logic means the value can be used as an index when this value
// isn't set.
EPNV(NoIO, 1 << 3)
EEND()
#undef ENAME

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if BBCMICRO_DEBUGGER

// Flags not supported by the current setup should be treated as no-ops.

static_assert((NUM_MAPPER_REGIONS & (NUM_MAPPER_REGIONS - 1)) == 0);

#define ENAME BBCMicroDebugStateOverride
EBEGIN_DERIVED(uint32_t)
// If OverrideROM, select specific ROM bank as per ROM bits.
EQPNV(ROM, 15)
EPNV(OverrideROM, 1 << 4)

// If OverrideANDY, ANDY selects ANDY (1) or current sideways ROM (0) at $8000
// (exact size model-dependent).
EPNV(ANDY, 1 << 5)
EPNV(OverrideANDY, 1 << 6)

// If OverrideHAZEL, HAZEL selects HAZEL (1) or OS (0) at $c000...$dfff.
EPNV(HAZEL, 1 << 7)
EPNV(OverrideHAZEL, 1 << 8)

// If OverrideShadow, Shadow selects shadow RAM (1) or main RAM (0) as
// $3000...$7fff.
EPNV(Shadow, 1 << 9)
EPNV(OverrideShadow, 1 << 10)

// If OverrideOS, OS selects behaviour of $fc00...$feff: read MOS ROM/write I/O
// (1) or read/write I/O (0).
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
EQPNV(MapperRegionMask, NUM_MAPPER_REGIONS - 1)

// If OverrideIFJ, IFJ selects behaviour of $fc00...$fdff: IFJ (1) or XFJ (0).
EPNV(OverrideIFJ, 1 << 21)
EPNV(IFJ, 1 << 22)

// TODO: XTU/ITU...

//next free bit is 23

EEND()
#undef ENAME
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
