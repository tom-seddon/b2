#define ENAME VideoDataType
EBEGIN()
EPNV(Data,0)
EPNV(Nothing,2)
EPN(Cursor)//must be Nothing^1
EPN(HSync)
EPN(VSync)

#if BBCMICRO_FINER_TELETEXT
EPN(Teletext)
#endif

EPN(NuLAAttribute)

EEND()
#undef ENAME
