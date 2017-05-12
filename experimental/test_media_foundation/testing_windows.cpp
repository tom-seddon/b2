#include <shared/system.h>
#include <shared/system_specific.h>
#include <shared/testing.h>
#include <shared/log.h>
#include "testing_windows.h"

__declspec(thread) HRESULT g_last_hr;

void PrintLastHRESULT(const TestFailArgs *tfa) {
    (void)tfa;

    LOGF(ERR,"Last HRESULT: 0x%08lX (%s)\n",g_last_hr,GetErrorDescription(g_last_hr));
}
