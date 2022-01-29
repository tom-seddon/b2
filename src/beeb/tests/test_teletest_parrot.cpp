#include <shared/system.h>
#include "test_common.h"
#include <shared/path.h>
#include <shared/testing.h>

int main() {
    TestBBCMicro bbc(TestBBCMicroType_BTape);

    bbc.RunUntilOSWORD0(10.0);

    bbc.LoadFile(GetTestFileName(BEEBLINK_VOLUME_PATH, 0, "$.RED"), 0x7c00);

    RunImageTest(PathJoined(BEEBLINK_VOLUME_PATH, "red.png"),
                 "red",
                 &bbc);
}
