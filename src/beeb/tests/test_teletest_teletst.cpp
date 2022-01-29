#include <shared/system.h>
#include "test_common.h"
#include <shared/path.h>
#include <shared/testing.h>

int main() {
    TestBBCMicro bbc(TestBBCMicroType_BTape);

    bbc.RunUntilOSWORD0(10.0);

    bbc.LoadFile(GetTestFileName(BEEBLINK_VOLUME_PATH, 0, "$.TELETST"), 0xe00);

    bbc.Paste("OLD\rRUN\r");

    bbc.RunUntilOSWORD0(10.0);

    RunImageTest(PathJoined(BEEBLINK_VOLUME_PATH, "teletst.png"),
                 "teletst",
                 &bbc);
}
