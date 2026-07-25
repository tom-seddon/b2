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

EPN_BIT_FLAG(SymbolGroups, 6)
EPN_BIT_FLAG(SymbolGroupsVerbose, 7)

EEND_SERIALIZABLE("3e0c7677345522fa61c11995ddcd8e3133e9c426")
#undef ENAME
