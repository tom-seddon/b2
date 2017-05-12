#ifdef HEADER_143510ED93774B4392FBE08A5298EEC8
/* This is not for inclusion from other headers. */
#error shared_posix.h included twice, probably due to inclusion in a header
#else
#define HEADER_143510ED93774B4392FBE08A5298EEC8

#ifdef __cplusplus
extern "C"
{
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

/* I don't know how I feel about this, but it follows the example of
 * system_windows.h, and it's certainly convenient. */
#include <signal.h>
#include <unistd.h>
#include <execinfo.h>

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif

#endif
