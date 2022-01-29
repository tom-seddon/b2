#ifdef HEADER_4FEB716E3F5344B6ABD578D92FFA8BAC
/* This is not for inclusion from other headers. */
#error shared_windows.h included twice, probably due to inclusion in a header
#else
#define HEADER_4FEB716E3F5344B6ABD578D92FFA8BAC
#include "cpp_begin.h"

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#ifdef _WIN32_WINNT
// 0x600 = Vista+; 0x601 = Win7+; 0x602 = Win8+;
#if _WIN32_WINNT < 0x601
#error _WIN32_WINNT should probably be 0x601 or better
#endif
#else
#define _WIN32_WINNT 0x601
#endif

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <windows.h>

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

/* Result may point into static thread-local buffer overwritten on
 * each call.
 */
const char *GetLastErrorDescription(void);

/* Result may point into static thread-local buffer overwritten on
 * each call.
 */
const char *GetErrorDescription(DWORD error);

/* Windows version of backtrace. */
int backtrace(void **array, int size);

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#include "cpp_end.h"
#endif
