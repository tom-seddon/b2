#ifndef HEADER_DC000C3A3B824FD5996AAC979C8B4922 // -*- mode:c++ -*-
#define HEADER_DC000C3A3B824FD5996AAC979C8B4922

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#ifdef _MSC_VER
// Anywhere better for this?
#define _CRT_NONSTDC_NO_WARNINGS
#endif

#include <stdint.h>
#include <stddef.h>
#include <stdarg.h>
#include <stdlib.h>
#ifdef _MSC_VER
// This is the relevant part of intrin.h. Don't #include it; it is somewhat slow
// to parse (for whatever reason) and it saves a few seconds in a full rebuild
// not having it included absolutely everywhere.
extern "C" void __nop(void);
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#ifdef _WIN32

#define SYSTEM_WINDOWS 1

#elif (defined __APPLE__) && (defined __MACH__)

#define SYSTEM_OSX 1
#define SYSTEM_POSIX 1

#elif defined __linux__

#define SYSTEM_LINUX 1
#define SYSTEM_POSIX 1

#else

#error Unknown platform.

#endif

#ifndef SYSTEM_WINDOWS
#define SYSTEM_WINDOWS 0
#endif

#ifndef SYSTEM_POSIX
#define SYSTEM_POSIX 0
#endif

#ifndef SYSTEM_LINUX
#define SYSTEM_LINUX 0
#endif

#ifndef SYSTEM_OSX
#define SYSTEM_OSX 0
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// CPU define.
#if __GNUC__

#ifdef __i386
#define CPU_X86 1
#elif (defined __x86_64)
#define CPU_X64 1
#elif (defined __arm__) || (defined __aarch64__)
#define CPU_ARM 1
#elif defined __PPC && __SIZEOF_POINTER__ == 4
#define CPU_PPC32 1
#else
#error Unknown gcc CPU
#endif

#elif (defined _MSC_VER)

#ifdef _M_IX86
#define CPU_X86 1
#elif (defined _M_X64)
#define CPU_X64 1
#else
#error Unknown VC++ CPU
#endif

#else

#error Unknown compiler

#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

/* Endianness define. */

#if CPU_X86 || CPU_X64 || CPU_ARM

#define CPU_LITTLE_ENDIAN 1

#elif CPU_PPC32

#define CPU_BIG_ENDIAN 1

#else

#error Unknown endianness

#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

/* Disable VC++ warning. */
#ifdef _MSC_VER

#define VC_WARN_PUSH_DISABLE(X) __pragma(warning(push)) __pragma(warning(disable \
                                                                         : X))
#define VC_WARN_POP() __pragma(warning(pop))

#else

#define VC_WARN_PUSH_DISABLE(X)
#define VC_WARN_POP()

#endif

//#define CONSTCOND(...) VC_WARN_PUSH_DISABLE(4127) (__VA_ARGS__) VC_WARN_POP()

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#define BEGIN_MACRO do
#define END_MACRO while (0)

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

int IsDebuggerAttached(void);

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

/* How to printf thousands separators. */

#if SYSTEM_OSX

#define PRIthou "'"

#else

#define PRIthou ""

#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

/* How to printf const char *file and int line in a way that's (probably)
 * clickable in a standard editor for the platform.
 */

#ifdef _MSC_VER

#define PRIfileline "%s(%d):"

#else

#define PRIfileline "%s:%d:"

#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#ifdef _MSC_VER

// Regarding the __nop():
// https://github.com/EpicGames/UnrealEngine/blob/9e0f5f2a419a75b83392c44bf75462366ca8c1e2/Engine/Source/Runtime/Core/Public/Windows/WIndowsPlatform.h#L103
#define DEBUG_BREAK() (__nop(), __debugbreak())

#define BREAK()        \
    BEGIN_MACRO {      \
        DEBUG_BREAK(); \
    }                  \
    END_MACRO

#elif CPU_X86 || CPU_X64

#define DEBUG_BREAK()                   \
    BEGIN_MACRO {                       \
        __asm__ __volatile__("int $3\n" \
                             "nop\n");  \
    }                                   \
    END_MACRO

#define BREAK()                     \
    BEGIN_MACRO {                   \
        if (IsDebuggerAttached()) { \
            DEBUG_BREAK();          \
        } else {                    \
            abort();                \
        }                           \
    }                               \
    END_MACRO

#elif CPU_ARM

#define DEBUG_BREAK() (abort())

#define BREAK()        \
    BEGIN_MACRO {      \
        DEBUG_BREAK(); \
    }                  \
    END_MACRO

#elif CPU_PPC32

