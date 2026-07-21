//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#define ENAME BBCMicroTypeID
EBEGIN_DERIVED(uint8_t)
EPN(B)
EPN(BPlus)
EPN(Master) // this should be Master128, but it's saved into b2.json... oops
EPN(MasterCompact)
EPN(Electron)
EEND_SERIALIZABLE("067da8722ae6b8184490c98899a0196b52853e2c")
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
EBEGIN_DERIVED(uint32_t)
// Set if display comes from shadow RAM rather than main RAM.
EPN_BIT_FLAG(DisplayShadow, 0)
EEND()
#undef ENAME

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#define ENAME HostIOFlag
EBEGIN_DERIVED(uint8_t)
// These 3 bit assignments are not arbitrary - they match the bit ordering in
// Master 128 ACCCON.
EPN_BIT_FLAG(ITU, 0) //set for internal Tube
EPN_BIT_FLAG(IFJ, 1) //set for internal FRED/JIM
EPN_BIT_FLAG(TST, 2) //set for ROM visible at $fc00...$feff

// The inverted logic means the value can be used as an index when this value
// isn't set.
EPN_BIT_FLAG(NoIO, 3)
EEND()
#undef ENAME

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#define ENAME BBCMicroTypeFlag
EBEGIN_DERIVED(uint32_t)
// Only applies to B. If set, all ROM banks are selectable; if clear, only banks
// 12-15 are available (corresponding to unexpanded B).
EPN_BIT_FLAG(ROMBoard, 0)
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
EPN_BIT_FIELD(ROM, 0, 4)
//EQPNV(ROM, 15)
EPN_BIT_FLAG(OverrideROM, 4)

// If OverrideANDY, ANDY selects ANDY (1) or current sideways ROM (0) at $8000
// (exact size model-dependent).
EPN_BIT_FLAG(ANDY, 5)
EPN_BIT_FLAG(OverrideANDY, 6)

// If OverrideHAZEL, HAZEL selects HAZEL (1) or OS (0) at $c000...$dfff.
EPN_BIT_FLAG(HAZEL, 7)
EPN_BIT_FLAG(OverrideHAZEL, 8)

// If OverrideShadow, Shadow selects shadow RAM (1) or main RAM (0) as
// $3000...$7fff.
EPN_BIT_FLAG(Shadow, 9)
EPN_BIT_FLAG(OverrideShadow, 10)

// If OverrideOS, OS selects behaviour of $fc00...$feff: read MOS ROM/write I/O
// (1) or read/write I/O (0).
EPN_BIT_FLAG(OS, 11)
EPN_BIT_FLAG(OverrideOS, 12)

EPN_BIT_FLAG(ParasiteROM, 13)
EPN_BIT_FLAG(OverrideParasiteROM, 14)

// This flag is special: it doesn't have a separate Override flag, and is itself
// the override flag, since it's only a property of the debugger's view of the
// system. It's always assumed to be clear (so the debugger views the host), but
// can be set to view the parasite instead.
EPN_BIT_FLAG(Parasite, 15)

EPN_BIT_FLAG(OverrideMapperRegion, 16)
EPN_BIT_FIELD(MapperRegion, 17, NUM_MAPPER_REGIONS_LOG2)
//EPNV(MapperRegionShift, 17)
//EQPNV(MapperRegionMask, NUM_MAPPER_REGIONS - 1)

// If OverrideIFJ, IFJ selects behaviour of $fc00...$fdff: IFJ (1) or XFJ (0).
EPN_BIT_FLAG(OverrideIFJ, 21)
EPN_BIT_FLAG(IFJ, 22)

// If OverrideITU, ITU selects behaviour of Tube: ITU (1) or XTU (0).
EPN_BIT_FLAG(OverrideITU, 23)
EPN_BIT_FLAG(ITU, 24)

//next free bit is 25

EEND()
#undef ENAME
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
