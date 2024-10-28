#include <shared/system.h>
#include <shared/debug.h>
#include <shared/log.h>
#include <beeb/conf.h>
#include <beeb/type.h>
#include <6502/6502.h>
#include <string.h>
#include <map>

#include <shared/enum_def.h>
#include <beeb/type.inl>
#include <shared/enum_end.h>

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// Total max addressable memory in the emulated system is 2,194K:
//
// - 64K RAM (main+shadow+ANDY+HAZEL)
// - 16 * 128K ROM
// - 16K MOS
// - 64K parasite RAM
// - 2K parasite ROM
//
// The paging generally operates at a 4K resolution, so this can be divided into
// 549 4K pages, or (to pick a term) big pages. (1 big page = 16 pages.) The
// following big pages are defined:
//
// <pre>
// 8    main RAM
// 1    ANDY (M128)/ANDY (B+)
// 2    HAZEL (M128)/ANDY (B+)
// 5    shadow RAM (M128/B+)
// 32   ROM 0 (actually typically 4, but ROM mappers may demand more)
// 32   ROM 1 (see above)
// ...
// 32   ROM 15 (see above)
// 4    MOS
// 16   parasite RAM
// 1    parasite ROM
// </pre>
//
// Each big page can be set up once, when the BBCMicro is first created,
// simplifying some of the logic. When switching to ROM 1 region w, for example,
// the buffers can be found by looking at the 32 pre-prepared big pages for that
// bank, and then the 4 pre-prepared big pages for that region - rather than
// having to check m_state.sideways_rom_buffers[1] (etc.). This also then covers
// the mapper behaviour fairly transparently.
//
// (This mechanism is still a little untidy. There's some logic in
// BBCMicro::InitReadOnlyBigPage that should probably go somewhere better.)
//
// The per-big page struct can also contain some cold info (debug flags index,
// static data, etc.), as it's only fetched when the paging registers are
// changed, rather than for every instruction.
//
// Regarding the debug flags index: it's possible for multiple big pages to
// share debug flags. For example, with the PALQST ROM mapper, the same data is
// always visible at $8000-$8fff regardless of region. So whether region 0 is
// selected (ROMn_BIG_PAGE_INDEX+0*4+0), or region 1
// (ROMn_BIG_PAGE_INDEX+1*4+0), or whatever, the same debug flags should apply
// to the region visible at $8000-$8fff. The big pages metadata table is scanned
// after creation to figure out which big pages should alias in this way and
// assign a debug flags index for each one.
//
// In principle the debug flags indexes could be entirely dynamically generated.
// But: they're not. There's a(n enormous) table in BBCMicro::DebugState, and
// each big page's index into that is either its own index in the type's table,
// or its alias's index. More untidiness - though this does mean a DebugState is
// (somewhat) reusable regardless of BBCMicroType...
//
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
//
// The BBC memory map is also divided into big pages, so things match up - the
// terminology is a bit slack but usually a 'big page' refers to one of the big
// pages in the list above, and a 'memory/mem big page' refers to a big page in
// the 6502 address space.
//
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
//
// The Master can have 2 parasites, when there's both internal and external
// second processors connected. This isn't supported currently.

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// Address suffixes:
//
// <pre>
// 0 - sideways ROM 0
// 1 - sideways ROM 1
// 2 - sideways ROM 2
// 3 - sideways ROM 3
// 4 - sideways ROM 4
// 5 - sideways ROM 5
// 6 - sideways ROM 6
// 7 - sideways ROM 7
// 8 - sideways ROM 8
// 9 - sideways ROM 9
// a - sideways ROM a
// b - sideways ROM b
// c - sideways ROM c
// d - sideways ROM d
// e - sideways ROM e
// f - sideways ROM f
// g -
// h - HAZEL
// i - I/O
// j -
// k -
// l -
// m - main RAM
// n - ANDY
// o - OS ROM
// p - parasite RAM
// q -
// r - parasite boot ROM
// s - shadow RAM
// t -
// u -
// v -
// w - ROM mapper bank 0
// x - ROM mapper bank 1
// y - ROM mapper bank 2
// z - ROM mapper bank 3
// W - ROM mapper bank 4
// X - ROM mapper bank 5
// Y - ROM mapper bank 6
// Z - ROM mapper bank 7
// </pre>
//
// How the ROM mapper bank affects things depends on the ROM mapper type.

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static const char ROM_BANK_CODES[] = "0123456789abcdef";
static const char MAPPER_REGION_CODES[] = "wxyzWXYZ";

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

char GetROMBankCode(uint32_t bank) {
    ASSERT(bank < 16);
    return ROM_BANK_CODES[bank];
}

