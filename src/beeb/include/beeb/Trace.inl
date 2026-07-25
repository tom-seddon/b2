#define ENAME TraceEventSource
EBEGIN_DERIVED(uint8_t)
// The trace mechanism stores this value in a 2-bit field.
EMETA_SIZE_BITS(2)

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
