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

#if BBCMICRO_DEBUGGER
static void ApplyROMDSO(ROMSEL *romsel, uint32_t dso) {
    if (dso & BBCMicroDebugStateOverride_OverrideROM) {
        romsel->b_bits.pr = dso & BBCMicroDebugStateOverride_ROM;
    }
}
#endif

static std::string g_all_lower_case_big_page_codes;

static void InitBigPagesMetadata(std::vector<BigPageMetadata> *big_pages,
                                 size_t index,
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
        ASSERT(index + i <= NUM_BIG_PAGES);
        BigPageMetadata *bp = &(*big_pages)[index + i];

        ASSERT(bp->index == 0xff);
        bp->index = (uint8_t)(index + i);
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
                             ROM0_BIG_PAGE_INDEX + (size_t)i * NUM_ROM_BIG_PAGES,
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
        big_pages[PARASITE_BIG_PAGE_INDEX + i].is_parasite = true;
    }

    for (size_t i = 0; i < NUM_PARASITE_ROM_BIG_PAGES; ++i) {
        big_pages[PARASITE_ROM_BIG_PAGE_INDEX + i].is_parasite = true;
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
                                 ROMSEL romsel,
                                 ACCCON acccon) {
    (void)acccon; //not relevant for B

    tables->mem_big_pages[0][0] = MAIN_BIG_PAGE_INDEX + 0;
    tables->mem_big_pages[0][1] = MAIN_BIG_PAGE_INDEX + 1;
    tables->mem_big_pages[0][2] = MAIN_BIG_PAGE_INDEX + 2;
    tables->mem_big_pages[0][3] = MAIN_BIG_PAGE_INDEX + 3;
    tables->mem_big_pages[0][4] = MAIN_BIG_PAGE_INDEX + 4;
    tables->mem_big_pages[0][5] = MAIN_BIG_PAGE_INDEX + 5;
    tables->mem_big_pages[0][6] = MAIN_BIG_PAGE_INDEX + 6;
    tables->mem_big_pages[0][7] = MAIN_BIG_PAGE_INDEX + 7;

    uint8_t rom = ROM0_BIG_PAGE_INDEX + romsel.b_bits.pr * NUM_ROM_BIG_PAGES;
    tables->mem_big_pages[0][0x8] = rom + 0;
    tables->mem_big_pages[0][0x9] = rom + 1;
    tables->mem_big_pages[0][0xa] = rom + 2;
    tables->mem_big_pages[0][0xb] = rom + 3;

    tables->mem_big_pages[0][0xc] = MOS_BIG_PAGE_INDEX + 0;
    tables->mem_big_pages[0][0xd] = MOS_BIG_PAGE_INDEX + 1;
    tables->mem_big_pages[0][0xe] = MOS_BIG_PAGE_INDEX + 2;
    tables->mem_big_pages[0][0xf] = MOS_BIG_PAGE_INDEX + 3;

    memset(tables->mem_big_pages[1], 0, 16);
    memset(tables->pc_mem_big_pages_set, 0, 16);
    *paging_flags = 0;
}

#if BBCMICRO_DEBUGGER
static void ApplyDSOB(ROMSEL *romsel, ACCCON *acccon, uint32_t dso) {
    (void)acccon;

    ApplyROMDSO(romsel, dso);
}
#endif

