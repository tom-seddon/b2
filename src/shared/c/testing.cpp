#include <shared/system.h>
#include <shared/testing.h>
#include <shared/log.h>

#include <stdio.h>
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>
#include <vector>
#include <algorithm>
#include <shared/system_specific.h>

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

LOG_DEFINE(TESTING, "", &log_printer_stderr_and_debugger);

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

struct TestFailCallback {
    TestFailFn fn;
    void *context;
};

std::vector<TestFailCallback> g_fail_fns;

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if SYSTEM_WINDOWS

static LONG WINAPI HandleUnhandledException(struct _EXCEPTION_POINTERS *e) {
    (void)e;

    return EXCEPTION_EXECUTE_HANDLER;
}

static BOOL CALLBACK TestStartupInitOnce(PINIT_ONCE InitOnce,
                                         PVOID Parameter,
                                         PVOID *lpContext) {
    (void)InitOnce, (void)Parameter, (void)lpContext;
    /* This doesn't actually help - the WER dialog still pops up. */
    /* UINT old_mode=SetErrorMode(SEM_FAILCRITICALERRORS); */
    /* SetErrorMode(old_mode|SEM_FAILCRITICALERRORS); */

    /* This seems to be enough to convince the WER system that the
     * exception was anticipated. */
    SetUnhandledExceptionFilter(&HandleUnhandledException);

    return TRUE;
}

static INIT_ONCE g_test_startup_init_once = INIT_ONCE_STATIC_INIT;

void TestStartup(void) {
    InitOnceExecuteOnce(&g_test_startup_init_once,
                        &TestStartupInitOnce,
                        NULL,
                        NULL);
}

