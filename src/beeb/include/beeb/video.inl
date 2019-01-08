#define ENAME VideoDataType
EBEGIN()
EPNV(Data,0)
EPNV(Nothing,2)
EPN(Cursor)
EPN(HSync)
EPN(VSync)
EPN(Teletext)
EPN(NuLAAttribute)
EEND()
#undef ENAME

static_assert(VideoDataType_Cursor==(VideoDataType_Nothing^1),"Cursor and Nothing values must be related");
