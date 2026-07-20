#define ENAME CaptureRectFlag
EBEGIN_DERIVED(uint32_t)

// if set, move mouse to (0,0) before taking the screen grab.
EPN_BIT_FLAG(MoveMouseToOrigin, 0)

// if set, don't inflate the rect slightly before taking the capture.
EPN_BIT_FLAG(DontInflateRect, 1)

EEND()
#undef ENAME