#if BBCMICRO_DEBUGGER
static uint32_t GetDSOB(ROMSEL romsel, ACCCON acccon) {
    (void)acccon;

    uint32_t dso = 0;

    dso |= romsel.b_bits.pr;
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

const BBCMicroType BBC_MICRO_TYPE_B = {
    BBCMicroTypeID_B, //type_id
    "B",
    &M6502_nmos6502_config, //m6502_config
    32768,                  //ram_buffer_size
    DiscDriveType_133mm,    //default_disc_drive_type
#if BBCMICRO_DEBUGGER
    (BBCMicroDebugStateOverride_OverrideROM |
     BBCMicroDebugStateOverride_ROM), //dso_mask
#endif
    GetBigPagesMetadataB(),
    &GetMemBigPageTablesB, //get_mem_big_page_tables_fn,
#if BBCMICRO_DEBUGGER
    &ApplyDSOB, //apply_dso_fn
    &GetDSOB,   //get_dso_fn
#endif
    0x0f, //romsel_mask,
    0x00, //acccon_mask,
    {
        {0x00, 0x1f},
        {0x40, 0x7f},
        {0xc0, 0xdf},
    },      //sheila_cycle_stretch_regions
    0xfec0, //adc_addr
    32,     //adc_count
#if BBCMICRO_DEBUGGER
    &ParsePrefixLowerCaseCharB, //parse_prefix_char_fn
#endif
};

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
                                     ROMSEL romsel,
                                     ACCCON acccon) {
    tables->mem_big_pages[0][0] = MAIN_BIG_PAGE_INDEX + 0;
    tables->mem_big_pages[0][1] = MAIN_BIG_PAGE_INDEX + 1;
    tables->mem_big_pages[0][2] = MAIN_BIG_PAGE_INDEX + 2;
    tables->mem_big_pages[0][3] = MAIN_BIG_PAGE_INDEX + 3;
    tables->mem_big_pages[0][4] = MAIN_BIG_PAGE_INDEX + 4;
    tables->mem_big_pages[0][5] = MAIN_BIG_PAGE_INDEX + 5;
    tables->mem_big_pages[0][6] = MAIN_BIG_PAGE_INDEX + 6;
    tables->mem_big_pages[0][7] = MAIN_BIG_PAGE_INDEX + 7;

    if (acccon.bplus_bits.shadow) {
        tables->mem_big_pages[1][0] = MAIN_BIG_PAGE_INDEX + 0;
        tables->mem_big_pages[1][1] = MAIN_BIG_PAGE_INDEX + 1;
        tables->mem_big_pages[1][2] = MAIN_BIG_PAGE_INDEX + 2;
        tables->mem_big_pages[1][3] = SHADOW_BIG_PAGE_INDEX + 0;
        tables->mem_big_pages[1][4] = SHADOW_BIG_PAGE_INDEX + 1;
        tables->mem_big_pages[1][5] = SHADOW_BIG_PAGE_INDEX + 2;
        tables->mem_big_pages[1][6] = SHADOW_BIG_PAGE_INDEX + 3;
        tables->mem_big_pages[1][7] = SHADOW_BIG_PAGE_INDEX + 4;
    } else {
        tables->mem_big_pages[1][0] = MAIN_BIG_PAGE_INDEX + 0;
        tables->mem_big_pages[1][1] = MAIN_BIG_PAGE_INDEX + 1;
        tables->mem_big_pages[1][2] = MAIN_BIG_PAGE_INDEX + 2;
        tables->mem_big_pages[1][3] = MAIN_BIG_PAGE_INDEX + 3;
        tables->mem_big_pages[1][4] = MAIN_BIG_PAGE_INDEX + 4;
        tables->mem_big_pages[1][5] = MAIN_BIG_PAGE_INDEX + 5;
        tables->mem_big_pages[1][6] = MAIN_BIG_PAGE_INDEX + 6;
        tables->mem_big_pages[1][7] = MAIN_BIG_PAGE_INDEX + 7;
    }

    uint8_t rom = ROM0_BIG_PAGE_INDEX + romsel.bplus_bits.pr * NUM_ROM_BIG_PAGES;
    if (romsel.bplus_bits.ram) {
        tables->mem_big_pages[0][0x8] = ANDY_BIG_PAGE_INDEX + 0;
        tables->mem_big_pages[0][0x9] = ANDY_BIG_PAGE_INDEX + 1;
        tables->mem_big_pages[0][0xa] = ANDY_BIG_PAGE_INDEX + 2;
        tables->mem_big_pages[0][0xb] = rom + 3;

        memset(tables->pc_mem_big_pages_set, 0, 16);
        tables->pc_mem_big_pages_set[0xa] = 1;
        tables->pc_mem_big_pages_set[0xc] = 1;
        tables->pc_mem_big_pages_set[0xd] = 1;
    } else {
        tables->mem_big_pages[0][0x8] = rom + 0;
        tables->mem_big_pages[0][0x9] = rom + 1;
        tables->mem_big_pages[0][0xa] = rom + 2;
        tables->mem_big_pages[0][0xb] = rom + 3;

        memset(tables->pc_mem_big_pages_set, 0, 16);
        tables->pc_mem_big_pages_set[0xc] = 1;
        tables->pc_mem_big_pages_set[0xd] = 1;
    }

    tables->mem_big_pages[0][0xc] = MOS_BIG_PAGE_INDEX + 0;
    tables->mem_big_pages[0][0xd] = MOS_BIG_PAGE_INDEX + 1;
    tables->mem_big_pages[0][0xe] = MOS_BIG_PAGE_INDEX + 2;
    tables->mem_big_pages[0][0xf] = MOS_BIG_PAGE_INDEX + 3;

    memcpy(&tables->mem_big_pages[1][8], &tables->mem_big_pages[0][8], 8);

    *paging_flags = acccon.bplus_bits.shadow ? PagingFlags_DisplayShadow : 0;
}

