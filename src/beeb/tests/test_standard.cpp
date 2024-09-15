#include <shared/system.h>

#include <shared/enum_decl.h>
#include "test_common.inl"
#include <beeb/BBCMicro.inl>
#include <shared/enum_end.h>

// Runs one of the standard tests.
//
// Does a _exit(1) if any of the tests fail.
//
// See etc/b2_tests/README.md in the working copy for more about this.
//
// TODO - maybe move this into test_standard.cpp...?
void RunStandardTest(const char *beeblink_volume_path,
                     const char *beeblink_drive,
                     const char *test_name,
                     TestBBCMicroType type,
                     uint32_t clear_trace_flags,
                     uint32_t set_trace_flags);

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
