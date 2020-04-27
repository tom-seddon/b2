#include <shared/system.h>
#include "test_common.h"
#include <shared/path.h>
#include <shared/testing.h>

int main() {
    TestBBCMicro bbc(TestBBCMicroType_BTape);

    bbc.RunUntilOSWORD0(10.0);

    bbc.LoadFile(GetTestFileName(BEEBLINK_VOLUME_PATH,0,"$.ENGTEST"),0x7c00);

    RunImageTest(PathJoined(BEEBLINK_VOLUME_PATH,"engtest.png"),
                 "engtest",
                 &bbc);
}
