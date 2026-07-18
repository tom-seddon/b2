#define ENAME TraceUIStartCondition
EBEGIN()
EPN(Now)
EPN(Return)
EPN(Instruction)
EPN(WriteAddress)
EPN(Reset)
EEND_SERIALIZABLE("4d7bb441cecd41551e00170729f700239ea17110")
#undef ENAME

#define ENAME TraceUIStopCondition
EBEGIN()
EPN(ByRequest)
EPN(OSWORD0)
EPN(NumCycles)
EPN(WriteAddress)
EPN(BRK)
EEND_SERIALIZABLE("95b389ce31b917d8348ca0b1d36efb1e747b9397")
#undef ENAME
