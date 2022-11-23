//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#define ENAME BBCMicroTypeID
EBEGIN()
EPN(B)
EPN(BPlus)
EPN(Master)
EEND()
#undef ENAME

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#define ENAME DiscDriveType
EBEGIN()
//EPN(90mm)
EPN(133mm)
EEND()
#undef ENAME

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#define ENAME BBCMicroTypeFlag
EBEGIN()
// if set, system has shadow RAM, so it needs an extra 32K in its memory buffer.
EPNV(HasShadowRAM, 1 << 0)

// if set, system displays teletext data from $3c00 when MA11 is set.
EPNV(CanDisplayTeletext3c00, 1 << 1)

// if set, system has RTC.
EPNV(HasRTC, 1 << 2)

// if set, numeric keypad.
EPNV(HasNumericKeypad, 1 << 3)
EEND()
#undef ENAME

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if BBCMICRO_DEBUGGER

// This name has ended up a bit of a misnomer - it should really be
// BBCMicroDebugStateOverride, or something. It's a set of flags that indicate
// which aspects of the system may get overridden for debugging purposes.

#define ENAME BBCMicroDebugPagingOverride
EBEGIN()
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

EEND()
#undef ENAME
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
