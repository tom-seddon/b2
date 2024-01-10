#include <shared/system.h>
#include "test_tube.h"

int main() {
    TestBBCMicroArgs args = {};
    args.flags = TestBBCMicroFlags_ConfigureExTube;
    TestTube("test_tube_xtu_r124",
             TestBBCMicroType_Master128MOS320WithExternal3MHz6502,
             args,
             BEEBLINK_VOLUME_PATH,
             "4",
             "R124",
             0x800,
             "OLD\r*SPOOL X.R124\r",
             "*SPOOL\r");
}
