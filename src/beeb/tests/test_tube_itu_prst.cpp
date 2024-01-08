#include <shared/system.h>
#include "test_tube.h"

int main() {
    TestTube("test_tube_itu_prst",
             TestBBCMicroType_Master128MOS320WithMasterTurbo,
             BEEBLINK_VOLUME_PATH,
             "4",
             "PRST",
             "!&70=&C4FF3AD5\rI%=TRUE\r");
}
