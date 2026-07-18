#define ENAME TraceUIStartCondition
EBEGIN()
EPN(Now)
EPN(Return)
EPN(Instruction)
EPN(WriteAddress)
EPN(Reset)
EEND_SERIALIZABLE("9447b940e2b34ac84c24f13be4b930b1e0fd38ac")
#undef ENAME

#define ENAME TraceUIStopCondition
EBEGIN()
EPN(ByRequest)
EPN(OSWORD0)
EPN(NumCycles)
EPN(WriteAddress)
EPN(BRK)
EEND_SERIALIZABLE("54548834d79e4f2b667f7b7abfc044e53b08a90f")
#undef ENAME
