#include <shared/system.h>
#include <shared/debug.h>
#include <shared/log.h>
#include <beeb/conf.h>
#include <beeb/type.h>
#include <6502/6502.h>
#include <string.h>

#include <shared/enum_def.h>
#include <beeb/type.inl>
#include <shared/enum_end.h>

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
}
#endif

static std::string g_all_lower_case_big_page_codes;

static void InitBigPagesMetadata(std::vector<BigPageMetadata> *big_pages,
                                 BigPageIndex index,
                                 size_t n,
                                 char code,
                                 const std::string &description,
#if BBCMICRO_DEBUGGER
                                 uint32_t dso_clear,
                                 uint32_t dso_set,
#endif
                                 uint16_t base) {
    code = (char)tolower(code);

    for (size_t i = 0; i < n; ++i) {
        ASSERT(index.i + i <= NUM_BIG_PAGES);
        BigPageMetadata *bp = &(*big_pages)[index.i + i];

        ASSERT(bp->index.i == INVALID_BIG_PAGE_INDEX.i);
        bp->index.i = (BigPageIndex::Type)(index.i + i);
        bp->code = code;
        bp->description = description;
#if BBCMICRO_DEBUGGER
        bp->dso_mask = ~dso_clear;
        bp->dso_value = dso_set;
#endif
        bp->addr = (uint16_t)(base + i * 4096);
    }

    if (g_all_lower_case_big_page_codes.find(code) == std::string::npos) {
        g_all_lower_case_big_page_codes.push_back(code);
    }
}