#define DEBUG_BREAK() (abort())

#define BREAK()        \
    BEGIN_MACRO {      \
        DEBUG_BREAK(); \
    }                  \
    END_MACRO

#else

#error BREAK - unknown CPU

#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

/* Function attributes. */

#ifdef _MSC_VER
#define PRINTF_LIKE(A, B)
#define NORETURN __declspec(noreturn)
#define NOINLINE __declspec(noinline)
#define UNUSED
#else
#define PRINTF_LIKE(A, B) __attribute__((format(printf, (A), (B))))
#define NORETURN __attribute__((noreturn))
#define NOINLINE __attribute__((noinline))
#define UNUSED __attribute__((unused))
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#define STRINGIZE(A) STRINGIZE_2(A)
#define STRINGIZE_2(A) #A

#define CONCAT2(A, B) CONCAT2_2(A, B)
#define CONCAT2_2(A, B) A##B

#define CONCAT3(A, B, C) CONCAT3_2(A, B, C)
#define CONCAT3_2(A, B, C) A##B##C

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#define CHECK_SIZEOF(TYPE_NAME, SIZE) static_assert(sizeof(TYPE_NAME) == (SIZE), "Size of " #TYPE_NAME " must be " #SIZE)

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if SYSTEM_WINDOWS

// GNU extensions also present on OS X.
int PRINTF_LIKE(2, 3) asprintf(char **buf, const char *fmt, ...);
int vasprintf(char **buf, const char *fmt, va_list v);

#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if SYSTEM_LINUX || SYSTEM_WINDOWS

// BSD extension.
//
// See https://lwn.net/Articles/612244/
#ifndef HAVE_STRLCPY
size_t strlcpy(char *dest, const char *src, size_t size);
#endif // HAVE_STRLCPY
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// Same API as backtrace_symbols, but does a better job of finding
// useful symbols.
char **GetBacktraceSymbols(void *const *array, int size);

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// (The gmtime_s wrapper is a #define, to avoid having to include
// time.h.)

#if SYSTEM_WINDOWS

// SUSv2
#define gmtime_r(TIMER, RESULT) (gmtime_s((RESULT), (TIMER)))
#define localtime_r(TIMER, RESULT) (localtime_s((RESULT), (TIMER)))

#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// Bitstuff.

int GetLowestSetBitIndex32(uint32_t value);
int GetHighestSetBitIndex32(uint32_t value);

int GetLowestSetBitIndex64(uint64_t value);
int GetHighestSetBitIndex64(uint64_t value);

/* What's the right return type for this? Is size_t unnecessarily
 * large? */
size_t GetNumSetBits32(uint32_t value);
size_t GetNumSetBits64(uint64_t value);

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// TODO: somewhere better for these, surely?
#define BOOL_STR(X) ((X) ? "true" : "false")

extern const char BINARY_BYTE_STRINGS[256][9]; //8 binary digits
extern const char ASCII_BYTE_STRINGS[256][5];  //"" or "'x'" if isprint
extern const char HEX_CHARS_UC[];
extern const char HEX_CHARS_LC[];

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// Retrive width of terminal - via ioctl of stdout on POSIX, or
// console API on Windows. Returns INT_MAX if it couldn't be determined.
int GetTerminalWidth(void);

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// Set name of the current thread. The size for the expansion may be
// quite limited, so put any uniqueness in a prefix rather than a
// suffix.
void PRINTF_LIKE(1, 2) SetCurrentThreadNamef(const char *fmt, ...);
void SetCurrentThreadNamev(const char *fmt, va_list v);
void SetCurrentThreadName(const char *name);

// Callback called each time the thread name is set. This is basically
// a hack so that the Remotery thread names can be reliable without
// needing to have the shared stuff depend on it.
//
// (In the longer run, the shared stuff will probably just depend on
// Remotery, job done...)
typedef void (*SetCurrentThreadNameFn)(const char *name, void *context);
void SetSetCurrentThreadNameCallback(SetCurrentThreadNameFn fn, void *context);

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// On POSIX systems, this just ignores EINTR and will always delay for
// the specified period even if signals are received.
void SleepMS(unsigned ms);

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// The tick counter is supposed to count some
// shorter-than-milliseconds period.
uint64_t GetCurrentTickCount(void);
double GetSecondsFromTicks(uint64_t ticks);
double GetSecondsPerTick();

#define GetMillisecondsFromTicks(TICKS) (GetSecondsFromTicks(TICKS) * 1000.)

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#endif //HEADER_DC000C3A3B824FD5996AAC979C8B4922
