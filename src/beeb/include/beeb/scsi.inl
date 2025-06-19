#define ENAME SCSIPhase
EBEGIN_DERIVED(uint8_t)
EPN(BusFree)
EPN(Selection)
EPN(Command)
EPN(Execute)
EPN(Read)
EPN(Write)
EPN(Status)
EPN(Message)
EEND()
#undef ENAME

#define ENAME SCSICommand
EBEGIN_DERIVED(uint8_t)
EPNV(TestUnitReady, 0x00)   // X3.131 p62
EPNV(RequestSense, 0x03)    // X3.131 p63
EPNV(FormatUnit, 0x04)      // X3.131 p87
EPNV(Read, 0x08)            // X3.131 p95
EPNV(Write, 0x0a)           // X3.131 p96
EPNV(TranslateV, 0x0f)      // X3.131 p86 - vendor specific
EPNV(ModeSelect6, 0x15)     // X3.131 p98
EPNV(ModeSense6, 0x1a)      // X3.131 p108
EPNV(StartOrStopUnit, 0x1b) // X3.131 p111
EPNV(WriteExtended, 0x2a)   // X3.131 p118
EPNV(WriteAndVerify, 0x2e)  // X3.131 p120
EPNV(Verify, 0x2f)          // X3.131 p121
EEND()
#undef ENAME
