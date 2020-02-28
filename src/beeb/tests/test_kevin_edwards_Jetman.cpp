#include <shared/system.h>
#include "test_kevin_edwards.h"

int main() {
    TestKevinEdwards(BEEBLINK_VOLUME_PATH,2,"Jetman",
                     "P%=&380:[OPT 3:LDX #10:LDY #0:.loop LDA &3000,Y:STA &600,Y:INY:BNE loop:INC loop+2:INC loop+5:DEX:BNE loop:LDA #&1F:STA 0:LDA #&F:STA 1:LDA #1:STA &FC10:JMP &F01:]\rCALL &380\r",
                     true);
}
