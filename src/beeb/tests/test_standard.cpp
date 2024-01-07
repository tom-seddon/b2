#include <shared/system.h>
#include "test_common.h"

#ifndef CLEAR_TRACE_FLAGS
#define CLEAR_TRACE_FLAGS 0
#endif

#ifndef SET_TRACE_FLAGS
#define SET_TRACE_FLAGS 0
#endif

int main() {
    RunStandardTest(BEEBLINK_VOLUME_PATH,
                    BEEBLINK_DRIVE,
                    BBC_TEST_NAME,
                    BBC_TYPE,
                    CLEAR_TRACE_FLAGS,
                    SET_TRACE_FLAGS);
}
