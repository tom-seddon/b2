#include <shared/system.h>
#include "test_kevin_edwards.h"

int main() {
    TestKevinEdwards(BEEBLINK_VOLUME_PATH,2,"Nightsh",
                     "P%=&3900:[OPT3:LDX #9:LDY #0:.loop LDA &3000,Y:STA &700,Y:INY:BNE loop:INC loop+2:INC loop+5:DEX:BNE loop:LDA #&40:STA0:LDA #&F:STA 1:LDA #1:STA &FC10:JMP &F01:]\rCALL &3900\r",
                     false);
}