#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if !SYSTEM_WINDOWS
void TestStartup(void) {
}
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void AddTestFailFn(TestFailFn fn, void *context) {
    g_fail_fns.push_back({fn, context});
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void RemoveTestFailFnByContext(void *context) {
    g_fail_fns.erase(std::remove_if(g_fail_fns.begin(), g_fail_fns.end(), [=](auto &&x) {
                         return x.context == context;
                     }),
                     g_fail_fns.end());
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void TestFailed(const char *file, int line, const TestFailArgs *tfa) {
    fflush(stdout);
    fflush(stderr);

    for (const TestFailCallback &ff : g_fail_fns) {
        TestFailArgs tmp_tfa;

        if (!tfa) {
            memset(&tmp_tfa, 0, sizeof tmp_tfa);
        } else {
            tmp_tfa = *tfa;
        }

        tmp_tfa.context = ff.context;
        (*ff.fn)(&tmp_tfa);
    }

#ifdef _MSC_VER
    LOGF(TESTING, "%s(%d): test failure:\n", file, line);
#else
    LOGF(TESTING, "%s:%d: test failure:\n", file, line);
#endif
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void TestQuit(void) {
    fflush(stderr);
    fflush(stdout);

    //if(IsDebuggerAttached()) {
    //    DEBUG_BREAK();
    //}

    /* This does the same sort of thing as abort(), but in my tests on
     * OS X it was a lot quicker... */
    _exit(1);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void TestFailedII(int64_t lhs,
                  const char *lhs_str,
                  int64_t rhs,
                  const char *rhs_str,
                  const char *oper,
                  const char *file,
                  int line) {
    TestFailArgs tfa = {};
    tfa.lhs_int64 = &lhs;
    tfa.rhs_int64 = &rhs;

    TestFailed(file, line, &tfa);

    LOGF(TESTING, "       LHS expr: %s\n", lhs_str);
    LOGF(TESTING, "      Condition: %s\n", oper);
    LOGF(TESTING, "       RHS expr: %s\n", rhs_str);
    LOGF(TESTING, "\n");
    LOGF(TESTING, "      LHS value: %20" PRId64 " 0x%016" PRIx64 "\n", lhs, lhs);
    LOGF(TESTING, "      RHS value: %20" PRId64 " 0x%016" PRIx64 "\n", rhs, rhs);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void TestFailedUU(uint64_t lhs,
                  const char *lhs_str,
                  uint64_t rhs,
                  const char *rhs_str,
                  const char *oper,
                  const char *file,
                  int line) {
    TestFailed(file, line, NULL); //,(TestFailArgs){.lhs_uint64=&lhs,.wanted_uint64=&wanted});

    LOGF(TESTING, "       LHS expr: %s\n", lhs_str);
    LOGF(TESTING, "      Condition: %s\n", oper);
    LOGF(TESTING, "       RHS expr: %s\n", rhs_str);
    LOGF(TESTING, "\n");
    LOGF(TESTING, "      LHS value: %20" PRIu64 " 0x%016" PRIx64 "\n", lhs, lhs);
    LOGF(TESTING, "      RHS value: %20" PRIu64 " 0x%016" PRIx64 "\n", rhs, rhs);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

int TestBool(int got, const char *got_str, int wanted, const char *file, int line) {
    if (!!got != wanted) {
        TestFailed(file, line, NULL); //,(TestFailArgs){.got_bool=&got,.wanted_bool=&wanted});

        LOGF(TESTING, "    Got expr: %s\n", got_str);
        LOGF(TESTING, "\n");
        LOGF(TESTING, "    Wanted value: %s\n", BOOL_STR(wanted));
        LOGF(TESTING, "    Got value: %s\n", BOOL_STR(got));

        return 0;
    } else {
        return 1;
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

int TestPointer(int got_null, const char *got_str, int want_non_null, const char *file, int line) {
    if (got_null != want_non_null) {
        TestFailed(file, line, NULL); //,(TestFailArgs){.got_pointer=got,.w

        LOGF(TESTING, "    Got expr: %s\n", got_str);
        LOGF(TESTING, "\n");
        LOGF(TESTING, "    Wanted: %s\n", want_non_null ? "non-NULL" : "NULL");
        //LOGF(TESTING,"    Got value: %p\n",got);

        return 0;
    } else {
        return 1;
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

int TestEqSS(const char *got, const char *got_str, const char *wanted, const char *wanted_str, const char *file, int line) {
    if ((!got && wanted) ||
        (got && !wanted) ||
        (wanted && got && strcmp(got, wanted) != 0)) {
        TestFailed(file, line, NULL); //,(TestFailArgs){.got_str=got,.wanted_str=wanted});

        LOGF(TESTING, "    Wanted expr: %s\n", wanted_str);
        LOGF(TESTING, "       Got expr: %s\n", got_str);
        LOGF(TESTING, "\n");

        Log tmp("", LOG(TESTING));

        tmp.s("    Wanted value: \"");
        LogStringPrintable(&tmp, wanted);
        tmp.s("\"\n");

        tmp.s("       Got value: \"");
        LogStringPrintable(&tmp, got);
        tmp.s("\"\n");

        return 0;
    } else {
        return 1;
    }
}

int TestEqSS(const std::string &got, const char *got_str, const std::string &wanted, const char *wanted_str, const char *file, int line) {
    return TestEqSS(got.c_str(), got_str, wanted.c_str(), wanted_str, file, line);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static int DoHighlightValue(size_t offset, void *data) {
    return offset == *(size_t *)data;
}

int TestEqAA(const void *got, const char *got_str, const void *wanted, const char *wanted_str, size_t n, const char *file, int line) {
    const uint8_t *g = (const uint8_t *)got, *w = (const uint8_t *)wanted;

    /* It's most likely the buffers are the same, so check with memcmp
     * first. It's a lot quicker in an unoptimised build. */
    if (memcmp(g, w, n) == 0) {
        return 1;
    }

    for (size_t i = 0; i < n; ++i) {
        if (g[i] != w[i]) {
            TestFailed(file, line, NULL); //,(TestFailArgs){.got_array=got,.wanted_array=wanted,.array_size=n});

            LOGF(TESTING, "    Wanted expr: %s\n", wanted_str);
            LOGF(TESTING, "       Got expr: %s\n", got_str);
            LOGF(TESTING, "\n");

            size_t begin = i & ~15u;
            if (begin >= 16) {
                begin -= 16;
            }

            size_t end = begin + 48;
            if (end > n) {
                end = n;
            }

            LOGF(TESTING, "Got: ");
            {
                LOGI(TESTING);
                LogDumpBytesExData dbed = {};
                dbed.highlight_fn = &DoHighlightValue;
                dbed.highlight_data = &i;
                LogDumpBytesEx(&LOG(TESTING), (const uint8_t *)g + begin, end - begin, &dbed);
            }
            LOGF(TESTING, "\n");

            LOGF(TESTING, "Want: ");
            {
                LOGI(TESTING);
                LogDumpBytesExData dbed = {};
                dbed.highlight_fn = &DoHighlightValue;
                dbed.highlight_data = &i;

                LogDumpBytesEx(&LOG(TESTING), (const uint8_t *)w + begin, end - begin, &dbed);
            }
            LOGF(TESTING, "\n");

            LOGF(TESTING, "    Mismatch index: %zu/%zu (0x%zx/0x%zx)\n", i, n, i, n);
            LOGF(TESTING, "      Wanted value: %u (0x%X)\n", w[i], w[i]);
            LOGF(TESTING, "         Got value: %u (0x%X)\n", g[i], g[i]);
        }
    }

    return 0;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

int TestEqPP(const void *got, const char *got_str, const void *wanted, const char *wanted_str, const char *file, int line) {
    if (got != wanted) {
        TestFailed(file, line, NULL); //,(TestFailArgs){.got_ptr=got,.wanted_ptr=wanted});

        LOGF(TESTING, "    Wanted expr: %s\n", wanted_str);
        LOGF(TESTING, "       Got expr: %s\n", got_str);
        LOGF(TESTING, "\n");
        LOGF(TESTING, "    Wanted value: %p\n", wanted);
        LOGF(TESTING, "       Got value: %p\n", got);

        return 0;
    } else {
        return 1;
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void TestFail(const char *file, int line, const char *fmt, ...) {
    TestFailed(file, line, nullptr);

    LOGF(TESTING, "Test failure: ");

    va_list v;
    va_start(v, fmt);
    LOGV(TESTING, fmt, v);
    va_end(v);

    LOG(TESTING).EnsureBOL();
}
