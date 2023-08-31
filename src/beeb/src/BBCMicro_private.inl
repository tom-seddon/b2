// The IRQ/NMI flags for host and parasite could be separate, but they aren't. No danger of running out just yet.

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#define ENAME BBCMicroIRQDevice
EBEGIN()
EPNV(SystemVIA, 1 << 0)
EPNV(UserVIA, 1 << 1)
EPNV(HostTube, 1 << 2)
EPNV(ParasiteTube, 1 << 3)
EEND()
#undef ENAME

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#define ENAME BBCMicroNMIDevice
EBEGIN()
EPNV(1770, 1 << 0)
EPNV(ParasiteTube, 1 << 1)
EEND()
#undef ENAME

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#define ENAME BBCMicroUpdateFlag
EBEGIN()
EPNV(HasRTC, 1 << 0)
EPNV(HasBeebLink, 1 << 1)
EPNV(Hacks, 1 << 2)
EPNV(Debug, 1 << 3)
EPNV(Trace, 1 << 4)
EPNV(Parasite, 1 << 5)

// Special mode covers non-default Tube operation modes: any or all of boot
// mode, host-initiated Tube reset, and parasite reset.
//
// Special mode is not efficient.
EPNV(ParasiteSpecial, 1 << 6)

EPNV(DebugStepParasite, 1 << 7)
EPNV(DebugStepHost, 1 << 8)

// If clear, parasite (if any) runs at 4 MHz.
//
// If set, parasite (if any) runs at an effective 3 MHz, by running for 3 cycles
// out of every 4.
EPNV(Parasite3MHzExternal, 1 << 9)

EPNV(ParallelPrinter, 1 << 10)

EEND()
#undef ENAME

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
