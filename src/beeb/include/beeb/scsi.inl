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
EPNV(TestUnitReady,0x00)
EPNV(RequestSense, 0x03)
EPNV(FormatUnit, 0x04)
EPNV(Read, 0x08)
EPNV(Write, 0x0a)
EPNV(TranslateV, 0x0f)
EPNV(ModeSelect6, 0x15)
EPNV(ModeSense6, 0x1a)
EPNV(StartOrStopUnit, 0x1b)
EPNV(WriteExtended, 0x2a)
EPNV(WriteAndVerify, 0x2e)
EPNV(Verify, 0x2f)
EEND()
#undef ENAME
