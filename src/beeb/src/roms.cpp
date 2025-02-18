#include <shared/system.h>
#include <shared/debug.h>
#include <beeb/roms.h>

#include <shared/enum_def.h>
#include <beeb/roms.inl>
#include <shared/enum_end.h>

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static const ROMTypeMetadata ROM_TYPES_METADATA[] = {
    {ROMType_16KB, "16 KB", 16384},
    {ROMType_CCIWORD, "Inter-Word (32 KB)", 32768},
    {ROMType_CCIBASE, "Inter-Base (64 KB)", 65536},
    {ROMType_CCISPELL, "Spellmaster (128 KB)", 131072},
    {ROMType_PALQST, "Quest Paint (32 KB)", 32768},
    {ROMType_PALWAP, "Wapping Editor (64 KB)", 65536},
    {ROMType_PALTED, "TED (32 KB)", 32768},
    {ROMType_ABEP, "PRES ABE+ (32 KB)", 32768},
    {ROMType_ABE, "PRES ABE (32 KB)", 32768},
    {ROMType_Trilogy, "View Trilogy (64 KB)", 65536},
    {ROMType_MO2, "Mini Office II (128 KB)", 131072},
};
static_assert(sizeof ROM_TYPES_METADATA / sizeof ROM_TYPES_METADATA[0] == ROMType_Count);

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

static const OSROMTypeMetadata OS_ROM_TYPES_METADATA[OSROMType_Count] = {
    {OSROMType_16KB, "16 KB", 16384, 0, 16384},
    {OSROMType_Compact, "Master Compact (64 KB)", 65536, 0, 65536},
    {OSROMType_MegaROM, "MegaROM (128 KB)", 131072, 0, 131072},
    {OSROMType_MultiOSBank0, "Multi-OS (512 KB) bank 0", 524288, 0 * 131072, 131072},
    {OSROMType_MultiOSBank1, "Multi-OS (512 KB) bank 1", 524288, 1 * 131072, 131072},
    {OSROMType_MultiOSBank2, "Multi-OS (512 KB) bank 2", 524288, 2 * 131072, 131072},
    {OSROMType_MultiOSBank3, "Multi-OS (512 KB) bank 3", 524288, 3 * 131072, 131072},
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

const OSROMTypeMetadata *GetOSROMTypeMetadata(OSROMType type) {
    ASSERT(type >= 0 && type < OSROMType_Count);
    const OSROMTypeMetadata *metadata = &OS_ROM_TYPES_METADATA[type];
    ASSERT(metadata->type == type);
    return metadata;
}

uint8_t GetNumNonOSSidewaysROMs(OSROMType type) {
    const OSROMTypeMetadata *metadata = GetOSROMTypeMetadata(type);
    ASSERT(metadata->rom_size_bytes >= 16384);
    ASSERT(metadata->rom_size_bytes % 16384 == 0);
    size_t n = (metadata->rom_size_bytes - 16384) / 16384;
    ASSERT(n < UINT8_MAX);
    return (uint8_t)n;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
