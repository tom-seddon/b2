#include <shared/system.h>
#include <shared/debug.h>
#include <beeb/roms.h>

#include <shared/enum_def.h>
#include <beeb/roms.inl>
#include <shared/enum_end.h>

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static const ROMTypeMetadata ROM_TYPES_METADATA[ROMType_Count] = {
    {ROMType_16KB, "16 KB", 16384},
    {ROMType_CCIWORD, "Inter-Word (32 KB)", 32768},
    {ROMType_CCIBASE, "Inter-Base (64 KB)", 65536},
    {ROMType_CCISPELL, "Spellmaster (128 KB)", 131072},
    {ROMType_PALQST, "Quest Paint (32 KB)", 32768},
    {ROMType_PALWAP, "Wapping Editor (64 KB)", 65536},
    {ROMType_PALTED, "TED (32 KB)", 32768},
    {ROMType_ABEP, "PRES ABE+ (32 KB)", 32768},
    {ROMType_ABE, "PRES ABE (32 KB)", 32768},
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

const ROMTypeMetadata *GetROMTypeMetadata(ROMType type) {
    ASSERT(type >= 0 && type < ROMType_Count);
    const ROMTypeMetadata *metadata = &ROM_TYPES_METADATA[type];
    ASSERT(metadata->type == type);
    return metadata;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
