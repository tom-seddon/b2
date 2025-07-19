//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static_assert(NUM_DRIVES <= 4);
static_assert(NUM_HARD_DISKS <= 4);

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#define ENAME BBCMicroLEDFlag
EBEGIN()
EPNV(CapsLock, 1 << 0)
EPNV(ShiftLock, 1 << 1)
EPNV(TapeMotor, 1 << 2)

EQPNV(FloppyDisk0Shift, 4)
EQPNV(HardDisk0Shift, 8)
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
EPNV(DiskDrive, 1 << 15)
#if ENABLE_SCSI
EPNV(SCSI, 1 << 16)
#endif
EPNV(Serial, 1 << 17)
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
EPNV(Serial, 1 << 1)
EPNV(Drive0, 1 << 16)
// ...up to DriveN, which is Drive0<<(NUM_DRIVES-1)
EPNV(HardDisk0, 1 << 20)
// ...up to HardDiskN, which is HardDisk0<<(NUM_HARD_DISKS-1)
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

// ROM types from the perspective of BBCMicro::Update. Related to ROMType, but
// not identical.
#define ENAME BBCMicroUpdateROMType
EBEGIN_DERIVED(uint8_t)
EPN(EmptySocket)
EPN(16KB)
EPN(CCIWORD)
EPN(CCIBASE)
EPN(CCISPELL)
EPN(PALQST)
EPN(PALWAP)
EPN(PALTED)
EPN(ABEP_OR_ABE)
EPN(Trilogy)
EPN(MO2)

// Must be last.
EPN(Count)
EEND()
#undef ENAME

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// There are actually private, but at some point I realised it would be useful
// to have them displayed in the debugging UI.
//
// Lower bits should ideally not include flags modified or tested by
// GetNormalizedBBCMicroUpdateFlags, because that will result in unnecessary
// instantations when building with BBCMICRO_NUM_UPDATE_GROUPS>1.
//
// The update_mfns table is not accessed often enough for its layout to be a
// pressing concern.
//
// More flags = slower compile time, so some flags cover multiple things, quite
// possibly not in a very logical way.

#define ENAME BBCMicroUpdateFlag
EBEGIN_DERIVED(uint32_t)
// Mouse connected.
EPNV(Mouse, 1 << 0)

// Some non-fast path cases:
//
// - OSRDCH Paste
// - Instruction functions
//   - copy OSWRCH
//   - trace start/stop conditions
EPNV(NonFastPath, 1 << 1)

// Parallel printer connected.
EPNV(ParallelPrinter, 1 << 2)

// Tracing active.
EPNV(Trace, 1 << 3)

// 6502 2nd processor connected.
EPNV(Parasite, 1 << 4)

// Additional non-fast path cases that are rare or transient:
//
// - special parasite operation modes:
//   - boot mode
//   - host-initiated Tube reset
//   - parasite reset
// - debug single step
EPNV(TransientNonFastPath, 1 << 5)

// Master Compact gets its own flag, as it implies multiple things:
//
// - no Tube
// - has Master Compact EEPROM
// - mouse (if present) is Compact type
EPNV(IsMasterCompact, 1 << 6)

// Machine is Master 128.
EPNV(IsMaster128, 1 << 7)

// If clear, parasite (if any) runs at 4 MHz.
//
// If set, parasite (if any) runs at an effective 3 MHz, by running for 3 cycles
// out of every 4.
EPNV(Parasite3MHzExternal, 1 << 8)

// If set, check for breakpoints wwhile running.
EPNV(Debug, 1 << 9)

EQPNV(UpdateROMTypeShift, 10)
EQPNV(UpdateROMTypeMask, 15)
// next free bit is 1<<14

// If set, serial/tape hardware is present.
EPNV(Serial, 1 << 14)

EEND()
#undef ENAME
