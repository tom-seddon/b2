#define ENAME SDLEventType
EBEGIN()

// code - display ID
EPN(VBlank)

// N/A
EPN(UpdateWindowTitle)

// (std::function<void()> *)data1 - delete
EPN(Function)

// (BeebWindowSettings *)data1 - delete
EPN(SaveConfig)

EPN(Count)

EEND()
#undef ENAME
