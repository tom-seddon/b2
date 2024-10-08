#include <shared/system.h>
#include "test_tube.h"

int main() {
    TestBBCMicroArgs args;
    args.flags = TestBBCMicroFlags_ConfigureNoTube;
    TestTube("test_tube_itu_prst",
             TestBBCMicroType_Master128MOS320WithMasterTurbo,
             args,
             BEEBLINK_VOLUME_PATH,
             "4",
             "PRST",
             0xffff1900,
             "PAGE=&1900\rOLD\r!&70=&C4FF3AD5\rI%=TRUE\r",
             "");
}
