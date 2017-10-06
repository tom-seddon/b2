#define ENAME BeebControlPixel
EBEGIN()
EPNV(Nothing,0x8000)
EPN(Cursor)//must be Nothing^1
EPN(HSync)
EPN(VSync)

#if BBCMICRO_FINER_TELETEXT
EPN(Teletext)
#endif

EEND()
#undef ENAME
