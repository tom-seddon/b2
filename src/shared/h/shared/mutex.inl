#define ENAME MutexInterestingEvent
EBEGIN()
EPNV(Lock, 1 << 0)
EPNV(ContendedLock, 1 << 1)
EEND()
#undef ENAME
