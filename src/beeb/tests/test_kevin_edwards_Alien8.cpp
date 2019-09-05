#include <shared/system.h>
#include "test_kevin_edwards.h"

int main() {
    TestKevinEdwards("Alien8",
                     "P%=&900:[OPT 2:LDX #4:LDY #0:.loop LDA &3000,Y:STA &C00,Y:INY:BNE loop:INC loop+2:INC loop+5:DEX:BNE loop:LDA #1:STA &FC10:JMP &CEA:]\rCALL &900\r",
                     false);
}
