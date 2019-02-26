#define ENAME TraceUIStartCondition
EBEGIN()
EPN(Now)
EPN(Return)
EPN(Instruction)
EEND()
#undef ENAME

#define ENAME TraceUIStopCondition
EBEGIN()
EPN(ByRequest)
EPN(OSWORD0)
EPN(NumCycles)
EEND()
#undef ENAME

#define ENAME TraceUICyclesOutput
EBEGIN()
EPN(Absolute)
EPN(Relative)
EPN(None)
EEND()
#undef ENAME
