#define ENAME SDLEventType
EBEGIN()

// code - display ID
EPN(VBlank)

// N/A
EPN(UpdateWindowTitle)

// (BeebWindowInitArguments *)data1 - delete
EPN(NewWindow)

// (std::function<void()> *)data1 - delete
EPN(Function)

EPN(Count)

EEND()
#undef ENAME