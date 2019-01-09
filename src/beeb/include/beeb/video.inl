#define ENAME VideoDataType
EBEGIN()
EPNV(Bitmap16MHz,0)
EPNV(Teletext,1)
EPNV(Bitmap12MHz,2)//Video NuLA only - should really use it for Teletext too, though...
EPNV(HSync,3)
EPNV(VSync,4)
EEND()
#undef ENAME

// Ensure a memset(x,0,sizeof *x) (or similar) produces blank pixels.
static_assert(VideoDataType_Bitmap16MHz==0,"");
