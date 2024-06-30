#include <shared/system.h>
#include <shared/debug.h>
#include <beeb/roms.h>

#include <shared/enum_def.h>
#include <beeb/roms.inl>
#include <shared/enum_end.h>

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

const ROMTypeMetadata ROM_TYPE_PARASITE_OS = {"Parasite OS (2 KB)", 2048};

static const ROMTypeMetadata ROM_TYPE_METADATA_16KB = {"16 KB", 16384};
static const ROMTypeMetadata ROM_TYPE_METADATA_CCIWORD = {"Inter-Word (32 KB)", 32768};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

const ROMTypeMetadata *GetROMTypeMetadata(ROMType type) {
    switch (type) {
    default:
        ASSERT(false);
        [[fallthrough]];
    case ROMType_16KB:
        return &ROM_TYPE_METADATA_16KB;

    case ROMType_CCIWORD:
        return &ROM_TYPE_METADATA_CCIWORD;
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
