#define ENAME TVOutputState
EBEGIN()
EPN(VerticalRetrace)
EPN(VerticalRetraceWait)
EPN(Scanout)
EPN(ScanoutNoRender)
EPN(FrontPorch)
EPN(HorizontalRetraceWithoutSync)
EPN(HorizontalRetrace)
EPN(HorizontalRetraceWait)
EPN(BackPorch)
EEND()
#undef ENAME
