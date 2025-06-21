#define ENAME MutexInterestingEvent
EBEGIN_DERIVED(uint8_t)
EPNV(Lock, 1 << 0)
EPNV(ContendedLock, 1 << 1)
EEND()
#undef ENAME
