//#define ENAME Action
//EBEGIN()
//// Do nothing
//EPN(None)
//
//// Call instruction's IFN. (The BCDCMOS version is specifically for
//// CMOS ADC/SBC - these take an extra cycle in BCD mode.)
//EPN(Call)
//EPN(CallBCDCMOS)
//
//// Call the instruction's IFN, or not, depending on whether there was
//// a carry in the address calculations.
//EPN(MaybeCall)
//EPN(MaybeCallBCDCMOS)
//
//// Call the callback function.
//EPN(Callback)
//EEND()
//#undef ENAME

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#define ENAME Mode
EBEGIN()
EPN(Abs)
EPN(Abx)
EPN(Aby)
EPN(Acc)
EPN(Imm)
EPN(Imp)
EPN(Ind)
EPN(Indx)
EPN(Inx)
EPN(Iny)
EPN(Inz)
EPN(Rel)
EPN(Zpg)
EPN(Zpx)
EPN(Zpy)
EPN(Nop11_CMOS)
EPN(Nop22_CMOS)
EPN(Nop23_CMOS)
EPN(Nop24_CMOS)
EPN(Nop34_CMOS)
EPN(Nop38_CMOS)
EPN(Abx2_CMOS)
EPN(Abx_Broken_NMOS)
EPN(Aby_Broken_NMOS)
EPN(Iny_Broken_NMOS)
EPN(Zpg_Rel_Rockwell)
EEND()
#undef ENAME

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// TODO: fix overly terse naming and inconsistent capitalization.
#define ENAME InstrType
EBEGIN()
EPN(Unknown)
EPN(R)
EPN(W)
EPN(Branch)
EPN(IMP)
EPN(Push)
EPN(Pop)
EPN(RMW)
EEND()
#undef ENAME

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

//#define ENAME SimplifiedFun
//EBEGIN()
//EPN(None)
//EPN(Fetch)
//EPN(ZP)
//EPN(Stack)
//EEND()
//#undef ENAME

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

//#define ENAME What
//EBEGIN()
//EPN(ADL)
//EPN(ADH)
//EPN(Data)
//EPN(DataDummy)
//EPN(IAL)
//EPN(IAH)
//EPN(PCH)
//EPN(PCL)
//EPN(P)
//EEND()
//#undef ENAME

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

//#define ENAME Index
//EBEGIN()
//EPN(None)
//EPN(X)
//EPN(Y)
//EEND()
//#undef ENAME
