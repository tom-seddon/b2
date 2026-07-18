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
EPNV(SCSI, 1 << 16)
EPNV(Serial, 1 << 17)
EPNV(SerialExtra, 1 << 18)
EPNV(6845Rows, 1 << 19)
EPNV(6845Columns, 1 << 20)
EPNV(Plus1, 1 << 21)
EEND_SERIALIZABLE("8cbd909c805d038809925b10d10c51d38da63c0d")
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
EPNV(MMFS, 1 << 2)
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
EPN(ElectronKeyboard)

// Must be last.
EPN(Count)
EEND()
#undef ENAME

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

//// Master Compact gets its own flag, as it implies multiple things:
////
//// - no Tube
//// - has Master Compact EEPROM
//// - mouse (if present) is Compact type
//EPNV(IsMasterCompact, 1 << 6)
//
//// Machine is Master 128.
//EPNV(IsMaster128, 1 << 7)

#define ENAME BBCMicroUpdateSystemType
EBEGIN_DERIVED(uint8_t)
EPNV(BBCMicro, 0)
EPNV(Master128, 1)
EPNV(MasterCompact, 2)
EPNV(ElectronWithPlus1, 3)
EEND()
#undef ENAME

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// There are actually private, but at some point I realised it would be useful
// to have them displayed in the debugging UI.
//
// Lower bits should ideally not include flags conditionally modified or tested
// by GetNormalizedBBCMicroUpdateFlags, because that will result in unnecessary
// instantations when building with BBCMICRO_NUM_UPDATE_GROUPS>1.
//
// The update_mfns table is not accessed often enough for its layout to be a
// pressing concern.
//
// More flags = slower compile time, so some flags cover multiple things, quite
// possibly not in a very logical way.

#define ENAME BBCMicroUpdateFlag
EBEGIN_DERIVED(uint32_t)
// If set, check for breakpoints wwhile running.
EPNV(Debug, 1 << 0)

// Some non-fast path cases:
//
// - Cassette motor on
// - OSRDCH Paste
// - Instruction functions
//   - copy OSWRCH
//   - trace start/stop conditions
//
// These are off the fast path, but shouldn't do anything abominable.
EPNV(NonFastPath, 1 << 1)

// Parallel printer connected.
EPNV(ParallelPrinter, 1 << 2)

// Tracing active.
EPNV(Trace, 1 << 3)

// Mouse connected.
EPNV(Mouse, 1 << 4)

// Additional rarer non-fast path cases:
//
// - special parasite operation modes:
//   - boot mode
//   - host-initiated Tube reset
//   - parasite reset
// - debug single step
// - memory access error
//
// These are rare and/or transient, and don't promise to be remotely efficient.
EPNV(RareNonFastPath, 1 << 5)

EQPNV(UpdateSystemTypeShift, 6)
EQPNV(UpdateSystemTypeMask, 3)

// If clear, parasite (if any) runs at 4 MHz.
//
// If set, parasite (if any) runs at an effective 3 MHz, by running for 3 cycles
// out of every 4.
EPNV(Parasite3MHzExternal, 1 << 8)

// 6502 2nd processor connected.
EPNV(Parasite, 1 << 9)

EQPNV(UpdateROMTypeShift, 10)
EQPNV(UpdateROMTypeMask, 15)
// next free bit is 1<<14

// If set, serial/tape hardware is present.
EPNV(Serial, 1 << 14)

EEND()
#undef ENAME

static_assert((uint8_t)BBCMicroUpdateROMType_Count <= (uint32_t)BBCMicroUpdateFlag_UpdateROMTypeMask);

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#define ENAME BBCMicroIOByteDebugFlagRegion
EBEGIN_DERIVED(uint8_t)
EPNV(XFJ, 0)        // 16*32 external FRED/JIM - 0xfc00-0xfdff
EPNV(IFJ, 16)       // 16*32 internal FRED/JIM - 0xfc00-0xfdff
EPNV(S_XTU, 32)     // 8*32 Sheila with XTU - 0xfe00-0xfeff
EPNV(S_ITU, 32 + 8) // 1*32 Sheila with ITU - 0xfee0-0xfeff
EPN(Count)
EEND()
#undef ENAME

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#define ENAME BBCMicroHaltReason
EBEGIN_DERIVED(uint8_t)
EPN(None)       //system is running
EPN(Write)      //write breakpoint
EPN(SingleStep) //single step
EPN(Execute)    //execute breakpoint
EPN(Read)       //read breakpoint
EPN(Interrupt)  //interrupt breakpoint
EPN(ManualHalt) //explicit full-system stop
EEND()
#undef ENAME

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
