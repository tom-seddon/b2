#define ENAME MessageType
EBEGIN()
EPN(Error)
EPN(Warning)
EPN(Info)
EEND()
#undef ENAME

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#define ENAME MessageListFlags
EBEGIN()
EPNV(Save, 1 << 0)  //set if messages saved in list
EPNV(Stdio, 1 << 1) //set to print messages to stdout/stderr
EEND()
#undef ENAME
