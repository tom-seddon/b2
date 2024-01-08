#include <shared/system.h>
#include "test_tube.h"

int main() {
    TestTube("test_tube_xtu_prst",
             TestBBCMicroType_Master128MOS320WithExternal3MHz6502,
             TestBBCMicroFlags_ConfigureNoTube,
             BEEBLINK_VOLUME_PATH,
             "4",
             "PRST",
             0xffff1900,
             "PAGE=&1900\rOLD\r!&70=&C4FF3AD5\rI%=FALSE\r",
             "");
}
