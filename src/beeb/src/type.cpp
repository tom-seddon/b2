#include <shared/system.h>
#include <shared/debug.h>
#include <beeb/type.h>

#include <shared/enum_def.h>
#include <beeb/type.inl>
#include <shared/enum_end.h>

#include <6502/6502.h>

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

const M6502Config *Get6502ConfigForBBCMicroType(BBCMicroType type) {
    switch(type) {
        default:
            ASSERT(false);
            // fall through
        case BBCMicroType_B:
        case BBCMicroType_BPlus:
            return &M6502_nmos6502_config;

        case BBCMicroType_Master:
            return &M6502_cmos6502_config;
    }
}

