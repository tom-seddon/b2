#define ENAME DiscInterfaceFlag
EBEGIN()
EPNV(NoINTRQ, 1 << 0)
EPNV(1772, 1 << 1)

// Quick bodge to indicate that the Challenger is known to use page &FC, making
// it incompatible with the ExtRam. (Some better mechanism for all of this is
// plausible... one day...)
EPNV(Uses1MHzBus, 1 << 2)

// This is not currently the default, but it may yet turn out that it's true for
// every interface...
EPNV(ControlIsReadOnly, 1 << 3)
EEND()
#undef ENAME
