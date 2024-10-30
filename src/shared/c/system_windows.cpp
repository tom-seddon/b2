#include <shared/system.h>
#include <shared/system_windows.h>
#include <shared/debug.h>
#include <stdio.h>
#include <stdlib.h>
#include <direct.h>
#include <intrin.h>
#include <vfw.h>
#include <vector>

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static __declspec(thread) char g_last_error_description[500];

static BOOL g_got_qpc_freq;
static double g_secs_per_tick;

static bool g_got_set_thread_description;
static HRESULT (*g_set_thread_description)(HANDLE, PCWSTR);

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

int IsDebuggerAttached() {
    return IsDebuggerPresent();
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

int asprintf(char **buf, const char *fmt, ...) {
    va_list v;

    va_start(v, fmt);
    int r = vasprintf(buf, fmt, v);
    va_end(v);

    return r;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

int vasprintf(char **buf, const char *fmt, va_list v_) {
    // value is slightly lower than 16K, to avoid /analyze warning.
    static const size_t SIZE = 16000;
    char tmp[SIZE]; //probably gonig to be large enough.
    va_list v;

    // Try to use the stack buffer.
    va_copy(v, v_);
    int n = vsnprintf(tmp, sizeof tmp, fmt, v);
    va_end(v);
    if (n < 0) {
        // Is this even possible?
        return -1;
    }

    if (n < (int)SIZE) {
        *buf = (char *)malloc((size_t)n + 1);
        if (!buf) {
            return -1;
        }

        memcpy(*buf, tmp, (size_t)n + 1);
        return n;
    }

    // Allocate a buffer large enough, and use that.
    *buf = (char *)malloc((size_t)n + 1);
    if (!*buf) {
        return -1;
    }

    va_copy(v, v_);
    int n2 = vsnprintf(*buf, (size_t)n + 1, fmt, v);
    va_end(v);
    if (n2 < 0) {
        // Is this even possible?
        return -1;
    }

    ASSERT(n == n2);
    return n;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// http://www.opensource.apple.com/source/mail_cmds/mail_cmds-24/mail/strlcpy.c
size_t strlcpy(char *dest, const char *src, size_t size) {
    char *d = dest;
    const char *s = src;
    size_t n = size;

    /* Copy as many bytes as will fit */
    if (n != 0 && --n != 0) {
        do {
            if ((*d++ = *s++) == 0)
                break;
        } while (--n != 0);
    }

    /* Not enough room in dst, add NUL and traverse rest of src */
    if (n == 0) {
        if (size != 0)
            *d = '\0'; /* NUL-terminate dst */
        while (*s++)
            ;
    }

    return (s - src - 1); /* count does not include NUL */
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

int backtrace(void **array, int size) {
    (void)array, (void)size;

    return 0;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

char **GetBacktraceSymbols(void *const *array, int size) {
    (void)array, (void)size;

    return NULL;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#ifndef B2_LIBRETRO_CORE
const char *GetLastErrorDescription(void) {
    return GetErrorDescription(GetLastError());
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

const char *GetErrorDescription(DWORD error) {
    DWORD n = FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM, NULL, error, 0, g_last_error_description,
                            ARRAYSIZE(g_last_error_description), NULL);

    if (n == 0) {
        /* Bleargh. */

#define CASE(X) \
    case (X):   \
        return #X

        switch (error) {
        default:
            snprintf(g_last_error_description, sizeof g_last_error_description,
                     "(Unknown error: 0x%08lX)", error);
            break;

            // These have been added in on a case by case basis, as
            // I've found stuff that FormatMessage doesn't support.
            CASE(AVIERR_UNSUPPORTED);
            CASE(AVIERR_BADFORMAT);
            CASE(AVIERR_MEMORY);
            CASE(AVIERR_INTERNAL);
            CASE(AVIERR_BADFLAGS);
            CASE(AVIERR_BADPARAM);
            CASE(AVIERR_BADSIZE);
            CASE(AVIERR_BADHANDLE);
            CASE(AVIERR_FILEREAD);
            CASE(AVIERR_FILEWRITE);
            CASE(AVIERR_FILEOPEN);
            CASE(AVIERR_COMPRESSOR);
            CASE(AVIERR_NOCOMPRESSOR);
            CASE(AVIERR_READONLY);
            CASE(AVIERR_NODATA);
            CASE(AVIERR_BUFFERTOOSMALL);
            CASE(AVIERR_CANTCOMPRESS);
            CASE(AVIERR_USERABORT);
            CASE(AVIERR_ERROR);
        }

#undef CASE

    } else {
        /* For some reason, Windows error strings end with a carriage
         * return. */

        ASSERT(g_last_error_description[n] == 0);
        while (n > 0 && isspace(g_last_error_description[n - 1])) {
            g_last_error_description[--n] = 0;
        }
    }

    return g_last_error_description;
}
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

int GetLowestSetBitIndex32(uint32_t value) {
    unsigned long index;

    if (_BitScanForward(&index, value) == 0) {
        return -1;
    } else {
        return (int)index;
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

int GetHighestSetBitIndex32(uint32_t value) {
    unsigned long index;

    if (_BitScanReverse(&index, value) == 0) {
        return -1;
    } else {
        return (int)index;
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

int GetLowestSetBitIndex64(uint64_t value) {
    unsigned long index;

#if CPU_X64
    if (_BitScanForward64(&index, value)) {
        return (int)index;
    }
#else
    if (_BitScanForward(&index, (DWORD)value)) {
        return (int)index;
    }

    if (_BitScanForward(&index, (DWORD)(value >> 32))) {
        return 32 + (int)index;
    }
#endif

    return -1;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

int GetHighestSetBitIndex64(uint64_t value) {
    unsigned long index;

#if CPU_X64
    if (_BitScanReverse64(&index, value)) {
        return (int)index;
    }
#else
    if (_BitScanReverse(&index, (DWORD)(value >> 32))) {
        return 32 + (int)index;
    }

    if (_BitScanReverse(&index, (DWORD)value)) {
        return (int)index;
    }
#endif

    return -1;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

size_t GetNumSetBits32(uint32_t value) {
    return __popcnt(value);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

size_t GetNumSetBits64(uint64_t value) {
#if CPU_X64
    return __popcnt64(value);
#else
    return __popcnt((unsigned)value) + __popcnt((unsigned)(value >> 32));
#endif
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

int GetTerminalWidth(void) {
    CONSOLE_SCREEN_BUFFER_INFO csbi = {sizeof csbi};

    if (!GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &csbi)) {
        return INT_MAX;
    }

    return csbi.dwSize.X;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// https://msdn.microsoft.com/en-gb/library/xcb2z8hs.aspx
//
// Should really use
// https://msdn.microsoft.com/en-us/library/windows/desktop/mt774976(v=vs.85).aspx...
// but that's only from Windows 10 build 1607! Seems a bit rude to
// require that when everything else only needs Vista+.

#ifndef B2_LIBRETRO_CORE
#include <shared/pshpack8.h>
struct THREADNAME_INFO {
    DWORD dwType;
    LPCSTR szName;
    DWORD dwThreadID;
    DWORD dwFlags;
};
#include <shared/poppack.h>

static void SetCurrentThreadNameInternal2(const char *name) {
    THREADNAME_INFO i = {
        0x1000,
        name,
        GetCurrentThreadId(),
    };

    __try {
        RaiseException(0x406d1388, 0, sizeof i / sizeof(ULONG_PTR), (ULONG_PTR *)&i);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
    }
}

void SetCurrentThreadNameInternal(const char *name) {
    // error C2712: Cannot use __try in functions that require object unwinding
    SetCurrentThreadNameInternal2(name);

    // It's possible for multiple threads to end up doing this at once, but
    // that's not a big problem.
    //
    // The refcount for kernel32.dll will end up screwy, I guess... I don't
    // think that's a problem either.
    if (!g_got_set_thread_description) {
        HMODULE kernel32_h = LoadLibrary("kernel32.dll");
        g_set_thread_description =
            (decltype(g_set_thread_description))GetProcAddress(
                kernel32_h, "SetThreadDescription");
        g_got_set_thread_description = true;
    }

    if (g_set_thread_description) {
        // What's the right code page here?
        int n = MultiByteToWideChar(CP_UTF8, 0, name, -1, nullptr, 0);
        if (n > 0) {
            std::vector<wchar_t> buffer((size_t)n);
            MultiByteToWideChar(
                CP_UTF8, 0, name, -1, buffer.data(), (int)buffer.size());
            (*g_set_thread_description)(GetCurrentThread(), buffer.data());
        }
    }
}
#else
void SetCurrentThreadNameInternal(const char *name) {}
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void SleepMS(unsigned ms) {
    Sleep(ms);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

double GetSecondsPerTick(void) {
    if (!g_got_qpc_freq) {
        LARGE_INTEGER hz;
        QueryPerformanceFrequency(&hz);

        g_secs_per_tick = 1. / hz.QuadPart;

        g_got_qpc_freq = TRUE;
    }

    return g_secs_per_tick;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

uint64_t GetCurrentTickCount(void) {
    LARGE_INTEGER now;
    QueryPerformanceCounter(&now);

    return (uint64_t)now.QuadPart;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

double GetSecondsFromTicks(uint64_t ticks) {
    return ticks * GetSecondsPerTick();
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static std::string GetUTF8String(const wchar_t *str, size_t len) {
    if (len > INT_MAX) {
        return "";
    }

    int n = WideCharToMultiByte(CP_UTF8, 0, str, (int)len, nullptr, 0, nullptr, nullptr);
    if (n == 0) {
        return "";
    }

    std::vector<char> buffer;
    buffer.resize(n);
    WideCharToMultiByte(CP_UTF8, 0, str, (int)len, buffer.data(), (int)buffer.size(), nullptr, nullptr);

    return std::string(buffer.begin(), buffer.end());
}

std::string GetUTF8String(const wchar_t *str) {
    size_t len = wcslen(str);
    return GetUTF8String(str, len);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

std::string GetUTF8String(const std::wstring &str) {
    return GetUTF8String(str.data(), str.size());
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static std::wstring GetWideString(const char *str, size_t len) {
    if (len > INT_MAX) {
        return L"";
    }

    int n = MultiByteToWideChar(CP_UTF8, 0, str, (int)len, nullptr, 0);
    if (n == 0) {
        return L"";
    }

    std::vector<wchar_t> buffer;
    buffer.resize(n);
    MultiByteToWideChar(CP_UTF8, 0, str, (int)len, buffer.data(), (int)buffer.size());

    return std::wstring(buffer.begin(), buffer.end());
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

std::wstring GetWideString(const char *str) {
    size_t len = strlen(str);
    return GetWideString(str, len);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

std::wstring GetWideString(const std::string &str) {
    return GetWideString(str.data(), str.size());
}
