#include <shared/system.h>
#include "native_ui_private.h"

#include <shared/enum_def.h>
#include "native_ui_private.inl"
#include <shared/enum_end.h>

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

Mutex g_native_ui_globals_mutex;
static MutexNameSetter g_native_ui_globals_mutex_name(&g_native_ui_globals_mutex, "native_ui_globals");
NativeUiModalState g_native_ui_modal_state;

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool WaitForModalNotOpen(double timeout_seconds) {
    uint64_t begin_ticks = GetCurrentTickCount();

    while (GetSecondsFromTicks(GetCurrentTickCount() - begin_ticks) < timeout_seconds) {
        {
            LockGuard<Mutex> lock(g_native_ui_globals_mutex);

            if (g_native_ui_modal_state == NativeUiModalState_NotOpen) {
                return true;
            }
        }
        SleepMS(1);
    }

    return false;
}
