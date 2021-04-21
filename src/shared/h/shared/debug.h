#ifndef HEADER_8CB0B7841DB5498CBBE2C2B077C9DB85
#define HEADER_8CB0B7841DB5498CBBE2C2B077C9DB85
#include "cpp_begin.h"

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#ifndef ASSERT_ENABLED
#define ASSERT_ENABLED 0
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if ASSERT_ENABLED

void LogAssertFailed(const char *file,int line,const char *function,const char *expr,int debugger);
void PRINTF_LIKE(1,2) LogAssertElaboration(const char *fmt,...);
void HandleAssertFailed(void);

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#define ASSERT(EXPR)                                                    \
    BEGIN_MACRO {                                                       \
        if(!(EXPR)) {                                                   \
            int debugger=IsDebuggerAttached();                          \
            LogAssertFailed(__FILE__,__LINE__,__func__,#EXPR,debugger); \
            if(debugger) {                                              \
                BREAK();                                                \
            }                                                           \
            HandleAssertFailed();                                       \
        }                                                               \
    } END_MACRO

#define ASSERTF(EXPR,...)                                               \
    BEGIN_MACRO {                                                       \
        if(!(EXPR)) {                                                   \
            int debugger=IsDebuggerAttached();                          \
            LogAssertFailed(__FILE__,__LINE__,__func__,#EXPR,debugger); \
            LogAssertElaboration(__VA_ARGS__);                          \
            if(debugger) {                                              \
                BREAK();                                                \
            }                                                           \
            HandleAssertFailed();                                       \
        }                                                               \
    } END_MACRO

#else

#define ASSERT(EXPR) ((void)0)
#define ASSERTF(...) ((void)0)

#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#include "cpp_end.h"
#endif//HEADER_8CB0B7841DB5498CBBE2C2B077C9DB85
