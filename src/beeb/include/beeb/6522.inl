//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#define ENAME R6522Cx2Control
EBEGIN_DERIVED(uint8_t)
EMETA_SIZE_BITS(3)
EPNV(Input_NegEdge, 0)
EPNV(Input_IndIRQNegEdge, 1)
EPNV(Input_PosEdge, 2)
EPNV(Input_IndIRQPosEdge, 3)
EPNV(Output_Handshake, 4)
EPNV(Output_Pulse, 5)
EPNV(Output_Low, 6)
EPNV(Output_High, 7)
EEND()
#undef ENAME

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#define ENAME R6522IRQMask
EBEGIN_DERIVED(uint8_t)
EMETA_SIZE_BITS(7)
EPNV(CA2, 1)
EPNV(CA1, 2)
EPNV(SR, 4)
EPNV(CB2, 8)
EPNV(CB1, 16)
EPNV(T2, 32)
EPNV(T1, 64)
EEND()
#undef ENAME

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#define ENAME R6522SRControl
EBEGIN_DERIVED(uint8_t)
EMETA_SIZE_BITS(3)
EPNV(Disabled, 0)
EPNV(ShiftIn_T2, 1)
EPNV(ShiftIn_Clock, 2)
EPNV(ShiftIn_ExtClock, 3)
EPNV(ShiftOut_FreeT2, 4)
EPNV(ShiftOut_T2, 5)
EPNV(ShiftOut_Clock, 6)
EPNV(ShiftOut_ExtClock, 7)
EEND()
#undef ENAME
