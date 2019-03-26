#include <shared/system.h>
#include <shared/debug.h>
#include <beeb/type.h>

#include <shared/enum_def.h>
#include <beeb/type.inl>
#include <shared/enum_end.h>

#include <6502/6502.h>

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

const BBCMicroType BBC_MICRO_TYPE_B={
    BBCMicroTypeID_B,
    &M6502_nmos6502_config,
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

const BBCMicroType BBC_MICRO_TYPE_B_PLUS={
    BBCMicroTypeID_BPlus,
    &M6502_nmos6502_config,
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

const BBCMicroType BBC_MICRO_TYPE_MASTER={
    BBCMicroTypeID_Master,
    &M6502_cmos6502_config,
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

const BBCMicroType *GetBBCMicroTypeForTypeID(BBCMicroTypeID type_id) {
    switch(type_id) {
        default:
            ASSERT(false);
            // fall through
        case BBCMicroTypeID_B:
            return &BBC_MICRO_TYPE_B;

        case BBCMicroTypeID_BPlus:
            return &BBC_MICRO_TYPE_B_PLUS;

        case BBCMicroTypeID_Master:
            return &BBC_MICRO_TYPE_MASTER;
    }
}

