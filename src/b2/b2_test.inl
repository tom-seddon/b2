#define ENAME CaptureRectFlag
EBEGIN_DERIVED(uint32_t)

// if set, move mouse to (0,0) before taking the screen grab.
EPNV(MoveMouseToOrigin, 1 << 0)

// if set, don't inflate the rect slightly before taking the capture.
EPNV(DontInflateRect, 1 << 1)

EEND()
#undef ENAME
