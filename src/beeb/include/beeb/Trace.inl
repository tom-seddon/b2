#define ENAME TraceEventSource
EBEGIN()
// No specific source recorded.
EPNV(None, 0)

// Event comes from the host.
EPNV(Host, 1)

// Event comes from the parasite.
EPNV(Parasite, 2)

//////////////////////////////////////////////////////////////////////////
EPN(Count)
EEND()
#undef ENAME

// The trace mechanism stores this value in a 2-bit field.
static_assert(TraceEventSource_Count < 4);
