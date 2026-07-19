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
EBEGIN_DERIVED(uint32_t)
EPN_BIT_FLAG(Save, 0)  //set if messages saved in list
EPN_BIT_FLAG(Stdio, 1) //set to print messages to stdout/stderr
EEND()
#undef ENAME