#if BBCMICRO_DEBUGGER
static void ApplyDSOBPlus(ROMSEL *romsel, ACCCON *acccon, uint32_t dso) {
    ApplyROMDSO(romsel, dso);

    if (dso & BBCMicroDebugStateOverride_OverrideANDY) {
        romsel->bplus_bits.ram = !!(dso & BBCMicroDebugStateOverride_ANDY);
    }

    if (dso & BBCMicroDebugStateOverride_OverrideShadow) {
        acccon->bplus_bits.shadow = !!(dso & BBCMicroDebugStateOverride_Shadow);
    }
}
#endif

#if BBCMICRO_DEBUGGER
static uint32_t GetDSOBPlus(ROMSEL romsel, ACCCON acccon) {
    uint32_t dso = 0;

    dso |= romsel.bplus_bits.pr;
    dso |= BBCMicroDebugStateOverride_OverrideROM;

    if (romsel.bplus_bits.ram) {
        dso |= BBCMicroDebugStateOverride_ANDY;
    }
    dso |= BBCMicroDebugStateOverride_OverrideANDY;

    if (acccon.bplus_bits.shadow) {
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

const BBCMicroType BBC_MICRO_TYPE_B_PLUS = {
    BBCMicroTypeID_BPlus, //type_id
    "B+",
    &M6502_nmos6502_config, //m6502_config
    65536,                  //ram_buffer_size
    DiscDriveType_133mm,    //default_disc_drive_type
#if BBCMICRO_DEBUGGER
    (BBCMicroDebugStateOverride_ROM |
     BBCMicroDebugStateOverride_OverrideROM |
     BBCMicroDebugStateOverride_ANDY |
     BBCMicroDebugStateOverride_OverrideANDY |
     BBCMicroDebugStateOverride_Shadow |
     BBCMicroDebugStateOverride_OverrideShadow), //dso_mask
#endif
    GetBigPagesMetadataBPlus(),
    &GetMemBigPageTablesBPlus, //get_mem_big_page_tables_fn,
#if BBCMICRO_DEBUGGER
    &ApplyDSOBPlus, //apply_dso_fn
    &GetDSOBPlus,   //get_dso_fn
#endif
    0x8f, //romsel_mask,
    0x80, //acccon_mask,
    {
        {0x00, 0x1f},
        {0x40, 0x7f},
        {0xc0, 0xdf},
    },      //sheila_cycle_stretch_regions
    0xfec0, //adc_addr
    32,     //adc_count
#if BBCMICRO_DEBUGGER
    &ParsePrefixLowerCaseCharBPlus, //parse_prefix_char_fn
#endif
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static void GetMemBigPagesTablesMaster(MemoryBigPageTables *tables,
                                       uint32_t *paging_flags,
                                       ROMSEL romsel,
                                       ACCCON acccon) {
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

    if (acccon.m128_bits.x) {
        tables->mem_big_pages[0][0] = MAIN_BIG_PAGE_INDEX + 0;
        tables->mem_big_pages[0][1] = MAIN_BIG_PAGE_INDEX + 1;
        tables->mem_big_pages[0][2] = MAIN_BIG_PAGE_INDEX + 2;
        tables->mem_big_pages[0][3] = SHADOW_BIG_PAGE_INDEX + 0;
        tables->mem_big_pages[0][4] = SHADOW_BIG_PAGE_INDEX + 1;
        tables->mem_big_pages[0][5] = SHADOW_BIG_PAGE_INDEX + 2;
        tables->mem_big_pages[0][6] = SHADOW_BIG_PAGE_INDEX + 3;
        tables->mem_big_pages[0][7] = SHADOW_BIG_PAGE_INDEX + 4;
    } else {
        tables->mem_big_pages[0][0] = MAIN_BIG_PAGE_INDEX + 0;
        tables->mem_big_pages[0][1] = MAIN_BIG_PAGE_INDEX + 1;
        tables->mem_big_pages[0][2] = MAIN_BIG_PAGE_INDEX + 2;
        tables->mem_big_pages[0][3] = MAIN_BIG_PAGE_INDEX + 3;
        tables->mem_big_pages[0][4] = MAIN_BIG_PAGE_INDEX + 4;
        tables->mem_big_pages[0][5] = MAIN_BIG_PAGE_INDEX + 5;
        tables->mem_big_pages[0][6] = MAIN_BIG_PAGE_INDEX + 6;
        tables->mem_big_pages[0][7] = MAIN_BIG_PAGE_INDEX + 7;
    }

    if ((acccon.m128_bits.y && acccon.m128_bits.x) ||
        (!acccon.m128_bits.y && acccon.m128_bits.e)) {
        tables->mem_big_pages[1][0] = MAIN_BIG_PAGE_INDEX + 0;
        tables->mem_big_pages[1][1] = MAIN_BIG_PAGE_INDEX + 1;
        tables->mem_big_pages[1][2] = MAIN_BIG_PAGE_INDEX + 2;
        tables->mem_big_pages[1][3] = SHADOW_BIG_PAGE_INDEX + 0;
        tables->mem_big_pages[1][4] = SHADOW_BIG_PAGE_INDEX + 1;
        tables->mem_big_pages[1][5] = SHADOW_BIG_PAGE_INDEX + 2;
        tables->mem_big_pages[1][6] = SHADOW_BIG_PAGE_INDEX + 3;
        tables->mem_big_pages[1][7] = SHADOW_BIG_PAGE_INDEX + 4;
    } else {
        tables->mem_big_pages[1][0] = MAIN_BIG_PAGE_INDEX + 0;
        tables->mem_big_pages[1][1] = MAIN_BIG_PAGE_INDEX + 1;
        tables->mem_big_pages[1][2] = MAIN_BIG_PAGE_INDEX + 2;
        tables->mem_big_pages[1][3] = MAIN_BIG_PAGE_INDEX + 3;
        tables->mem_big_pages[1][4] = MAIN_BIG_PAGE_INDEX + 4;
        tables->mem_big_pages[1][5] = MAIN_BIG_PAGE_INDEX + 5;
        tables->mem_big_pages[1][6] = MAIN_BIG_PAGE_INDEX + 6;
        tables->mem_big_pages[1][7] = MAIN_BIG_PAGE_INDEX + 7;
    }

    uint8_t rom = ROM0_BIG_PAGE_INDEX + romsel.bplus_bits.pr * NUM_ROM_BIG_PAGES;
    if (romsel.m128_bits.ram) {
        tables->mem_big_pages[0][0x8] = ANDY_BIG_PAGE_INDEX + 0;
        tables->mem_big_pages[0][0x9] = rom + 1;
        tables->mem_big_pages[0][0xa] = rom + 2;
        tables->mem_big_pages[0][0xb] = rom + 3;
    } else {
        tables->mem_big_pages[0][0x8] = rom + 0;
        tables->mem_big_pages[0][0x9] = rom + 1;
        tables->mem_big_pages[0][0xa] = rom + 2;
        tables->mem_big_pages[0][0xb] = rom + 3;
    }

    if (acccon.m128_bits.y) {
        tables->mem_big_pages[0][0xc] = HAZEL_BIG_PAGE_INDEX + 0;
        tables->mem_big_pages[0][0xd] = HAZEL_BIG_PAGE_INDEX + 1;
        tables->mem_big_pages[0][0xe] = MOS_BIG_PAGE_INDEX + 2;
        tables->mem_big_pages[0][0xf] = MOS_BIG_PAGE_INDEX + 3;

        memset(tables->pc_mem_big_pages_set, 0, 16);
    } else {
        tables->mem_big_pages[0][0xc] = MOS_BIG_PAGE_INDEX + 0;
        tables->mem_big_pages[0][0xd] = MOS_BIG_PAGE_INDEX + 1;
        tables->mem_big_pages[0][0xe] = MOS_BIG_PAGE_INDEX + 2;
        tables->mem_big_pages[0][0xf] = MOS_BIG_PAGE_INDEX + 3;

        memset(tables->pc_mem_big_pages_set, 0, 16);
        tables->pc_mem_big_pages_set[0xc] = 1;
        tables->pc_mem_big_pages_set[0xd] = 1;
    }

    memcpy(&tables->mem_big_pages[1][8], &tables->mem_big_pages[0][8], 8);

    *paging_flags = ((acccon.m128_bits.tst ? PagingFlags_ROMIO : 0) |
                     (acccon.m128_bits.d ? PagingFlags_DisplayShadow : 0) |
                     (acccon.m128_bits.ifj ? PagingFlags_IFJ : 0));
}

#if BBCMICRO_DEBUGGER
static void ApplyDSOMaster(ROMSEL *romsel, ACCCON *acccon, uint32_t dso) {
    ApplyROMDSO(romsel, dso);

    if (dso & BBCMicroDebugStateOverride_OverrideANDY) {
        romsel->m128_bits.ram = !!(dso & BBCMicroDebugStateOverride_ANDY);
    }

    if (dso & BBCMicroDebugStateOverride_OverrideHAZEL) {
        acccon->m128_bits.y = !!(dso & BBCMicroDebugStateOverride_HAZEL);
    }

    if (dso & BBCMicroDebugStateOverride_OverrideShadow) {
        acccon->m128_bits.x = !!(dso & BBCMicroDebugStateOverride_Shadow);
    }

    if (dso & BBCMicroDebugStateOverride_OverrideOS) {
        acccon->m128_bits.tst = !!(dso & BBCMicroDebugStateOverride_OS);
    }
}
#endif

#if BBCMICRO_DEBUGGER
static uint32_t GetDSOMaster(ROMSEL romsel, ACCCON acccon) {
    uint32_t dso = 0;

    dso |= romsel.m128_bits.pm;
    dso |= BBCMicroDebugStateOverride_OverrideROM;

    if (romsel.m128_bits.ram) {
        dso |= BBCMicroDebugStateOverride_ANDY;
    }
    dso |= BBCMicroDebugStateOverride_OverrideANDY;

    if (acccon.m128_bits.x) {
        dso |= BBCMicroDebugStateOverride_Shadow;
    }
    dso |= BBCMicroDebugStateOverride_OverrideShadow;

    if (acccon.m128_bits.y) {
        dso |= BBCMicroDebugStateOverride_HAZEL;
    }
    dso |= BBCMicroDebugStateOverride_OverrideHAZEL;

    if (acccon.m128_bits.tst) {
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
        big_pages[MOS_BIG_PAGE_INDEX + i].dso_mask &= ~(uint32_t)~BBCMicroDebugStateOverride_HAZEL;
        big_pages[MOS_BIG_PAGE_INDEX + i].dso_value |= BBCMicroDebugStateOverride_OverrideHAZEL;
    }

    // Switch IO off to see all of the last 4K of MOS.
    //
    // Might as well, since the hardware lets you...
    big_pages[MOS_BIG_PAGE_INDEX + 3].dso_value |= BBCMicroDebugStateOverride_OverrideOS | BBCMicroDebugStateOverride_OS;
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

const BBCMicroType BBC_MICRO_TYPE_MASTER_128 = {
    BBCMicroTypeID_Master, //type_id
    "Master 128",
    &M6502_cmos6502_config, //m6502_config
    65536,                  //ram_buffer_size
    DiscDriveType_133mm,    //default_disc_drive_type
#if BBCMICRO_DEBUGGER
    (BBCMicroDebugStateOverride_ROM |
     BBCMicroDebugStateOverride_OverrideROM |
     BBCMicroDebugStateOverride_ANDY |
     BBCMicroDebugStateOverride_OverrideANDY |
     BBCMicroDebugStateOverride_HAZEL |
     BBCMicroDebugStateOverride_OverrideHAZEL |
     BBCMicroDebugStateOverride_Shadow |
     BBCMicroDebugStateOverride_OverrideShadow |
     BBCMicroDebugStateOverride_OS |
     BBCMicroDebugStateOverride_OverrideOS), //dso_mask
#endif
    GetBigPagesMetadataMaster(),
    &GetMemBigPagesTablesMaster, //get_mem_big_page_tables_fn,
#if BBCMICRO_DEBUGGER
    &ApplyDSOMaster, //apply_dso_fn
    &GetDSOMaster,   //get_dso_fn
#endif
    0x8f, //romsel_mask,
    0xff, //acccon_mask,
    {
        {0x00, 0x1f},
        {0x28, 0x2b},
        {0x40, 0x7f},
    },      //sheila_cycle_stretch_regions
    0xfe18, //adc_addr
    8,      //adc_count
#if BBCMICRO_DEBUGGER
    &ParsePrefixLowerCaseCharMaster, //parse_prefix_char_fn
#endif
};

const BBCMicroType BBC_MICRO_TYPE_MASTER_COMPACT = {
    BBCMicroTypeID_MasterCompact, //type_id
    "Master Compact",
    &M6502_cmos6502_config,
    65536,
    DiscDriveType_90mm,
#if BBCMICRO_DEBUGGER
    (BBCMicroDebugStateOverride_ROM |
     BBCMicroDebugStateOverride_OverrideROM |
     BBCMicroDebugStateOverride_ANDY |
     BBCMicroDebugStateOverride_OverrideANDY |
     BBCMicroDebugStateOverride_HAZEL |
     BBCMicroDebugStateOverride_OverrideHAZEL |
     BBCMicroDebugStateOverride_Shadow |
     BBCMicroDebugStateOverride_OverrideShadow |
     BBCMicroDebugStateOverride_OS |
     BBCMicroDebugStateOverride_OverrideOS), //dso_mask
#endif
    GetBigPagesMetadataMaster(),
    &GetMemBigPagesTablesMaster, //get_mem_big_page_tables_fn,
#if BBCMICRO_DEBUGGER
    &ApplyDSOMaster, //apply_dso_fn
    &GetDSOMaster,   //get_dso_fn
#endif
    0x8f, //romsel_mask,
    0xff, //acccon_mask,
    {
        {0x00, 0x1f},
        {0x28, 0x2b},
        {0x40, 0x7f},
    }, //sheila_cycle_stretch_regions
    0, //adc_addr
    0, //adc_count
#if BBCMICRO_DEBUGGER
    &ParsePrefixLowerCaseCharMaster, //parse_prefix_char_fn
#endif
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static const BBCMicroType *const BBC_MICRO_TYPES[] = {
    &BBC_MICRO_TYPE_B,
    &BBC_MICRO_TYPE_B_PLUS,
    &BBC_MICRO_TYPE_MASTER_128,
    &BBC_MICRO_TYPE_MASTER_COMPACT,
};
static const size_t NUM_BBC_MICRO_TYPES = sizeof BBC_MICRO_TYPES / sizeof BBC_MICRO_TYPES[0];

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

size_t GetNumBBCMicroTypes() {
    return NUM_BBC_MICRO_TYPES;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

const BBCMicroType *GetBBCMicroTypeByIndex(size_t index) {
    ASSERT(index < GetNumBBCMicroTypes());
    return BBC_MICRO_TYPES[index];
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

const BBCMicroType *GetBBCMicroTypeForTypeID(BBCMicroTypeID type_id) {
    for (size_t i = 0; i < NUM_BBC_MICRO_TYPES; ++i) {
        if (BBC_MICRO_TYPES[i]->type_id == type_id) {
            return BBC_MICRO_TYPES[i];
        }
    }

    ASSERT(false);
    return &BBC_MICRO_TYPE_B;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static bool IsMaster(const BBCMicroType *type) {
    return type->type_id == BBCMicroTypeID_Master || type->type_id == BBCMicroTypeID_MasterCompact;
}

static bool IsB(const BBCMicroType *type) {
    return type->type_id == BBCMicroTypeID_B || type->type_id == BBCMicroTypeID_BPlus;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool HasNVRAM(const BBCMicroType *type) {
    return IsMaster(type);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool CanDisplayTeletextAt3C00(const BBCMicroType *type) {
    return IsB(type);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool HasNumericKeypad(const BBCMicroType *type) {
    return IsMaster(type);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool HasSpeech(const BBCMicroType *type) {
    return IsB(type);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool HasTube(const BBCMicroType *type) {
    return type->type_id != BBCMicroTypeID_MasterCompact;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool HasCartridges(const BBCMicroType *type) {
    return type->type_id == BBCMicroTypeID_Master;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool HasUserPort(const BBCMicroType *type) {
    return type->type_id != BBCMicroTypeID_MasterCompact;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool Has1MHzBus(const BBCMicroType *type) {
    return type->type_id != BBCMicroTypeID_MasterCompact;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool HasADC(const BBCMicroType *type) {
    return type->type_id != BBCMicroTypeID_MasterCompact;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool HasIndependentMOSView(const BBCMicroType *type) {
    return type->type_id != BBCMicroTypeID_B;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if BBCMICRO_DEBUGGER
bool ParseAddressPrefix(uint32_t *dso_ptr,
                        const BBCMicroType *type,
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