static std::vector<BigPageMetadata> GetBigPagesMetadataCommon() {
    std::vector<BigPageMetadata> big_pages;
    big_pages.resize(NUM_BIG_PAGES);

    InitBigPagesMetadata(&big_pages,
                         MAIN_BIG_PAGE_INDEX,
                         NUM_MAIN_BIG_PAGES,
                         'm', "Main RAM",
#if BBCMICRO_DEBUGGER
                         0,
                         0,
#endif
                         0x0000);

    for (uint8_t i = 0; i < 16; ++i) {
        char code[2];
        snprintf(code, sizeof code, "%x", i);

        char description[100];
        snprintf(description, sizeof description, "ROM %x", i);

        InitBigPagesMetadata(&big_pages,
                             {(BigPageIndex::Type)(ROM0_BIG_PAGE_INDEX.i + (size_t)i * NUM_ROM_BIG_PAGES)},
                             NUM_ROM_BIG_PAGES,
                             code[0], description,
#if BBCMICRO_DEBUGGER
                             (uint32_t)BBCMicroDebugStateOverride_ROM,
                             BBCMicroDebugStateOverride_OverrideROM | i,
#endif
                             0x8000);
    }

    InitBigPagesMetadata(&big_pages,
                         MOS_BIG_PAGE_INDEX,
                         NUM_MOS_BIG_PAGES,
                         'o', "MOS ROM",
#if BBCMICRO_DEBUGGER
                         0,
                         0,
#endif
                         0xc000);

    InitBigPagesMetadata(&big_pages,
                         PARASITE_BIG_PAGE_INDEX,
                         NUM_PARASITE_BIG_PAGES,
                         'p', "Parasite",
#if BBCMICRO_DEBUGGER
                         0,
                         0,
#endif
                         0x0000);

    InitBigPagesMetadata(&big_pages,
                         PARASITE_ROM_BIG_PAGE_INDEX,
                         NUM_PARASITE_ROM_BIG_PAGES,
                         'r', "Parasite ROM",
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
static bool HandleROMPrefixChar(uint32_t *dso, uint8_t rom) {
    ASSERT(rom >= 0 && rom <= 15);

    *dso &= ~(uint32_t)(BBCMicroDebugStateOverride_ROM | BBCMicroDebugStateOverride_ANDY);
    *dso |= BBCMicroDebugStateOverride_OverrideANDY | BBCMicroDebugStateOverride_OverrideROM | rom;

    return true;
}
#endif

#if BBCMICRO_DEBUGGER
// select ROM
static bool ParseROMPrefixLowerCaseChar(uint32_t *dso, char c) {
    if (c >= '0' && c <= '9') {
        return HandleROMPrefixChar(dso, (uint8_t)(c - '0'));
    } else if (c >= 'a' && c <= 'f') {
        return HandleROMPrefixChar(dso, (uint8_t)(c - 'a' + 10));
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

    BigPageIndex::Type rom = ROM0_BIG_PAGE_INDEX.i + paging.romsel.b_bits.pr * NUM_ROM_BIG_PAGES;
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

    dso |= paging.romsel.b_bits.pr;
    dso |= BBCMicroDebugStateOverride_OverrideROM;

    return dso;
}
#endif

static std::vector<BigPageMetadata> GetBigPagesMetadataB() {
    std::vector<BigPageMetadata> big_pages = GetBigPagesMetadataCommon();

    return big_pages;
}

#if BBCMICRO_DEBUGGER
static bool ParsePrefixLowerCaseCharB(uint32_t *dso, char c) {
    if (ParseROMPrefixLowerCaseChar(dso, c)) {
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

    BigPageIndex::Type rom = ROM0_BIG_PAGE_INDEX.i + paging.romsel.bplus_bits.pr * NUM_ROM_BIG_PAGES;
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

    dso |= paging.romsel.bplus_bits.pr;
    dso |= BBCMicroDebugStateOverride_OverrideROM;

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

static std::vector<BigPageMetadata> GetBigPagesMetadataBPlus() {
    std::vector<BigPageMetadata> big_pages = GetBigPagesMetadataCommon();

    InitBigPagesMetadata(&big_pages,
                         ANDY_BIG_PAGE_INDEX,
                         NUM_ANDY_BIG_PAGES + NUM_HAZEL_BIG_PAGES,
                         'n', "ANDY",
#if BBCMICRO_DEBUGGER
                         0,
                         BBCMicroDebugStateOverride_OverrideANDY | BBCMicroDebugStateOverride_ANDY,
#endif
                         0x8000);

    InitBigPagesMetadata(&big_pages,
                         SHADOW_BIG_PAGE_INDEX,
                         NUM_SHADOW_BIG_PAGES,
                         's', "Shadow RAM",
#if BBCMICRO_DEBUGGER
                         0,
                         BBCMicroDebugStateOverride_OverrideShadow | BBCMicroDebugStateOverride_Shadow,
#endif
                         0x3000);

    return big_pages;
}

#if BBCMICRO_DEBUGGER
static bool ParsePrefixLowerCaseCharBPlus(uint32_t *dso, char c) {
    if (ParseROMPrefixLowerCaseChar(dso, c)) {
        // ...
    } else if (c == 's') {
        *dso |= BBCMicroDebugStateOverride_OverrideShadow | BBCMicroDebugStateOverride_Shadow;
    } else if (c == 'm') {
        *dso |= BBCMicroDebugStateOverride_OverrideShadow;
        *dso &= ~(uint32_t)BBCMicroDebugStateOverride_Shadow;
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

    BigPageIndex::Type rom = ROM0_BIG_PAGE_INDEX.i + paging.romsel.bplus_bits.pr * NUM_ROM_BIG_PAGES;
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

    dso |= paging.romsel.m128_bits.pm;
    dso |= BBCMicroDebugStateOverride_OverrideROM;

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

static std::vector<BigPageMetadata> GetBigPagesMetadataMaster() {
    std::vector<BigPageMetadata> big_pages = GetBigPagesMetadataCommon();

    InitBigPagesMetadata(&big_pages,
                         ANDY_BIG_PAGE_INDEX,
                         NUM_ANDY_BIG_PAGES,
                         'n', "ANDY",
#if BBCMICRO_DEBUGGER
                         0,
                         BBCMicroDebugStateOverride_OverrideANDY | BBCMicroDebugStateOverride_ANDY,
#endif
                         0x8000);

    InitBigPagesMetadata(&big_pages,
                         HAZEL_BIG_PAGE_INDEX,
                         NUM_HAZEL_BIG_PAGES,
                         'h', "HAZEL",
#if BBCMICRO_DEBUGGER
                         0,
                         BBCMicroDebugStateOverride_OverrideHAZEL | BBCMicroDebugStateOverride_HAZEL,
#endif
                         0xc000);

    InitBigPagesMetadata(&big_pages,
                         SHADOW_BIG_PAGE_INDEX,
                         NUM_SHADOW_BIG_PAGES,
                         's', "Shadow RAM",
#if BBCMICRO_DEBUGGER
                         0,
                         BBCMicroDebugStateOverride_OverrideShadow | BBCMicroDebugStateOverride_Shadow,
#endif
                         0x3000);

#if BBCMICRO_DEBUGGER
    // Update the MOS DSO flags.

    // Switch HAZEL off to see the first 8K of MOS.
    for (size_t i = 0; i < 2; ++i) {
        big_pages[MOS_BIG_PAGE_INDEX.i + i].dso_mask &= ~(uint32_t)~BBCMicroDebugStateOverride_HAZEL;
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
static bool ParsePrefixLowerCaseCharMaster(uint32_t *dso, char c) {
    if (ParseROMPrefixLowerCaseChar(dso, c)) {
        // ...
    } else if (c == 's') {
        *dso |= BBCMicroDebugStateOverride_OverrideShadow | BBCMicroDebugStateOverride_Shadow;
    } else if (c == 'm') {
        *dso |= BBCMicroDebugStateOverride_OverrideShadow;
        *dso &= ~(uint32_t)BBCMicroDebugStateOverride_Shadow;
    } else if (c == 'h') {
        *dso |= BBCMicroDebugStateOverride_OverrideHAZEL | BBCMicroDebugStateOverride_HAZEL;
    } else if (c == 'n') {
        *dso |= BBCMicroDebugStateOverride_OverrideANDY | BBCMicroDebugStateOverride_ANDY;
    } else if (c == 'o') {
        *dso |= BBCMicroDebugStateOverride_OverrideHAZEL | BBCMicroDebugStateOverride_OverrideOS | BBCMicroDebugStateOverride_OS;
        *dso &= ~(uint32_t)BBCMicroDebugStateOverride_HAZEL;
    } else if (c == 'i') {
        *dso |= BBCMicroDebugStateOverride_OverrideOS;
        *dso &= ~(uint32_t)BBCMicroDebugStateOverride_OS;
    } else {
        return false;
    }

    return true;
}
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

std::shared_ptr<const BBCMicroType> CreateBBCMicroTypeForTypeID(BBCMicroTypeID type_id) {
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
        type->big_pages_metadata = GetBigPagesMetadataB();
        type->get_mem_big_page_tables_fn = &GetMemBigPageTablesB;
        break;

    case BBCMicroTypeID_BPlus:
        type->big_pages_metadata = GetBigPagesMetadataBPlus();
        type->get_mem_big_page_tables_fn = &GetMemBigPageTablesBPlus;
        break;

    case BBCMicroTypeID_Master:
    case BBCMicroTypeID_MasterCompact:
        type->big_pages_metadata = GetBigPagesMetadataMaster();
        type->get_mem_big_page_tables_fn = &GetMemBigPagesTablesMaster;
        break;
    }

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
        type->parse_prefix_lower_case_char_fn = &ParsePrefixLowerCaseCharB;
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
        type->parse_prefix_lower_case_char_fn = &ParsePrefixLowerCaseCharBPlus;
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
        type->parse_prefix_lower_case_char_fn = &ParsePrefixLowerCaseCharMaster;
        break;
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
bool ParseAddressPrefix(uint32_t *dso_ptr,
                        const std::shared_ptr<const BBCMicroType> &type,
                        const char *prefix_begin,
                        const char *prefix_end,
                        Log *log) {
    uint32_t dso = *dso_ptr;

    for (const char *prefix_char = prefix_begin;
         *prefix_char != 0 && prefix_char != prefix_end;
         ++prefix_char) {
        char c = (char)tolower(*prefix_char);

        if (c == 'p') {
            dso |= BBCMicroDebugStateOverride_Parasite;
        } else if (c == 'r') {
            dso |= BBCMicroDebugStateOverride_OverrideParasiteROM | BBCMicroDebugStateOverride_ParasiteROM;
        } else if ((*type->parse_prefix_lower_case_char_fn)(&dso, c)) {
            // Valid flag for this model.
        } else if (g_all_lower_case_big_page_codes.find(c) != std::string::npos) {
            // Valid flag - but not for this model, so ignore.
        } else {
            if (log) {
                log->f("'%c': unknown address prefix", *prefix_char);
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
