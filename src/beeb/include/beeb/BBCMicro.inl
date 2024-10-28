//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#define ENAME BBCMicroLEDFlag
EBEGIN()
EPNV(CapsLock, 1 << 0)
EPNV(ShiftLock, 1 << 1)

// For i<=0<NUM_DRIVES, drive i's bit is Drive0<<i.
EPNV(Drive0, 1 << 16)
EEND()
#undef ENAME

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#define ENAME BBCMicroTraceFlag
EBEGIN_DERIVED(uint32_t)
EPNV(6845, 1 << 0)
EPNV(6845Scanlines, 1 << 1)
EPNV(6845ScanlinesSeparators, 1 << 2)
EPNV(RTC, 1 << 3)
EPNV(1770, 1 << 4)
EPNV(SystemVIA, 1 << 5)
EPNV(UserVIA, 1 << 6)
EPNV(VideoULA, 1 << 7)
EPNV(SN76489, 1 << 8)
EPNV(BeebLink, 1 << 9)
EPNV(SystemVIAExtra, 1 << 10)
EPNV(UserVIAExtra, 1 << 11)
EPNV(Tube, 1 << 12)
EPNV(ADC, 1 << 13)
EPNV(EEPROM, 1 << 14)
EEND()
#undef ENAME

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#define ENAME BBCMicroHackFlag
EBEGIN()
EPNV(Paste, 1 << 0)
EEND()
#undef ENAME

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if BBCMICRO_DEBUGGER
//#define ENAME BBCMicroDebugByteFlag
//EBEGIN()
//EPNV(BreakExecute,1<<0)
//EPNV(BreakRead,1<<1)
//EPNV(BreakWrite,1<<2)
//EEND()
#undef ENAME

#define ENAME BBCMicroStepType
EBEGIN()
EPN(None)
EPN(StepIn)
EPN(StepIntoIRQHandler)
EPN(Count)
EEND()
#undef ENAME

#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#define ENAME BBCMicroVIAID
EBEGIN()
EPN(SystemVIA)
EPN(UserVIA)
EEND()
#undef ENAME

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#define ENAME BBCMicroCloneImpediment
EBEGIN()
EPNV(BeebLink, 1 << 0)
EPNV(Drive0, 1 << 24)
// ...up to DriveN, which is Drive0<<(NUM_DRIVES-1)
EEND()
#undef ENAME

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if BBCMICRO_DEBUGGER
// 8-bit quantity.
#define ENAME BBCMicroByteDebugFlag
EBEGIN_DERIVED(uint8_t)
EPNV(BreakExecute, 1 << 0)
EPNV(TempBreakExecute, 1 << 1)
EPNV(BreakRead, 1 << 2)
EPNV(BreakWrite, 1 << 3)
EQPNV(AnyBreakReadMask, BBCMicroByteDebugFlag_BreakExecute | BBCMicroByteDebugFlag_TempBreakExecute | BBCMicroByteDebugFlag_BreakRead)
EQPNV(AnyBreakWriteMask, BBCMicroByteDebugFlag_BreakWrite)
EEND()
#undef ENAME
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#define ENAME BBCMicroUpdateResultFlag
EBEGIN()
EPNV(AudioUnit, 1 << 0)
EPNV(VideoUnit, 1 << 1)
EPNV(Host, 1 << 2)
EPNV(Parasite, 1 << 3)
EEND()
#undef ENAME

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// There are actually private, but at some point I realised it would be useful
// to have them displayed in the debugging UI.
//
// Lower bits should ideally not include flags cleared by
// GetNormalizedBBCMicroUpdateFlags, because that will result in unnecessary
// instantations when building with BBCMICRO_NUM_UPDATE_GROUPS>1.
//
// The update_mfns table is not accessed often enough for its layout to be a
// pressing concern.

#define ENAME BBCMicroUpdateFlag
EBEGIN_DERIVED(uint32_t)
EPNV(Mouse, 1 << 0)
EPNV(Hacks, 1 << 1)
EPNV(Debug, 1 << 2)
EPNV(Trace, 1 << 3)
EPNV(Parasite, 1 << 4)

// Special mode covers non-default Tube operation modes: any or all of boot
// mode, host-initiated Tube reset, and parasite reset.
//
// Special mode is not efficient.
EPNV(ParasiteSpecial, 1 << 5)

EPNV(DebugStepParasite, 1 << 6)
EPNV(DebugStepHost, 1 << 7)

// If clear, parasite (if any) runs at 4 MHz.
//
// If set, parasite (if any) runs at an effective 3 MHz, by running for 3 cycles
// out of every 4.
EPNV(Parasite3MHzExternal, 1 << 8)

EPNV(ParallelPrinter, 1 << 9)

// Master Compact gets its own flag, as it implies multiple things:
//
// - no Tube
// - has Master Compact EEPROM
// - mouse (if present) is Compact type
EPNV(IsMasterCompact, 1 << 10)

EQPNV(ROMTypeShift, 11)
EQPNV(ROMTypeMask, 15)
// next free bit is 1<<15

EPNV(IsMaster128, 1 << 15)

EEND()
#undef ENAME

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
