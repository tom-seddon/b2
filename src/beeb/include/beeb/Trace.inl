#define ENAME TraceEventSource
EBEGIN()
// No specific source recorded.
EPN(None)

// Event comes from the host.
EPN(Host)

// Event comes from the parasite.
EPN(Parasite)

//////////////////////////////////////////////////////////////////////////
EPN(Count)
EEND()
#undef ENAME
