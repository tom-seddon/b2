#define ENAME TraceOutputFlags
EBEGIN_DERIVED(uint32_t)
// If set, include register names in output (takes up more columns...)
EPN_BIT_FLAG(RegisterNames, 0)

// If set, include cycles in output
EPN_BIT_FLAG(Cycles, 1)

// If Cycles flag also set, include absolute cycle counts rather than relative
EPN_BIT_FLAG(AbsoluteCycles, 2)

// Extra ROM mapper verbosity
EPN_BIT_FLAG(ROMMapper, 3)

EPN_BIT_FLAG(SymbolsAnnotations, 4)

// If adding symbols annotations, only show the annotation when it's providing
// additional info.
EPN_BIT_FLAG(MinimalSymbolsAnnotations, 5)

EEND_SERIALIZABLE("f6ad2dfb4b896587fd0ce49ff04324d74090e35f")
#undef ENAME
