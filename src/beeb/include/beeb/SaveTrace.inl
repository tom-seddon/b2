#define ENAME TraceOutputFlags
EBEGIN_DERIVED(uint32_t)
// If set, include register names in output (takes up more columns...)
EPNV(RegisterNames, 1 << 0)

// If set, include cycles in output
EPNV(Cycles, 1 << 1)

// If Cycles flag also set, include absolute cycle counts rather than relative
EPNV(AbsoluteCycles, 1 << 2)

// Extra ROM mapper verbosity
EPNV(ROMMapper, 1 << 3)

EPNV(SymbolsAnnotations, 1 << 4)

// If adding symbols annotations, only show the annotation when it's providing
// additional info.
EPNV(MinimalSymbolsAnnotations, 1 << 5)

EEND_SERIALIZABLE("5ba3ba7ebd11f6dfca8fa1a2392d366ea933b66a")
#undef ENAME
