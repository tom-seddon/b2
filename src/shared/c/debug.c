#include <shared/system.h>
#include <shared/debug.h>
#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <string.h>
#include <shared/system_specific.h>

#if SYSTEM_OSX
#include <mach-o/getsect.h>
#include <mach-o/dyld.h>
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static void DumpProcessLoadAddress(void) {
#if SYSTEM_OSX

    /* This is a pretty stinky workaround for backtrace_symbols's
     * inability to print line numbers and the ASLR bullshit. */

    /* http://stackoverflow.com/a/22625223/1618406 */
    const struct segment_command_64 *command = getsegbyname("__TEXT");
    if (!command) {
        return;
    }

    intptr_t slide = 0;

    char path[4096];
    uint32_t size = sizeof path;
    if (_NSGetExecutablePath(path, &size) == -1) {
        return;
    }

    for (uint32_t i = 0; i < _dyld_image_count(); ++i) {
        if (strcmp(_dyld_get_image_name(i), path) == 0) {
            slide = _dyld_get_image_vmaddr_slide(i);
            break;
        }
    }

    fprintf(stderr, "Load address: 0x%" PRIx64 "\n", (uint64_t)(command->vmaddr + (uint64_t)slide));
#endif
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static void DumpStackTrace(const char *function) {
    void *buffer[100];
    int n = backtrace(buffer, sizeof buffer / sizeof buffer[0]);

    fprintf(stderr, "In function: %s\n", function);

    // This isn't actually much use with glibc, because it only finds
    // non-static symbols, and those only when the stars are aligned.
    char **symbols = GetBacktraceSymbols(buffer, n);
    if (symbols) {
        fprintf(stderr, "Stack trace:\n");

        for (int i = 0; i < n; ++i) {
            fprintf(stderr, "    %d. %s\n", i, symbols[i]);
        }

        free(symbols);
        symbols = NULL;
    }

    DumpProcessLoadAddress();
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void LogAssertFailed(const char *file, int line, const char *function, const char *expr) {
    fprintf(stderr, PRIfileline " assertion failed: %s\n", file, line, expr);

    DumpStackTrace(function);

    fprintf(stderr, PRIfileline " assertion failed: %s\n", file, line, expr);
    fflush(stderr);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void LogAssertElaboration(const char *fmt, ...) {
    va_list v;

    fprintf(stderr, "Additional info: ");
    fflush(stderr);

    va_start(v, fmt);
    vfprintf(stderr, fmt, v);
    fflush(stderr);
    va_end(v);

    fprintf(stderr, "\n");
    fflush(stderr);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

/* "handle" */
void HandleAssertFailed(void) {
    if (IsDebuggerAttached()) {
        // When the debugger is attached, it ought to be possible to
        // resume execution, to see what happens.
        return;
    }

    abort();
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
