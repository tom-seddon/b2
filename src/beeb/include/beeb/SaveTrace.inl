#define ENAME TraceOutputFlags
EBEGIN()
// If set, include register names in output (takes up more columns...)
EPNV(RegisterNames, 1)

// If set, include cycles in output
EPNV(Cycles, 2)

// If Cycles flag also set, include absolute cycle counts rather than relative
EPNV(AbsoluteCycles, 4)
EEND()
#undef ENAME