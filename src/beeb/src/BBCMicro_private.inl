// The IRQ/NMI flags for host and parasite could be separate, but they aren't. No danger of running out just yet.

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#define ENAME BBCMicroIRQDevice
EBEGIN_DERIVED(M6502_DeviceIRQFlags)
EPN_BIT_FLAG(SystemVIA, 0)
EPN_BIT_FLAG(UserVIA, 1)
EPN_BIT_FLAG(HostTube, 2)
EPN_BIT_FLAG(ParasiteTube, 3)
EPN_BIT_FLAG(SCSI, 4)
EPN_BIT_FLAG(ACIA, 5)
EPN_BIT_FLAG(ElectronULA, 6)
EEND()
#undef ENAME

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#define ENAME BBCMicroNMIDevice
EBEGIN_DERIVED(M6502_DeviceNMIFlags)
EPN_BIT_FLAG(1770, 0)
EPN_BIT_FLAG(ParasiteTube, 1)
EEND()
#undef ENAME

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#define ENAME BBCMicroMMIOScopeFlag
EBEGIN_DERIVED(uint8_t)
EPN_BIT_FLAG(XFJ, 0)
EPN_BIT_FLAG(IFJ, 1)
EPN_BIT_FLAG(XTU, 2)
EPN_BIT_FLAG(ITU, 3)
EQPNV(All, 15)
EEND()
#undef ENAME