char GetMapperRegionCode(uint32_t region) {
    ASSERT(region < 8);
    return MAPPER_REGION_CODES[region];
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static bool IsMaster(BBCMicroTypeID type_id) {
    return type_id == BBCMicroTypeID_Master || type_id == BBCMicroTypeID_MasterCompact;
}

static bool IsB(BBCMicroTypeID type_id) {
    return type_id == BBCMicroTypeID_B || type_id == BBCMicroTypeID_BPlus;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if BBCMICRO_DEBUGGER
static void ApplyROMDSO(PagingState *paging, uint32_t dso) {
    if (dso & BBCMicroDebugStateOverride_OverrideROM) {
        paging->romsel.b_bits.pr = dso & BBCMicroDebugStateOverride_ROM;
    }

    if (dso & BBCMicroDebugStateOverride_OverrideMapperRegion) {
        // This is sort-of wrong, in that it only overrides the mapper region
        // for the current ROM. But you can't tell from the UI.
        paging->rom_regions[paging->romsel.b_bits.pr] = dso >> BBCMicroDebugStateOverride_MapperRegionShift & BBCMicroDebugStateOverride_MapperRegionMask;
    }
}
#endif

#if BBCMICRO_DEBUGGER
static uint32_t GetROMDSO(const PagingState &paging) {
    uint32_t dso = 0;

    dso |= paging.romsel.b_bits.pr;
    dso |= BBCMicroDebugStateOverride_OverrideROM;

    if (paging.rom_types[paging.romsel.b_bits.pr] != ROMType_16KB) {
        dso |= BBCMicroDebugStateOverride_OverrideMapperRegion;
        dso |= (uint32_t)(paging.rom_regions[paging.romsel.b_bits.pr] << BBCMicroDebugStateOverride_MapperRegionShift);
    }

    return dso;
}
#endif

static std::string g_all_big_page_codes;

static void AddBigPageCode(char code) {
    if (g_all_big_page_codes.find(code) == std::string::npos) {
        g_all_big_page_codes.push_back(code);
    }
}

static void InitBigPagesMetadata(std::vector<BigPageMetadata> *big_pages,
                                 BigPageIndex index,
                                 size_t n,
                                 char code0,
                                 char code1,
                                 const std::string &description,
#if BBCMICRO_DEBUGGER
                                 uint32_t dso_clear,
                                 uint32_t dso_set,
#endif
                                 uint16_t base) {
    for (size_t i = 0; i < n; ++i) {
        ASSERT(index.i + i < NUM_BIG_PAGES);
        BigPageMetadata *bp = &(*big_pages)[index.i + i];

        static_assert(sizeof bp->aligned_codes >= 3);
        static_assert(sizeof bp->minimal_codes >= 3);
        ASSERT(code0 != 0);

        if (code1 == 0) {
            bp->aligned_codes[0] = code0;
            bp->aligned_codes[1] = ' ';

            bp->minimal_codes[0] = code0;
        } else {
            bp->aligned_codes[0] = code0;
            bp->aligned_codes[1] = code1;

            bp->minimal_codes[0] = code0;
            bp->minimal_codes[1] = code1;
        }

        bp->description = description;

#if BBCMICRO_DEBUGGER
        bp->dso_mask = ~dso_clear;
        bp->dso_value = dso_set;
#endif

        bp->addr = (uint16_t)(base + i * 4096);
    }

    AddBigPageCode(code0);
    if (code1 != 0) {
        AddBigPageCode(code1);
    }
}

size_t GetROMOffset(ROMType rom_type, uint32_t relative_big_page_index, uint32_t region) {
    ASSERT(relative_big_page_index < 4);
    ASSERT(region < 8);

    switch (rom_type) {
    default:
        ASSERT(false);
        [[fallthrough]];
    case ROMType_16KB:
        return relative_big_page_index * BIG_PAGE_SIZE_BYTES;

    case ROMType_CCIWORD:
    case ROMType_ABEP:
    case ROMType_ABE:
        return (region & 1) * 4 * BIG_PAGE_SIZE_BYTES + relative_big_page_index * BIG_PAGE_SIZE_BYTES;

    case ROMType_CCIBASE:
        return (region & 3) * 4 * BIG_PAGE_SIZE_BYTES + relative_big_page_index * BIG_PAGE_SIZE_BYTES;

    case ROMType_CCISPELL:
        return region * 4 * BIG_PAGE_SIZE_BYTES + relative_big_page_index * BIG_PAGE_SIZE_BYTES;

    case ROMType_PALQST:
    case ROMType_PALTED:
        switch (relative_big_page_index) {
        default:
            ASSERT(false);
            [[fallthrough]];
        case 0:
        case 1:
            return relative_big_page_index * BIG_PAGE_SIZE_BYTES;

        case 2:
        case 3:
            return ((region & 3u) << 1u | (relative_big_page_index & 1u)) * BIG_PAGE_SIZE_BYTES;
        }
        break;

    case ROMType_PALWAP:
        switch (relative_big_page_index) {
        default:
            ASSERT(false);
            [[fallthrough]];
        case 0:
        case 1:
            return relative_big_page_index * BIG_PAGE_SIZE_BYTES;

        case 2:
        case 3:
            return ((region & 7u) << 1u | (relative_big_page_index & 1u)) * BIG_PAGE_SIZE_BYTES;
        }
        break;
    }
}

static std::vector<BigPageMetadata> GetBigPagesMetadataCommon(const ROMType *rom_types) {
    std::vector<BigPageMetadata> big_pages;
    big_pages.resize(NUM_BIG_PAGES);

    InitBigPagesMetadata(&big_pages,
                         MAIN_BIG_PAGE_INDEX,
                         NUM_MAIN_BIG_PAGES,
                         'm', 0, "Main RAM",
#if BBCMICRO_DEBUGGER
                         0,
                         0,
#endif
                         0x0000);

    for (uint32_t bank = 0; bank < 16; ++bank) {
        char bank_code = GetROMBankCode(bank);

        char rom_desc[100];
        snprintf(rom_desc, sizeof rom_desc, "ROM %c", bank_code);

        for (uint32_t region = 0; region < 8; ++region) {
            char region_code = GetMapperRegionCode(region);

            char rom_and_region_desc[100];
            snprintf(rom_and_region_desc, sizeof rom_and_region_desc, "ROM %c (Region %c)", bank_code, region_code);

            BigPageIndex::Type base_big_page_index = (BigPageIndex::Type)(ROM0_BIG_PAGE_INDEX.i + (size_t)bank * NUM_ROM_BIG_PAGES + region * 4);

            switch (rom_types[bank]) {
            default:
                ASSERT(false);
                [[fallthrough]];
            case ROMType_16KB:
                InitBigPagesMetadata(&big_pages,
                                     {base_big_page_index},
                                     4,
                                     bank_code, 0, rom_desc,
#if BBCMICRO_DEBUGGER
                                     (uint32_t)BBCMicroDebugStateOverride_ROM,
                                     BBCMicroDebugStateOverride_ROM | bank,
#endif
                                     0x8000);
                break;

            case ROMType_CCIWORD:
            case ROMType_CCIBASE:
            case ROMType_CCISPELL:
            case ROMType_ABEP:
            case ROMType_ABE:
                // Mapper selects region visible $8000-$bfff.
                InitBigPagesMetadata(&big_pages,
                                     {base_big_page_index},
                                     4,
                                     bank_code, region_code, rom_and_region_desc,
#if BBCMICRO_DEBUGGER
                                     BBCMicroDebugStateOverride_ROM | BBCMicroDebugStateOverride_MapperRegionMask << BBCMicroDebugStateOverride_MapperRegionShift,
                                     BBCMicroDebugStateOverride_ROM | bank | BBCMicroDebugStateOverride_OverrideMapperRegion | bank << BBCMicroDebugStateOverride_MapperRegionShift,
#endif
                                     0x8000);
                break;

            case ROMType_PALQST:
            case ROMType_PALWAP:
            case ROMType_PALTED:
                // Mapper selects region visible $a000-$bfff.
                InitBigPagesMetadata(&big_pages,
                                     {base_big_page_index},
                                     2,
                                     bank_code, 0, rom_desc,
#if BBCMICRO_DEBUGGER
                                     BBCMicroDebugStateOverride_ROM,
                                     BBCMicroDebugStateOverride_ROM | bank,
#endif
                                     0x8000);

                InitBigPagesMetadata(&big_pages,
                                     {(BigPageIndex::Type)(base_big_page_index + 2)},
                                     2,
                                     bank_code, region_code, rom_and_region_desc,
#if BBCMICRO_DEBUGGER
                                     BBCMicroDebugStateOverride_ROM | BBCMicroDebugStateOverride_MapperRegionMask << BBCMicroDebugStateOverride_MapperRegionShift,
                                     BBCMicroDebugStateOverride_ROM | bank | BBCMicroDebugStateOverride_OverrideMapperRegion | bank << BBCMicroDebugStateOverride_MapperRegionShift,
#endif
                                     0xa000);
                break;
            }
        }
    }

    InitBigPagesMetadata(&big_pages,
                         MOS_BIG_PAGE_INDEX,
                         NUM_MOS_BIG_PAGES,
                         'o', 0, "MOS ROM",
#if BBCMICRO_DEBUGGER
                         0,
                         0,
#endif
                         0xc000);

    InitBigPagesMetadata(&big_pages,
                         PARASITE_BIG_PAGE_INDEX,
                         NUM_PARASITE_BIG_PAGES,
                         'p', 0, "Parasite",
#if BBCMICRO_DEBUGGER
                         0,
                         0,
#endif
                         0x0000);

    InitBigPagesMetadata(&big_pages,
                         PARASITE_ROM_BIG_PAGE_INDEX,
                         NUM_PARASITE_ROM_BIG_PAGES,
                         'r', 0, "Parasite ROM",
#if BBCMICRO_DEBUGGER
                         0,
                         BBCMicroDebugStateOverride_OverrideParasiteROM | BBCMicroDebugStateOverride_ParasiteROM,
#endif
                         0xf000);

    for (size_t i = 0; i < NUM_PARASITE_BIG_PAGES; ++i) {
        big_pages[PARASITE_BIG_PAGE_INDEX.i + i].is_parasite = true;
    }

    for (size_t i = 0; i < NUM_PARASITE_ROM_BIG_PAGES; ++i) {
        big_pages[PARASITE_ROM_BIG_PAGE_INDEX.i + i].is_parasite = true;
    }

    return big_pages;
}

#if BBCMICRO_DEBUGGER
static bool HandleROMSuffixChar(uint32_t *dso, uint8_t rom) {
    ASSERT(rom >= 0 && rom <= 15);

    *dso &= ~(BBCMicroDebugStateOverride_ROM | BBCMicroDebugStateOverride_ANDY);
    *dso |= BBCMicroDebugStateOverride_OverrideANDY | BBCMicroDebugStateOverride_OverrideROM | rom;

    return true;
}
#endif

#if BBCMICRO_DEBUGGER
// select ROM
static bool ParseROMSuffixChar(uint32_t *dso, char c) {
    if (c >= '0' && c <= '9') {
        return HandleROMSuffixChar(dso, (uint8_t)(c - '0'));
    } else if (c >= 'a' && c <= 'f') {
        return HandleROMSuffixChar(dso, (uint8_t)(c - 'a' + 10));
    } else {
        return false;
    }
}
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static void GetMemBigPageTablesB(MemoryBigPageTables *tables,
                                 uint32_t *paging_flags,
                                 const PagingState &paging) {
    tables->mem_big_pages[0][0].i = MAIN_BIG_PAGE_INDEX.i + 0;
    tables->mem_big_pages[0][1].i = MAIN_BIG_PAGE_INDEX.i + 1;
    tables->mem_big_pages[0][2].i = MAIN_BIG_PAGE_INDEX.i + 2;
    tables->mem_big_pages[0][3].i = MAIN_BIG_PAGE_INDEX.i + 3;
    tables->mem_big_pages[0][4].i = MAIN_BIG_PAGE_INDEX.i + 4;
    tables->mem_big_pages[0][5].i = MAIN_BIG_PAGE_INDEX.i + 5;
    tables->mem_big_pages[0][6].i = MAIN_BIG_PAGE_INDEX.i + 6;
    tables->mem_big_pages[0][7].i = MAIN_BIG_PAGE_INDEX.i + 7;

    BigPageIndex::Type rom = ROM0_BIG_PAGE_INDEX.i + paging.romsel.b_bits.pr * NUM_ROM_BIG_PAGES + paging.rom_regions[paging.romsel.b_bits.pr] * 4;
    tables->mem_big_pages[0][0x8].i = rom + 0;
    tables->mem_big_pages[0][0x9].i = rom + 1;
    tables->mem_big_pages[0][0xa].i = rom + 2;
    tables->mem_big_pages[0][0xb].i = rom + 3;

    tables->mem_big_pages[0][0xc].i = MOS_BIG_PAGE_INDEX.i + 0;
    tables->mem_big_pages[0][0xd].i = MOS_BIG_PAGE_INDEX.i + 1;
    tables->mem_big_pages[0][0xe].i = MOS_BIG_PAGE_INDEX.i + 2;
    tables->mem_big_pages[0][0xf].i = MOS_BIG_PAGE_INDEX.i + 3;

    memset(tables->mem_big_pages[1], 0, sizeof tables->mem_big_pages[1]);
    memset(tables->pc_mem_big_pages_set, 0, sizeof tables->pc_mem_big_pages_set);
    *paging_flags = 0;
}

#if BBCMICRO_DEBUGGER
static void ApplyDSOB(PagingState *paging, uint32_t dso) {
    ApplyROMDSO(paging, dso);
}
#endif

#if BBCMICRO_DEBUGGER
static uint32_t GetDSOB(const PagingState &paging) {
    uint32_t dso = 0;

    dso |= GetROMDSO(paging);

    return dso;
}
#endif

static std::vector<BigPageMetadata> GetBigPagesMetadataB(const ROMType *rom_types) {
    std::vector<BigPageMetadata> big_pages = GetBigPagesMetadataCommon(rom_types);

    return big_pages;
}

#if BBCMICRO_DEBUGGER
static bool ParseSuffixCharB(uint32_t *dso, char c) {
    if (ParseROMSuffixChar(dso, c)) {
        // ...
    } else {
        return false;
    }

    return true;
}
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// YXE  Usr  MOS
// ---  ---  ---
// 000   M    M
// 001   M    S
// 010   S    M
// 011   S    S
// 100   M    M
// 101   M    M
// 110   S    S
// 111   S    S
//
// Usr Shadow = X
//
// MOS Shadow = (Y AND X) OR (NOT Y AND E)

static void GetMemBigPageTablesBPlus(MemoryBigPageTables *tables,
                                     uint32_t *paging_flags,
                                     const PagingState &paging) {
    tables->mem_big_pages[0][0].i = MAIN_BIG_PAGE_INDEX.i + 0;
    tables->mem_big_pages[0][1].i = MAIN_BIG_PAGE_INDEX.i + 1;
    tables->mem_big_pages[0][2].i = MAIN_BIG_PAGE_INDEX.i + 2;
    tables->mem_big_pages[0][3].i = MAIN_BIG_PAGE_INDEX.i + 3;
    tables->mem_big_pages[0][4].i = MAIN_BIG_PAGE_INDEX.i + 4;
    tables->mem_big_pages[0][5].i = MAIN_BIG_PAGE_INDEX.i + 5;
    tables->mem_big_pages[0][6].i = MAIN_BIG_PAGE_INDEX.i + 6;
    tables->mem_big_pages[0][7].i = MAIN_BIG_PAGE_INDEX.i + 7;

    if (paging.acccon.bplus_bits.shadow) {
        tables->mem_big_pages[1][0].i = MAIN_BIG_PAGE_INDEX.i + 0;
        tables->mem_big_pages[1][1].i = MAIN_BIG_PAGE_INDEX.i + 1;
        tables->mem_big_pages[1][2].i = MAIN_BIG_PAGE_INDEX.i + 2;
        tables->mem_big_pages[1][3].i = SHADOW_BIG_PAGE_INDEX.i + 0;
        tables->mem_big_pages[1][4].i = SHADOW_BIG_PAGE_INDEX.i + 1;
        tables->mem_big_pages[1][5].i = SHADOW_BIG_PAGE_INDEX.i + 2;
        tables->mem_big_pages[1][6].i = SHADOW_BIG_PAGE_INDEX.i + 3;
        tables->mem_big_pages[1][7].i = SHADOW_BIG_PAGE_INDEX.i + 4;
    } else {
        tables->mem_big_pages[1][0].i = MAIN_BIG_PAGE_INDEX.i + 0;
        tables->mem_big_pages[1][1].i = MAIN_BIG_PAGE_INDEX.i + 1;
        tables->mem_big_pages[1][2].i = MAIN_BIG_PAGE_INDEX.i + 2;
        tables->mem_big_pages[1][3].i = MAIN_BIG_PAGE_INDEX.i + 3;
        tables->mem_big_pages[1][4].i = MAIN_BIG_PAGE_INDEX.i + 4;
        tables->mem_big_pages[1][5].i = MAIN_BIG_PAGE_INDEX.i + 5;
        tables->mem_big_pages[1][6].i = MAIN_BIG_PAGE_INDEX.i + 6;
        tables->mem_big_pages[1][7].i = MAIN_BIG_PAGE_INDEX.i + 7;
    }

    BigPageIndex::Type rom = ROM0_BIG_PAGE_INDEX.i + paging.romsel.bplus_bits.pr * NUM_ROM_BIG_PAGES + paging.rom_regions[paging.romsel.b_bits.pr] * 4;
    if (paging.romsel.bplus_bits.ram) {
        tables->mem_big_pages[0][0x8].i = ANDY_BIG_PAGE_INDEX.i + 0;
        tables->mem_big_pages[0][0x9].i = ANDY_BIG_PAGE_INDEX.i + 1;
        tables->mem_big_pages[0][0xa].i = ANDY_BIG_PAGE_INDEX.i + 2;
        tables->mem_big_pages[0][0xb].i = rom + 3;

        memset(tables->pc_mem_big_pages_set, 0, sizeof tables->pc_mem_big_pages_set);
        tables->pc_mem_big_pages_set[0xa] = 1;
        tables->pc_mem_big_pages_set[0xc] = 1;
        tables->pc_mem_big_pages_set[0xd] = 1;
    } else {
        tables->mem_big_pages[0][0x8].i = rom + 0;
        tables->mem_big_pages[0][0x9].i = rom + 1;
        tables->mem_big_pages[0][0xa].i = rom + 2;
        tables->mem_big_pages[0][0xb].i = rom + 3;

        memset(tables->pc_mem_big_pages_set, 0, sizeof tables->pc_mem_big_pages_set);
        tables->pc_mem_big_pages_set[0xc] = 1;
        tables->pc_mem_big_pages_set[0xd] = 1;
    }

    tables->mem_big_pages[0][0xc].i = MOS_BIG_PAGE_INDEX.i + 0;
    tables->mem_big_pages[0][0xd].i = MOS_BIG_PAGE_INDEX.i + 1;
    tables->mem_big_pages[0][0xe].i = MOS_BIG_PAGE_INDEX.i + 2;
    tables->mem_big_pages[0][0xf].i = MOS_BIG_PAGE_INDEX.i + 3;

    memcpy(&tables->mem_big_pages[1][8], &tables->mem_big_pages[0][8], 8 * sizeof tables->mem_big_pages[0][0]);

    *paging_flags = paging.acccon.bplus_bits.shadow ? PagingFlags_DisplayShadow : 0;
}

#if BBCMICRO_DEBUGGER
static void ApplyDSOBPlus(PagingState *paging, uint32_t dso) {
    ApplyROMDSO(paging, dso);

    if (dso & BBCMicroDebugStateOverride_OverrideANDY) {
        paging->romsel.bplus_bits.ram = !!(dso & BBCMicroDebugStateOverride_ANDY);
    }

    if (dso & BBCMicroDebugStateOverride_OverrideShadow) {
        paging->acccon.bplus_bits.shadow = !!(dso & BBCMicroDebugStateOverride_Shadow);
    }
}
#endif

#if BBCMICRO_DEBUGGER
static uint32_t GetDSOBPlus(const PagingState &paging) {
    uint32_t dso = 0;

    dso |= GetROMDSO(paging);

    if (paging.romsel.bplus_bits.ram) {
        dso |= BBCMicroDebugStateOverride_ANDY;
    }
    dso |= BBCMicroDebugStateOverride_OverrideANDY;

    if (paging.acccon.bplus_bits.shadow) {
        dso |= BBCMicroDebugStateOverride_Shadow;
    }
    dso |= BBCMicroDebugStateOverride_OverrideShadow;

    return dso;
}
#endif

static std::vector<BigPageMetadata> GetBigPagesMetadataBPlus(const ROMType *rom_types) {
    std::vector<BigPageMetadata> big_pages = GetBigPagesMetadataCommon(rom_types);

    InitBigPagesMetadata(&big_pages,
                         ANDY_BIG_PAGE_INDEX,
                         NUM_ANDY_BIG_PAGES + NUM_HAZEL_BIG_PAGES,
                         'n', 0, "ANDY",
#if BBCMICRO_DEBUGGER
                         0,
                         BBCMicroDebugStateOverride_OverrideANDY | BBCMicroDebugStateOverride_ANDY,
#endif
                         0x8000);

    InitBigPagesMetadata(&big_pages,
                         SHADOW_BIG_PAGE_INDEX,
                         NUM_SHADOW_BIG_PAGES,
                         's', 0, "Shadow RAM",
#if BBCMICRO_DEBUGGER
                         0,
                         BBCMicroDebugStateOverride_OverrideShadow | BBCMicroDebugStateOverride_Shadow,
#endif
                         0x3000);

    return big_pages;
}

#if BBCMICRO_DEBUGGER
static bool ParseSuffixCharBPlus(uint32_t *dso, char c) {
    if (ParseROMSuffixChar(dso, c)) {
        // ...
    } else if (c == 's') {
        *dso |= BBCMicroDebugStateOverride_OverrideShadow | BBCMicroDebugStateOverride_Shadow;
    } else if (c == 'm') {
        *dso |= BBCMicroDebugStateOverride_OverrideShadow;
        *dso &= ~BBCMicroDebugStateOverride_Shadow;
    } else if (c == 'n') {
        *dso |= BBCMicroDebugStateOverride_OverrideANDY | BBCMicroDebugStateOverride_ANDY;
        //} else if (c == 'i' || c == 'I' || c == 'o' || c == 'O') {
        //    // Valid, but no effect. These are supported on the basis that if you
        //    // can see them in the UI, you ought to be able to type them in...
    } else {
        return false;
    }

    return true;
}
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static void GetMemBigPagesTablesMaster(MemoryBigPageTables *tables,
                                       uint32_t *paging_flags,
                                       const PagingState &paging) {
    // YXE  Usr  MOS
    // ---  ---  ---
    // 000   M    M
    // 001   M    S
    // 010   S    M
    // 011   S    S
    // 100   M    M
    // 101   M    M
    // 110   S    S
    // 111   S    S
    //
    // Usr Shadow = X
    //
    // MOS Shadow = (Y AND X) OR (NOT Y AND E)

    if (paging.acccon.m128_bits.x) {
        tables->mem_big_pages[0][0].i = MAIN_BIG_PAGE_INDEX.i + 0;
        tables->mem_big_pages[0][1].i = MAIN_BIG_PAGE_INDEX.i + 1;
        tables->mem_big_pages[0][2].i = MAIN_BIG_PAGE_INDEX.i + 2;
        tables->mem_big_pages[0][3].i = SHADOW_BIG_PAGE_INDEX.i + 0;
        tables->mem_big_pages[0][4].i = SHADOW_BIG_PAGE_INDEX.i + 1;
        tables->mem_big_pages[0][5].i = SHADOW_BIG_PAGE_INDEX.i + 2;
        tables->mem_big_pages[0][6].i = SHADOW_BIG_PAGE_INDEX.i + 3;
        tables->mem_big_pages[0][7].i = SHADOW_BIG_PAGE_INDEX.i + 4;
    } else {
        tables->mem_big_pages[0][0].i = MAIN_BIG_PAGE_INDEX.i + 0;
        tables->mem_big_pages[0][1].i = MAIN_BIG_PAGE_INDEX.i + 1;
        tables->mem_big_pages[0][2].i = MAIN_BIG_PAGE_INDEX.i + 2;
        tables->mem_big_pages[0][3].i = MAIN_BIG_PAGE_INDEX.i + 3;
        tables->mem_big_pages[0][4].i = MAIN_BIG_PAGE_INDEX.i + 4;
        tables->mem_big_pages[0][5].i = MAIN_BIG_PAGE_INDEX.i + 5;
        tables->mem_big_pages[0][6].i = MAIN_BIG_PAGE_INDEX.i + 6;
        tables->mem_big_pages[0][7].i = MAIN_BIG_PAGE_INDEX.i + 7;
    }

    if ((paging.acccon.m128_bits.y && paging.acccon.m128_bits.x) ||
        (!paging.acccon.m128_bits.y && paging.acccon.m128_bits.e)) {
        tables->mem_big_pages[1][0].i = MAIN_BIG_PAGE_INDEX.i + 0;
        tables->mem_big_pages[1][1].i = MAIN_BIG_PAGE_INDEX.i + 1;
        tables->mem_big_pages[1][2].i = MAIN_BIG_PAGE_INDEX.i + 2;
        tables->mem_big_pages[1][3].i = SHADOW_BIG_PAGE_INDEX.i + 0;
        tables->mem_big_pages[1][4].i = SHADOW_BIG_PAGE_INDEX.i + 1;
        tables->mem_big_pages[1][5].i = SHADOW_BIG_PAGE_INDEX.i + 2;
        tables->mem_big_pages[1][6].i = SHADOW_BIG_PAGE_INDEX.i + 3;
        tables->mem_big_pages[1][7].i = SHADOW_BIG_PAGE_INDEX.i + 4;
    } else {
        tables->mem_big_pages[1][0].i = MAIN_BIG_PAGE_INDEX.i + 0;
        tables->mem_big_pages[1][1].i = MAIN_BIG_PAGE_INDEX.i + 1;
        tables->mem_big_pages[1][2].i = MAIN_BIG_PAGE_INDEX.i + 2;
        tables->mem_big_pages[1][3].i = MAIN_BIG_PAGE_INDEX.i + 3;
        tables->mem_big_pages[1][4].i = MAIN_BIG_PAGE_INDEX.i + 4;
        tables->mem_big_pages[1][5].i = MAIN_BIG_PAGE_INDEX.i + 5;
        tables->mem_big_pages[1][6].i = MAIN_BIG_PAGE_INDEX.i + 6;
        tables->mem_big_pages[1][7].i = MAIN_BIG_PAGE_INDEX.i + 7;
    }

    BigPageIndex::Type rom = ROM0_BIG_PAGE_INDEX.i + paging.romsel.bplus_bits.pr * NUM_ROM_BIG_PAGES + paging.rom_regions[paging.romsel.b_bits.pr] * 4;
    if (paging.romsel.m128_bits.ram) {
        tables->mem_big_pages[0][0x8].i = ANDY_BIG_PAGE_INDEX.i + 0;
        tables->mem_big_pages[0][0x9].i = rom + 1;
        tables->mem_big_pages[0][0xa].i = rom + 2;
        tables->mem_big_pages[0][0xb].i = rom + 3;
    } else {
        tables->mem_big_pages[0][0x8].i = rom + 0;
        tables->mem_big_pages[0][0x9].i = rom + 1;
        tables->mem_big_pages[0][0xa].i = rom + 2;
        tables->mem_big_pages[0][0xb].i = rom + 3;
    }

    if (paging.acccon.m128_bits.y) {
        tables->mem_big_pages[0][0xc].i = HAZEL_BIG_PAGE_INDEX.i + 0;
        tables->mem_big_pages[0][0xd].i = HAZEL_BIG_PAGE_INDEX.i + 1;
        tables->mem_big_pages[0][0xe].i = MOS_BIG_PAGE_INDEX.i + 2;
        tables->mem_big_pages[0][0xf].i = MOS_BIG_PAGE_INDEX.i + 3;

        memset(tables->pc_mem_big_pages_set, 0, sizeof tables->pc_mem_big_pages_set);
    } else {
        tables->mem_big_pages[0][0xc].i = MOS_BIG_PAGE_INDEX.i + 0;
        tables->mem_big_pages[0][0xd].i = MOS_BIG_PAGE_INDEX.i + 1;
        tables->mem_big_pages[0][0xe].i = MOS_BIG_PAGE_INDEX.i + 2;
        tables->mem_big_pages[0][0xf].i = MOS_BIG_PAGE_INDEX.i + 3;

        memset(tables->pc_mem_big_pages_set, 0, sizeof tables->pc_mem_big_pages_set);
        tables->pc_mem_big_pages_set[0xc] = 1;
        tables->pc_mem_big_pages_set[0xd] = 1;
    }

    memcpy(&tables->mem_big_pages[1][8], &tables->mem_big_pages[0][8], 8 * sizeof tables->mem_big_pages[0][0]);

    *paging_flags = ((paging.acccon.m128_bits.tst ? PagingFlags_ROMIO : 0) |
                     (paging.acccon.m128_bits.d ? PagingFlags_DisplayShadow : 0) |
                     (paging.acccon.m128_bits.ifj ? PagingFlags_IFJ : 0));
}

#if BBCMICRO_DEBUGGER
static void ApplyDSOMaster(PagingState *paging, uint32_t dso) {
    ApplyROMDSO(paging, dso);

    if (dso & BBCMicroDebugStateOverride_OverrideANDY) {
        paging->romsel.m128_bits.ram = !!(dso & BBCMicroDebugStateOverride_ANDY);
    }

    if (dso & BBCMicroDebugStateOverride_OverrideHAZEL) {
        paging->acccon.m128_bits.y = !!(dso & BBCMicroDebugStateOverride_HAZEL);
    }

    if (dso & BBCMicroDebugStateOverride_OverrideShadow) {
        paging->acccon.m128_bits.x = !!(dso & BBCMicroDebugStateOverride_Shadow);
    }

    if (dso & BBCMicroDebugStateOverride_OverrideOS) {
        paging->acccon.m128_bits.tst = !!(dso & BBCMicroDebugStateOverride_OS);
    }
}
#endif

#if BBCMICRO_DEBUGGER
static uint32_t GetDSOMaster(const PagingState &paging) {
    uint32_t dso = 0;

    dso |= GetROMDSO(paging);

    if (paging.romsel.m128_bits.ram) {
        dso |= BBCMicroDebugStateOverride_ANDY;
    }
    dso |= BBCMicroDebugStateOverride_OverrideANDY;

    if (paging.acccon.m128_bits.x) {
        dso |= BBCMicroDebugStateOverride_Shadow;
    }
    dso |= BBCMicroDebugStateOverride_OverrideShadow;

    if (paging.acccon.m128_bits.y) {
        dso |= BBCMicroDebugStateOverride_HAZEL;
    }
    dso |= BBCMicroDebugStateOverride_OverrideHAZEL;

    if (paging.acccon.m128_bits.tst) {
        dso |= BBCMicroDebugStateOverride_OS;
    }
    dso |= BBCMicroDebugStateOverride_OverrideOS;

    return dso;
}
#endif

static std::vector<BigPageMetadata> GetBigPagesMetadataMaster(const ROMType *rom_types) {
    std::vector<BigPageMetadata> big_pages = GetBigPagesMetadataCommon(rom_types);

    InitBigPagesMetadata(&big_pages,
                         ANDY_BIG_PAGE_INDEX,
                         NUM_ANDY_BIG_PAGES,
                         'n', 0, "ANDY",
#if BBCMICRO_DEBUGGER
                         0,
                         BBCMicroDebugStateOverride_OverrideANDY | BBCMicroDebugStateOverride_ANDY,
#endif
                         0x8000);

    InitBigPagesMetadata(&big_pages,
                         HAZEL_BIG_PAGE_INDEX,
                         NUM_HAZEL_BIG_PAGES,
                         'h', 0, "HAZEL",
#if BBCMICRO_DEBUGGER
                         0,
                         BBCMicroDebugStateOverride_OverrideHAZEL | BBCMicroDebugStateOverride_HAZEL,
#endif
                         0xc000);

    InitBigPagesMetadata(&big_pages,
                         SHADOW_BIG_PAGE_INDEX,
                         NUM_SHADOW_BIG_PAGES,
                         's', 0, "Shadow RAM",
#if BBCMICRO_DEBUGGER
                         0,
                         BBCMicroDebugStateOverride_OverrideShadow | BBCMicroDebugStateOverride_Shadow,
#endif
                         0x3000);

#if BBCMICRO_DEBUGGER
    // Update the MOS DSO flags.

    // Switch HAZEL off to see the first 8K of MOS.
    for (size_t i = 0; i < 2; ++i) {
        big_pages[MOS_BIG_PAGE_INDEX.i + i].dso_mask &= ~BBCMicroDebugStateOverride_HAZEL;
        big_pages[MOS_BIG_PAGE_INDEX.i + i].dso_value |= BBCMicroDebugStateOverride_OverrideHAZEL;
    }

    // Switch IO off to see all of the last 4K of MOS.
    //
    // Might as well, since the hardware lets you...
    big_pages[MOS_BIG_PAGE_INDEX.i + 3].dso_value |= BBCMicroDebugStateOverride_OverrideOS | BBCMicroDebugStateOverride_OS;
#endif

    return big_pages;
}

#if BBCMICRO_DEBUGGER
static bool ParseSuffixCharMaster(uint32_t *dso, char c) {
    if (ParseROMSuffixChar(dso, c)) {
        // ...
    } else if (c == 's') {
        *dso |= BBCMicroDebugStateOverride_OverrideShadow | BBCMicroDebugStateOverride_Shadow;
    } else if (c == 'm') {
        *dso |= BBCMicroDebugStateOverride_OverrideShadow;
        *dso &= ~BBCMicroDebugStateOverride_Shadow;
    } else if (c == 'h') {
        *dso |= BBCMicroDebugStateOverride_OverrideHAZEL | BBCMicroDebugStateOverride_HAZEL;
    } else if (c == 'n') {
        *dso |= BBCMicroDebugStateOverride_OverrideANDY | BBCMicroDebugStateOverride_ANDY;
    } else if (c == 'o') {
        *dso |= BBCMicroDebugStateOverride_OverrideHAZEL | BBCMicroDebugStateOverride_OverrideOS | BBCMicroDebugStateOverride_OS;
        *dso &= ~BBCMicroDebugStateOverride_HAZEL;
    } else if (c == 'i') {
        *dso |= BBCMicroDebugStateOverride_OverrideOS;
        *dso &= ~BBCMicroDebugStateOverride_OS;
    } else {
        return false;
    }

    return true;
}
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

std::shared_ptr<const BBCMicroType> CreateBBCMicroType(BBCMicroTypeID type_id, const ROMType *rom_types) {
    auto type = std::make_shared<BBCMicroType>();

    type->type_id = type_id;

    if (IsMaster(type->type_id)) {
        type->m6502_config = &M6502_cmos6502_config;
    } else {
        type->m6502_config = &M6502_nmos6502_config;
    }

    if (type->type_id == BBCMicroTypeID_B) {
        type->ram_buffer_size = 32768;
    } else {
        type->ram_buffer_size = 65536;
    }

    if (type->type_id == BBCMicroTypeID_MasterCompact) {
        type->default_disc_drive_type = DiscDriveType_90mm;
    } else {
        type->default_disc_drive_type = DiscDriveType_133mm;
    }

    switch (type->type_id) {
    default:
        ASSERT(false);
        [[fallthrough]];
    case BBCMicroTypeID_B:
        type->big_pages_metadata = GetBigPagesMetadataB(rom_types);
        type->get_mem_big_page_tables_fn = &GetMemBigPageTablesB;
        break;

    case BBCMicroTypeID_BPlus:
        type->big_pages_metadata = GetBigPagesMetadataBPlus(rom_types);
        type->get_mem_big_page_tables_fn = &GetMemBigPageTablesBPlus;
        break;

    case BBCMicroTypeID_Master:
    case BBCMicroTypeID_MasterCompact:
        type->big_pages_metadata = GetBigPagesMetadataMaster(rom_types);
        type->get_mem_big_page_tables_fn = &GetMemBigPagesTablesMaster;
        break;
    }

#if BBCMICRO_DEBUGGER
    // Assign a debug flags index to each big page.
    {
        ASSERT(type->big_pages_metadata.size() == NUM_BIG_PAGES);

        // map (bank,addr,offset) of a ROM big page to the first BigPageIndex that
        // refers to that.
        std::map<std::tuple<uint32_t, uint16_t, size_t>, BigPageIndex::Type> seen_rom_big_pages;
        for (BigPageIndex::Type i = 0; i < NUM_BIG_PAGES; ++i) {
            BigPageMetadata *bp = &type->big_pages_metadata[i];
            ASSERT(bp->debug_flags_index.i == INVALID_BIG_PAGE_INDEX.i);

            if (i >= ROM0_BIG_PAGE_INDEX.i && i < ROM0_BIG_PAGE_INDEX.i + 16 * NUM_ROM_BIG_PAGES) {
                // A sideways bank big page. This may alias a previously seen one.
                ASSERT(i != 0); //0 is not allowed to be a sideways big page.

                uint32_t bank = (uint32_t)((i - ROM0_BIG_PAGE_INDEX.i) / NUM_ROM_BIG_PAGES);
                uint32_t region = (uint32_t)((i - ROM0_BIG_PAGE_INDEX.i) % NUM_ROM_BIG_PAGES / 4);
                uint32_t relative_big_page_index = (uint32_t)((i - ROM0_BIG_PAGE_INDEX.i) % NUM_ROM_BIG_PAGES % 4);
                BigPageIndex::Type *index = &seen_rom_big_pages[{bank, bp->addr, GetROMOffset(rom_types[bank], relative_big_page_index, region)}];
                if (*index == 0) {
                    // Not seen this one before, so here it is.
                    *index = i;
                }

                bp->debug_flags_index.i = *index;
            } else {
                // A non-sideways big page. These always have their own debug
                // flags.
                bp->debug_flags_index.i = i;
            }
        }
    }
#endif

#if BBCMICRO_DEBUGGER
    switch (type->type_id) {
    default:
        ASSERT(false);
        [[fallthrough]];
    case BBCMicroTypeID_B:
        type->dso_mask = (BBCMicroDebugStateOverride_OverrideROM |
                          BBCMicroDebugStateOverride_ROM);
        type->apply_dso_fn = &ApplyDSOB;
        type->get_dso_fn = &GetDSOB;
        type->parse_suffix_char_fn = &ParseSuffixCharB;
        break;

    case BBCMicroTypeID_BPlus:
        type->dso_mask = (BBCMicroDebugStateOverride_ROM |
                          BBCMicroDebugStateOverride_OverrideROM |
                          BBCMicroDebugStateOverride_ANDY |
                          BBCMicroDebugStateOverride_OverrideANDY |
                          BBCMicroDebugStateOverride_Shadow |
                          BBCMicroDebugStateOverride_OverrideShadow);
        type->apply_dso_fn = &ApplyDSOBPlus;
        type->get_dso_fn = &GetDSOBPlus;
        type->parse_suffix_char_fn = &ParseSuffixCharBPlus;
        break;

    case BBCMicroTypeID_Master:
    case BBCMicroTypeID_MasterCompact:
        type->dso_mask = (BBCMicroDebugStateOverride_ROM |
                          BBCMicroDebugStateOverride_OverrideROM |
                          BBCMicroDebugStateOverride_ANDY |
                          BBCMicroDebugStateOverride_OverrideANDY |
                          BBCMicroDebugStateOverride_HAZEL |
                          BBCMicroDebugStateOverride_OverrideHAZEL |
                          BBCMicroDebugStateOverride_Shadow |
                          BBCMicroDebugStateOverride_OverrideShadow |
                          BBCMicroDebugStateOverride_OS |
                          BBCMicroDebugStateOverride_OverrideOS);
        type->apply_dso_fn = &ApplyDSOMaster;
        type->get_dso_fn = &GetDSOMaster;
        type->parse_suffix_char_fn = &ParseSuffixCharMaster;
        break;
    }

    for (uint8_t i = 0; i < 16; ++i) {
        if (rom_types[i] != ROMType_16KB) {
            type->dso_mask |= (BBCMicroDebugStateOverride_OverrideMapperRegion |
                               BBCMicroDebugStateOverride_MapperRegionMask << BBCMicroDebugStateOverride_MapperRegionShift);
            break;
        }
    }
#endif

    switch (type->type_id) {
    default:
        ASSERT(false);
        [[fallthrough]];
    case BBCMicroTypeID_B:
        type->romsel_mask = 0x0f;
        type->acccon_mask = 0x00;
        break;

    case BBCMicroTypeID_BPlus:
        type->romsel_mask = 0x8f;
        type->acccon_mask = 0x80;
        break;

    case BBCMicroTypeID_Master:
    case BBCMicroTypeID_MasterCompact:
        type->romsel_mask = 0x8f;
        type->acccon_mask = 0xff;
        break;
    }

    if (IsMaster(type->type_id)) {
        type->sheila_cycle_stretch_regions = {
            {0x00, 0x1f},
            {0x40, 0x7f},
            {0xc0, 0xdf},
        };
    } else {
        type->sheila_cycle_stretch_regions = {
            {0x00, 0x1f},
            {0x40, 0x7f},
            {0xc0, 0xdf},
        };
    }

    if (HasADC(type->type_id)) {
        if (IsMaster(type->type_id)) {
            type->adc_addr = 0xfe18;
            type->adc_count = 8;
        } else {
            type->adc_addr = 0xfec0;
            type->adc_count = 32;
        }
    }

    return type;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool HasNVRAM(BBCMicroTypeID type_id) {
    return IsMaster(type_id);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool CanDisplayTeletextAt3C00(BBCMicroTypeID type_id) {
    return IsB(type_id);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool HasNumericKeypad(BBCMicroTypeID type_id) {
    return IsMaster(type_id);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool HasSpeech(BBCMicroTypeID type_id) {
    return IsB(type_id);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool HasTube(BBCMicroTypeID type_id) {
    return type_id != BBCMicroTypeID_MasterCompact;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool HasCartridges(BBCMicroTypeID type_id) {
    return type_id == BBCMicroTypeID_Master;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool HasUserPort(BBCMicroTypeID type_id) {
    return type_id != BBCMicroTypeID_MasterCompact;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool Has1MHzBus(BBCMicroTypeID type_id) {
    return type_id != BBCMicroTypeID_MasterCompact;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool HasADC(BBCMicroTypeID type_id) {
    return type_id != BBCMicroTypeID_MasterCompact;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool HasIndependentMOSView(BBCMicroTypeID type_id) {
    return type_id != BBCMicroTypeID_B;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

const char *GetModelName(BBCMicroTypeID type_id) {
    switch (type_id) {
    default:
        ASSERT(false);
        [[fallthrough]];
    case BBCMicroTypeID_B:
        return "B";

    case BBCMicroTypeID_BPlus:
        return "B+";

    case BBCMicroTypeID_Master:
        return "Master 128";

    case BBCMicroTypeID_MasterCompact:
        return "Master Compact";
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if BBCMICRO_DEBUGGER
bool ParseAddressSuffix(uint32_t *dso_ptr,
                        const std::shared_ptr<const BBCMicroType> &type,
                        const char *suffix,
                        Log *log) {
    uint32_t dso = *dso_ptr;

    for (const char *suffix_char = suffix; *suffix_char != 0; ++suffix_char) {
        char c = *suffix_char;

        if (c == 'p') {
            dso |= BBCMicroDebugStateOverride_Parasite;
        } else if (c == 'r') {
            dso |= BBCMicroDebugStateOverride_OverrideParasiteROM | BBCMicroDebugStateOverride_ParasiteROM;
        } else if ((*type->parse_suffix_char_fn)(&dso, c)) {
            // Valid flag for this model.
        } else if (g_all_big_page_codes.find(c) != std::string::npos) {
            // Valid flag - but not for this model, so ignore.
        } else {
            if (log) {
                log->f("'%c': unknown address suffix", *suffix_char);
            }

            return false;
        }
    }

    *dso_ptr = dso;

    return true;
}
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
