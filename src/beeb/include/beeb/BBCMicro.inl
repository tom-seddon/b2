//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static_assert(NUM_DRIVES <= 4);
static_assert(NUM_HARD_DISKS <= 4);

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#define ENAME BBCMicroLEDFlag
EBEGIN_DERIVED(uint32_t)
EPN_BIT_FLAG(CapsLock, 0)
EPN_BIT_FLAG(ShiftLock, 1)
EPN_BIT_FLAG(TapeMotor, 2)

EPN_BIT_FIELD(FloppyDisks, 4, 4)
EPN_BIT_FIELD(HardDisks, 8, 4)
EEND()
#undef ENAME

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#define ENAME BBCMicroTraceFlag
EBEGIN_DERIVED(uint32_t)
EPN_BIT_FLAG(6845, 0)
EPN_BIT_FLAG(6845Scanlines, 1)
EPN_BIT_FLAG(6845ScanlinesSeparators, 2)
EPN_BIT_FLAG(RTC, 3)
EPN_BIT_FLAG(1770, 4)
EPN_BIT_FLAG(SystemVIA, 5)
EPN_BIT_FLAG(UserVIA, 6)
EPN_BIT_FLAG(VideoULA, 7)
EPN_BIT_FLAG(SN76489, 8)
EPN_BIT_FLAG(BeebLink, 9)
EPN_BIT_FLAG(SystemVIAExtra, 10)
EPN_BIT_FLAG(UserVIAExtra, 11)
EPN_BIT_FLAG(Tube, 12)
EPN_BIT_FLAG(ADC, 13)
EPN_BIT_FLAG(EEPROM, 14)
EPN_BIT_FLAG(DiskDrive, 15)
EPN_BIT_FLAG(SCSI, 16)
EPN_BIT_FLAG(Serial, 17)
EPN_BIT_FLAG(SerialExtra, 18)
EPN_BIT_FLAG(6845Rows, 19)
EPN_BIT_FLAG(6845Columns, 20)
EPN_BIT_FLAG(Plus1, 21)
EEND_SERIALIZABLE("30b25ff4c5dbfa6d67c0dc5a535271281e4bf884")
#undef ENAME

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#define ENAME BBCMicroHackFlag
EBEGIN_DERIVED(uint32_t)
EPN_BIT_FLAG(Paste, 0)
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
EBEGIN_DERIVED(uint32_t)
EPN_BIT_FLAG(BeebLink, 0)
EPN_BIT_FLAG(Serial, 1)
EPN_BIT_FLAG(MMFS, 2)
EPN_BIT_FIELD(Drives, 16, NUM_DRIVES)
EPN_BIT_FIELD(HardDisks, 20, NUM_HARD_DISKS)
EEND()
#undef ENAME

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if BBCMICRO_DEBUGGER
// 8-bit quantity.
#define ENAME BBCMicroByteDebugFlag
EBEGIN_DERIVED(uint8_t)
EPN_BIT_FLAG(BreakExecute, 0)
EPN_BIT_FLAG(TempBreakExecute, 1)
EPN_BIT_FLAG(BreakRead, 2)
EPN_BIT_FLAG(BreakWrite, 3)
EQPNV(AnyBreakReadMask, BBCMicroByteDebugFlag_BreakExecute | BBCMicroByteDebugFlag_TempBreakExecute | BBCMicroByteDebugFlag_BreakRead)
EQPNV(AnyBreakWriteMask, BBCMicroByteDebugFlag_BreakWrite)
EEND()
#undef ENAME
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#define ENAME BBCMicroUpdateResultFlag
EBEGIN_DERIVED(uint32_t)
EPN_BIT_FLAG(AudioUnit, 0)
EPN_BIT_FLAG(VideoUnit, 1)
EPN_BIT_FLAG(Host, 2)
EPN_BIT_FLAG(Parasite, 3)
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
