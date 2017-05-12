#define ENAME TraceUIStartCondition
EBEGIN()
EPN(Now)
EPN(Return)
EEND()
#undef ENAME

#define ENAME TraceUIStopCondition
EBEGIN()
EPN(ByRequest)
EPN(OSWORD0)
EEND()
#undef ENAME
