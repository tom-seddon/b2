#include <shared/system.h>
#include "test_tube.h"

int main() {
    TestTube("test_tube_xtu_r3",
             TestBBCMicroType_Master128MOS320WithExternal3MHz6502,
             {},
             BEEBLINK_VOLUME_PATH,
             "4",
             "R3",
             0x800,
             "OLD\r*SPOOL X.R3\r",
             "*SPOOL\r");
}
