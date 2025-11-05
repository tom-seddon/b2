#define ENAME TraceUIStartCondition
EBEGIN()
EPN(Now)
EPN(Return)
EPN(Instruction)
EPN(WriteAddress)
EPN(Reset)
EEND_SERIALIZABLE()
#undef ENAME

#define ENAME TraceUIStopCondition
EBEGIN()
EPN(ByRequest)
EPN(OSWORD0)
EPN(NumCycles)
EPN(WriteAddress)
EPN(BRK)
EEND_SERIALIZABLE()
#undef ENAME
