#define ENAME UpdateResult
EBEGIN()
EPN(Quit)
EPN(SpeedLimited)
EPN(FlatOut)
EEND()
#undef ENAME

#define ENAME ResetFlag
EBEGIN()
EPNV(Boot,1<<0)
EPNV(Run,1<<1)
EEND()
#undef ENAME

#define ENAME ReplaceFlag
EBEGIN()

// If set, copy new key state from current key state. Othewise, use
// state's key state.
EPNV(ResetKeyState,1<<0)

// Do the hold-down-SHIFT autoboot thing.
EPNV(Autoboot,1<<1)

// If set, keep current discs. Used when changing config.
EPNV(KeepCurrentDiscs,1<<2)

EEND()
#undef ENAME
