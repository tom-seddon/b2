#define ENAME DiscInterfaceFlag
EBEGIN_DERIVED(uint32_t)
EPN_BIT_FLAG(NoINTRQ, 0)
EPN_BIT_FLAG(1772, 1)

// Quick bodge to indicate that the Challenger is known to use page &FC, making
// it incompatible with the ExtRam. (Some better mechanism for all of this is
// plausible... one day...)
EPN_BIT_FLAG(Uses1MHzBus, 2)

// This is not currently the default, but it may yet turn out that it's true for
// every interface...
EPN_BIT_FLAG(ControlIsReadOnly, 3)
EEND()
#undef ENAME
