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
static void ApplyROMDPO(ROMSEL *romsel,uint32_t dpo) {
    if(dpo&BBCMicroDebugPagingOverride_OverrideROM) {
        romsel->b_bits.pr=dpo&BBCMicroDebugPagingOverride_ROM;
    }
}
#endif

static void InitBigPagesMetadata(std::vector<BigPageMetadata> *big_pages,
                                 size_t index,
                                 size_t n,
                                 char code,
                                 const std::string &description,
#if BBCMICRO_DEBUGGER
                                 uint32_t dpo_clear,
                                 uint32_t dpo_set,
#endif
                                 uint16_t base)
{
    for(size_t i=0;i<n;++i) {
        ASSERT(index+i<=NUM_BIG_PAGES);
        BigPageMetadata *bp=&(*big_pages)[index+i];

        ASSERT(bp->index==0xff);
        bp->index=(uint8_t)(index+i);
        bp->code=code;
        bp->description=description;
#if BBCMICRO_DEBUGGER
        bp->dpo_mask=~dpo_clear;
        bp->dpo_value=dpo_set;
#endif
        bp->addr=(uint16_t)(base+i*4096);
    }
}

static std::vector<BigPageMetadata> GetBigPagesMetadataCommon() {
    std::vector<BigPageMetadata> big_pages;
    big_pages.resize(NUM_BIG_PAGES);

    InitBigPagesMetadata(&big_pages,
                         MAIN_BIG_PAGE_INDEX,
                         NUM_MAIN_BIG_PAGES,
                         'm',"Main RAM",
#if BBCMICRO_DEBUGGER
                         0,
                         0,
#endif
                         0x0000);

    for(uint8_t i=0;i<16;++i) {
        char code[2];
        snprintf(code,sizeof code,"%x",i);

        char description[100];
        snprintf(description,sizeof description,"ROM %x",i);

        InitBigPagesMetadata(&big_pages,
                             ROM0_BIG_PAGE_INDEX+(size_t)i*NUM_ROM_BIG_PAGES,
                             NUM_ROM_BIG_PAGES,
                             code[0],description,
#if BBCMICRO_DEBUGGER
                             (uint32_t)BBCMicroDebugPagingOverride_ROM,
                             BBCMicroDebugPagingOverride_OverrideROM|i,
#endif
                             0x8000);
    }

    InitBigPagesMetadata(&big_pages,
                         MOS_BIG_PAGE_INDEX,
                         NUM_MOS_BIG_PAGES,
                         'o',"MOS ROM",
#if BBCMICRO_DEBUGGER
                         0,
                         0,
#endif
                         0xc000);

    return big_pages;
}

#if BBCMICRO_DEBUGGER
static bool HandleROMPrefixChar(uint32_t *dpo,uint8_t rom) {
    ASSERT(rom>=0&&rom<=15);

    *dpo&=~(uint32_t)(BBCMicroDebugPagingOverride_ROM|BBCMicroDebugPagingOverride_ANDY);
    *dpo|=BBCMicroDebugPagingOverride_OverrideANDY|BBCMicroDebugPagingOverride_OverrideROM|rom;

    return true;
}
#endif

#if BBCMICRO_DEBUGGER
// select ROM
static bool ParseROMPrefixChar(uint32_t *dpo,char c) {
    if(c>='0'&&c<='9') {
        return HandleROMPrefixChar(dpo,(uint8_t)(c-'0'));
    } else if(c>='a'&&c<='f') {
        return HandleROMPrefixChar(dpo,(uint8_t)(c-'a'+10));
    } else if(c>='A'&&c<='F') {
        return HandleROMPrefixChar(dpo,(uint8_t)(c-'A'+10));
    } else {
        return false;
    }
}
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static void GetMemBigPageTablesB(MemoryBigPageTables *tables,
                                 bool *io,
                                 bool *crt_shadow,
                                 ROMSEL romsel,
                                 ACCCON acccon)
{
    (void)acccon;//not relevant for B

    tables->mem_big_pages[0][0]=MAIN_BIG_PAGE_INDEX+0;
    tables->mem_big_pages[0][1]=MAIN_BIG_PAGE_INDEX+1;
    tables->mem_big_pages[0][2]=MAIN_BIG_PAGE_INDEX+2;
    tables->mem_big_pages[0][3]=MAIN_BIG_PAGE_INDEX+3;
    tables->mem_big_pages[0][4]=MAIN_BIG_PAGE_INDEX+4;
    tables->mem_big_pages[0][5]=MAIN_BIG_PAGE_INDEX+5;
    tables->mem_big_pages[0][6]=MAIN_BIG_PAGE_INDEX+6;
    tables->mem_big_pages[0][7]=MAIN_BIG_PAGE_INDEX+7;

    uint8_t rom=ROM0_BIG_PAGE_INDEX+romsel.b_bits.pr*NUM_ROM_BIG_PAGES;
    tables->mem_big_pages[0][0x8]=rom+0;
    tables->mem_big_pages[0][0x9]=rom+1;
    tables->mem_big_pages[0][0xa]=rom+2;
    tables->mem_big_pages[0][0xb]=rom+3;

    tables->mem_big_pages[0][0xc]=MOS_BIG_PAGE_INDEX+0;
    tables->mem_big_pages[0][0xd]=MOS_BIG_PAGE_INDEX+1;
    tables->mem_big_pages[0][0xe]=MOS_BIG_PAGE_INDEX+2;
    tables->mem_big_pages[0][0xf]=MOS_BIG_PAGE_INDEX+3;

    memset(tables->mem_big_pages[1],0,16);
    memset(tables->pc_mem_big_pages_set,0,16);
    *io=true;
    *crt_shadow=false;
}

#if BBCMICRO_DEBUGGER
static void ApplyDPOB(ROMSEL *romsel,ACCCON *acccon,uint32_t dpo) {
    (void)acccon;

    ApplyROMDPO(romsel,dpo);
}
#endif

#if BBCMICRO_DEBUGGER
static uint32_t GetDPOB(ROMSEL romsel,ACCCON acccon) {
    (void)acccon;

    uint32_t dpo=0;

    dpo|=romsel.b_bits.pr;
    dpo|=BBCMicroDebugPagingOverride_OverrideROM;

    return dpo;
}
#endif

static std::vector<BigPageMetadata> GetBigPagesMetadataB() {
    std::vector<BigPageMetadata> big_pages=GetBigPagesMetadataCommon();

    return big_pages;
}

#if BBCMICRO_DEBUGGER
static bool ParsePrefixCharB(uint32_t *dpo,char c) {
    if(ParseROMPrefixChar(dpo,c)) {
        // ...
    } else if(c=='m'||c=='M'||c=='o'||c=='O'||c=='i'||c=='I') {
        // Valid, but no effect. These are supported on the basis that if you
        // can see them in the UI, you ought to be able to type them in...
    } else {
        return false;
    }

    return true;
}
#endif

const BBCMicroType BBC_MICRO_TYPE_B={
    BBCMicroTypeID_B,//type_id
    "B",
    &M6502_nmos6502_config,//m6502_config
    32768,//ram_buffer_size
    DiscDriveType_133mm,//default_disc_drive_type
#if BBCMICRO_DEBUGGER
    (BBCMicroDebugPagingOverride_OverrideROM|
     BBCMicroDebugPagingOverride_ROM),//dpo_mask
#endif
    GetBigPagesMetadataB(),
    &GetMemBigPageTablesB,//get_mem_big_page_tables_fn,
#if BBCMICRO_DEBUGGER
    &ApplyDPOB,//apply_dpo_fn
    &GetDPOB,//get_dpo_fn
#endif
    0x0f,//romsel_mask,
    0x00,//acccon_mask,
    (BBCMicroTypeFlag_CanDisplayTeletext3c00),//flags
    {{0x00,0x1f},{0x40,0x7f},{0xc0,0xdf},},//sheila_cycle_stretch_regions
#if BBCMICRO_DEBUGGER
    &ParsePrefixCharB,//parse_prefix_char_fn
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
                                     bool *io,
                                     bool *crt_shadow,
                                     ROMSEL romsel,
                                     ACCCON acccon)
{
    tables->mem_big_pages[0][0]=MAIN_BIG_PAGE_INDEX+0;
    tables->mem_big_pages[0][1]=MAIN_BIG_PAGE_INDEX+1;
    tables->mem_big_pages[0][2]=MAIN_BIG_PAGE_INDEX+2;
    tables->mem_big_pages[0][3]=MAIN_BIG_PAGE_INDEX+3;
    tables->mem_big_pages[0][4]=MAIN_BIG_PAGE_INDEX+4;
    tables->mem_big_pages[0][5]=MAIN_BIG_PAGE_INDEX+5;
    tables->mem_big_pages[0][6]=MAIN_BIG_PAGE_INDEX+6;
    tables->mem_big_pages[0][7]=MAIN_BIG_PAGE_INDEX+7;

    if(acccon.bplus_bits.shadow) {
        tables->mem_big_pages[1][0]=MAIN_BIG_PAGE_INDEX+0;
        tables->mem_big_pages[1][1]=MAIN_BIG_PAGE_INDEX+1;
        tables->mem_big_pages[1][2]=MAIN_BIG_PAGE_INDEX+2;
        tables->mem_big_pages[1][3]=SHADOW_BIG_PAGE_INDEX+0;
        tables->mem_big_pages[1][4]=SHADOW_BIG_PAGE_INDEX+1;
        tables->mem_big_pages[1][5]=SHADOW_BIG_PAGE_INDEX+2;
        tables->mem_big_pages[1][6]=SHADOW_BIG_PAGE_INDEX+3;
        tables->mem_big_pages[1][7]=SHADOW_BIG_PAGE_INDEX+4;
    } else {
        tables->mem_big_pages[1][0]=MAIN_BIG_PAGE_INDEX+0;
        tables->mem_big_pages[1][1]=MAIN_BIG_PAGE_INDEX+1;
        tables->mem_big_pages[1][2]=MAIN_BIG_PAGE_INDEX+2;
        tables->mem_big_pages[1][3]=MAIN_BIG_PAGE_INDEX+3;
        tables->mem_big_pages[1][4]=MAIN_BIG_PAGE_INDEX+4;
        tables->mem_big_pages[1][5]=MAIN_BIG_PAGE_INDEX+5;
        tables->mem_big_pages[1][6]=MAIN_BIG_PAGE_INDEX+6;
        tables->mem_big_pages[1][7]=MAIN_BIG_PAGE_INDEX+7;
    }

    uint8_t rom=ROM0_BIG_PAGE_INDEX+romsel.bplus_bits.pr*NUM_ROM_BIG_PAGES;
    if(romsel.bplus_bits.ram) {
        tables->mem_big_pages[0][0x8]=ANDY_BIG_PAGE_INDEX+0;
        tables->mem_big_pages[0][0x9]=ANDY_BIG_PAGE_INDEX+1;
        tables->mem_big_pages[0][0xa]=ANDY_BIG_PAGE_INDEX+2;
        tables->mem_big_pages[0][0xb]=rom+3;

        memset(tables->pc_mem_big_pages_set,0,16);
        tables->pc_mem_big_pages_set[0xa]=1;
        tables->pc_mem_big_pages_set[0xc]=1;
        tables->pc_mem_big_pages_set[0xd]=1;
    } else {
        tables->mem_big_pages[0][0x8]=rom+0;
        tables->mem_big_pages[0][0x9]=rom+1;
        tables->mem_big_pages[0][0xa]=rom+2;
        tables->mem_big_pages[0][0xb]=rom+3;

        memset(tables->pc_mem_big_pages_set,0,16);
        tables->pc_mem_big_pages_set[0xc]=1;
        tables->pc_mem_big_pages_set[0xd]=1;
    }

    tables->mem_big_pages[0][0xc]=MOS_BIG_PAGE_INDEX+0;
    tables->mem_big_pages[0][0xd]=MOS_BIG_PAGE_INDEX+1;
    tables->mem_big_pages[0][0xe]=MOS_BIG_PAGE_INDEX+2;
    tables->mem_big_pages[0][0xf]=MOS_BIG_PAGE_INDEX+3;

    memcpy(&tables->mem_big_pages[1][8],&tables->mem_big_pages[0][8],8);

    *crt_shadow=acccon.bplus_bits.shadow!=0;
    *io=true;
}

#if BBCMICRO_DEBUGGER
static void ApplyDPOBPlus(ROMSEL *romsel,ACCCON *acccon,uint32_t dpo) {
    ApplyROMDPO(romsel,dpo);

    if(dpo&BBCMicroDebugPagingOverride_OverrideANDY) {
        romsel->bplus_bits.ram=!!(dpo&BBCMicroDebugPagingOverride_ANDY);
    }

    if(dpo&BBCMicroDebugPagingOverride_OverrideShadow) {
        acccon->bplus_bits.shadow=!!(dpo&BBCMicroDebugPagingOverride_Shadow);
    }
}
#endif

#if BBCMICRO_DEBUGGER
static uint32_t GetDPOBPlus(ROMSEL romsel,ACCCON acccon) {
    uint32_t dpo=0;

    dpo|=romsel.bplus_bits.pr;
    dpo|=BBCMicroDebugPagingOverride_OverrideROM;

    if(romsel.bplus_bits.ram) {
        dpo|=BBCMicroDebugPagingOverride_ANDY;
    }
    dpo|=BBCMicroDebugPagingOverride_OverrideANDY;

    if(acccon.bplus_bits.shadow) {
        dpo|=BBCMicroDebugPagingOverride_Shadow;
    }
    dpo|=BBCMicroDebugPagingOverride_OverrideShadow;

    return dpo;
}
#endif

static std::vector<BigPageMetadata> GetBigPagesMetadataBPlus() {
    std::vector<BigPageMetadata> big_pages=GetBigPagesMetadataCommon();

    InitBigPagesMetadata(&big_pages,
                         ANDY_BIG_PAGE_INDEX,
                         NUM_ANDY_BIG_PAGES+NUM_HAZEL_BIG_PAGES,
                         'n',"ANDY",
#if BBCMICRO_DEBUGGER
                         0,
                         BBCMicroDebugPagingOverride_OverrideANDY|BBCMicroDebugPagingOverride_ANDY,
#endif
                         0x8000);

    InitBigPagesMetadata(&big_pages,
                         SHADOW_BIG_PAGE_INDEX,
                         NUM_SHADOW_BIG_PAGES,
                         's',"Shadow RAM",
#if BBCMICRO_DEBUGGER
                         0,
                         BBCMicroDebugPagingOverride_OverrideShadow|BBCMicroDebugPagingOverride_Shadow,
#endif
                         0x3000);

    return big_pages;
}

#if BBCMICRO_DEBUGGER
static bool ParsePrefixCharBPlus(uint32_t *dpo,char c) {
    if(ParseROMPrefixChar(dpo,c)) {
        // ...
    } else if(c=='s'||c=='S') {
        *dpo|=BBCMicroDebugPagingOverride_OverrideShadow|BBCMicroDebugPagingOverride_Shadow;
    } else if(c=='m'||c=='M') {
        *dpo|=BBCMicroDebugPagingOverride_OverrideShadow;
        *dpo&=~(uint32_t)BBCMicroDebugPagingOverride_Shadow;
    } else if(c=='n'||c=='N') {
        *dpo|=BBCMicroDebugPagingOverride_OverrideANDY|BBCMicroDebugPagingOverride_ANDY;
    } else if(c=='i'||c=='I'||c=='o'||c=='O') {
        // Valid, but no effect. These are supported on the basis that if you
        // can see them in the UI, you ought to be able to type them in...
    } else {
        return false;
    }

    return true;
}
#endif

const BBCMicroType BBC_MICRO_TYPE_B_PLUS={
    BBCMicroTypeID_BPlus,//type_id
    "B+",
    &M6502_nmos6502_config,//m6502_config
    65536,//ram_buffer_size
    DiscDriveType_133mm,//default_disc_drive_type
#if BBCMICRO_DEBUGGER
    (BBCMicroDebugPagingOverride_ROM|
     BBCMicroDebugPagingOverride_OverrideROM|
     BBCMicroDebugPagingOverride_ANDY|
     BBCMicroDebugPagingOverride_OverrideANDY|
     BBCMicroDebugPagingOverride_Shadow|
     BBCMicroDebugPagingOverride_OverrideShadow),//dpo_mask
#endif
    GetBigPagesMetadataBPlus(),
    &GetMemBigPageTablesBPlus,//get_mem_big_page_tables_fn,
#if BBCMICRO_DEBUGGER
    &ApplyDPOBPlus,//apply_dpo_fn
    &GetDPOBPlus,//get_dpo_fn
#endif
    0x8f,//romsel_mask,
    0x80,//acccon_mask,
    (BBCMicroTypeFlag_CanDisplayTeletext3c00|
     BBCMicroTypeFlag_HasShadowRAM),//flags
    {{0x00,0x1f},{0x40,0x7f},{0xc0,0xdf},},//sheila_cycle_stretch_regions
#if BBCMICRO_DEBUGGER
    &ParsePrefixCharBPlus,//parse_prefix_char_fn
#endif
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static void GetMemBigPagesTablesMaster(MemoryBigPageTables *tables,
                                       bool *io,
                                       bool *crt_shadow,
                                       ROMSEL romsel,
                                       ACCCON acccon)
{
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

    if(acccon.m128_bits.x) {
        tables->mem_big_pages[0][0]=MAIN_BIG_PAGE_INDEX+0;
        tables->mem_big_pages[0][1]=MAIN_BIG_PAGE_INDEX+1;
        tables->mem_big_pages[0][2]=MAIN_BIG_PAGE_INDEX+2;
        tables->mem_big_pages[0][3]=SHADOW_BIG_PAGE_INDEX+0;
        tables->mem_big_pages[0][4]=SHADOW_BIG_PAGE_INDEX+1;
        tables->mem_big_pages[0][5]=SHADOW_BIG_PAGE_INDEX+2;
        tables->mem_big_pages[0][6]=SHADOW_BIG_PAGE_INDEX+3;
        tables->mem_big_pages[0][7]=SHADOW_BIG_PAGE_INDEX+4;
    } else {
        tables->mem_big_pages[0][0]=MAIN_BIG_PAGE_INDEX+0;
        tables->mem_big_pages[0][1]=MAIN_BIG_PAGE_INDEX+1;
        tables->mem_big_pages[0][2]=MAIN_BIG_PAGE_INDEX+2;
        tables->mem_big_pages[0][3]=MAIN_BIG_PAGE_INDEX+3;
        tables->mem_big_pages[0][4]=MAIN_BIG_PAGE_INDEX+4;
        tables->mem_big_pages[0][5]=MAIN_BIG_PAGE_INDEX+5;
        tables->mem_big_pages[0][6]=MAIN_BIG_PAGE_INDEX+6;
        tables->mem_big_pages[0][7]=MAIN_BIG_PAGE_INDEX+7;
    }

    if((acccon.m128_bits.y&&acccon.m128_bits.x)||
       (!acccon.m128_bits.y&&acccon.m128_bits.e))
    {
        tables->mem_big_pages[1][0]=MAIN_BIG_PAGE_INDEX+0;
        tables->mem_big_pages[1][1]=MAIN_BIG_PAGE_INDEX+1;
        tables->mem_big_pages[1][2]=MAIN_BIG_PAGE_INDEX+2;
        tables->mem_big_pages[1][3]=SHADOW_BIG_PAGE_INDEX+0;
        tables->mem_big_pages[1][4]=SHADOW_BIG_PAGE_INDEX+1;
        tables->mem_big_pages[1][5]=SHADOW_BIG_PAGE_INDEX+2;
        tables->mem_big_pages[1][6]=SHADOW_BIG_PAGE_INDEX+3;
        tables->mem_big_pages[1][7]=SHADOW_BIG_PAGE_INDEX+4;
    } else {
        tables->mem_big_pages[1][0]=MAIN_BIG_PAGE_INDEX+0;
        tables->mem_big_pages[1][1]=MAIN_BIG_PAGE_INDEX+1;
        tables->mem_big_pages[1][2]=MAIN_BIG_PAGE_INDEX+2;
        tables->mem_big_pages[1][3]=MAIN_BIG_PAGE_INDEX+3;
        tables->mem_big_pages[1][4]=MAIN_BIG_PAGE_INDEX+4;
        tables->mem_big_pages[1][5]=MAIN_BIG_PAGE_INDEX+5;
        tables->mem_big_pages[1][6]=MAIN_BIG_PAGE_INDEX+6;
        tables->mem_big_pages[1][7]=MAIN_BIG_PAGE_INDEX+7;
    }

    uint8_t rom=ROM0_BIG_PAGE_INDEX+romsel.bplus_bits.pr*NUM_ROM_BIG_PAGES;
    if(romsel.m128_bits.ram) {
        tables->mem_big_pages[0][0x8]=ANDY_BIG_PAGE_INDEX+0;
        tables->mem_big_pages[0][0x9]=rom+1;
        tables->mem_big_pages[0][0xa]=rom+2;
        tables->mem_big_pages[0][0xb]=rom+3;
    } else {
        tables->mem_big_pages[0][0x8]=rom+0;
        tables->mem_big_pages[0][0x9]=rom+1;
        tables->mem_big_pages[0][0xa]=rom+2;
        tables->mem_big_pages[0][0xb]=rom+3;
    }

    if(acccon.m128_bits.y) {
        tables->mem_big_pages[0][0xc]=HAZEL_BIG_PAGE_INDEX+0;
        tables->mem_big_pages[0][0xd]=HAZEL_BIG_PAGE_INDEX+1;
        tables->mem_big_pages[0][0xe]=MOS_BIG_PAGE_INDEX+2;
        tables->mem_big_pages[0][0xf]=MOS_BIG_PAGE_INDEX+3;

        memset(tables->pc_mem_big_pages_set,0,16);
    } else {
        tables->mem_big_pages[0][0xc]=MOS_BIG_PAGE_INDEX+0;
        tables->mem_big_pages[0][0xd]=MOS_BIG_PAGE_INDEX+1;
        tables->mem_big_pages[0][0xe]=MOS_BIG_PAGE_INDEX+2;
        tables->mem_big_pages[0][0xf]=MOS_BIG_PAGE_INDEX+3;

        memset(tables->pc_mem_big_pages_set,0,16);
        tables->pc_mem_big_pages_set[0xc]=1;
        tables->pc_mem_big_pages_set[0xd]=1;
    }

    memcpy(&tables->mem_big_pages[1][8],&tables->mem_big_pages[0][8],8);

    *io=acccon.m128_bits.tst==0;
    *crt_shadow=acccon.m128_bits.d!=0;
}

#if BBCMICRO_DEBUGGER
static void ApplyDPOMaster(ROMSEL *romsel,ACCCON *acccon,uint32_t dpo) {
    ApplyROMDPO(romsel,dpo);

    if(dpo&BBCMicroDebugPagingOverride_OverrideANDY) {
        romsel->m128_bits.ram=!!(dpo&BBCMicroDebugPagingOverride_ANDY);
    }

    if(dpo&BBCMicroDebugPagingOverride_OverrideHAZEL) {
        acccon->m128_bits.y=!!(dpo&BBCMicroDebugPagingOverride_HAZEL);
    }

    if(dpo&BBCMicroDebugPagingOverride_OverrideShadow) {
        acccon->m128_bits.x=!!(dpo&BBCMicroDebugPagingOverride_Shadow);
    }

    if(dpo&BBCMicroDebugPagingOverride_OverrideOS) {
        acccon->m128_bits.tst=!!(dpo&BBCMicroDebugPagingOverride_OS);
    }
}
#endif

#if BBCMICRO_DEBUGGER
static uint32_t GetDPOMaster(ROMSEL romsel,ACCCON acccon) {
    uint32_t dpo=0;

    dpo|=romsel.m128_bits.pm;
    dpo|=BBCMicroDebugPagingOverride_OverrideROM;

    if(romsel.m128_bits.ram) {
        dpo|=BBCMicroDebugPagingOverride_ANDY;
    }
    dpo|=BBCMicroDebugPagingOverride_OverrideANDY;

    if(acccon.m128_bits.x) {
        dpo|=BBCMicroDebugPagingOverride_Shadow;
    }
    dpo|=BBCMicroDebugPagingOverride_OverrideShadow;

    if(acccon.m128_bits.y) {
        dpo|=BBCMicroDebugPagingOverride_HAZEL;
    }
    dpo|=BBCMicroDebugPagingOverride_OverrideHAZEL;

    if(acccon.m128_bits.tst) {
        dpo|=BBCMicroDebugPagingOverride_OS;
    }
    dpo|=BBCMicroDebugPagingOverride_OverrideOS;

    return dpo;
}
#endif

static std::vector<BigPageMetadata> GetBigPagesMetadataMaster() {
    std::vector<BigPageMetadata> big_pages=GetBigPagesMetadataCommon();

    InitBigPagesMetadata(&big_pages,
                         ANDY_BIG_PAGE_INDEX,
                         NUM_ANDY_BIG_PAGES,
                         'n',"ANDY",
#if BBCMICRO_DEBUGGER
                         0,
                         BBCMicroDebugPagingOverride_OverrideANDY|BBCMicroDebugPagingOverride_ANDY,
#endif
                         0x8000);

    InitBigPagesMetadata(&big_pages,
                         HAZEL_BIG_PAGE_INDEX,
                         NUM_HAZEL_BIG_PAGES,
                         'h',"HAZEL",
#if BBCMICRO_DEBUGGER
                         0,
                         BBCMicroDebugPagingOverride_OverrideHAZEL|BBCMicroDebugPagingOverride_HAZEL,
#endif
                         0xc000);

    InitBigPagesMetadata(&big_pages,
                         SHADOW_BIG_PAGE_INDEX,
                         NUM_SHADOW_BIG_PAGES,
                         's',"Shadow RAM",
#if BBCMICRO_DEBUGGER
                         0,
                         BBCMicroDebugPagingOverride_OverrideShadow|BBCMicroDebugPagingOverride_Shadow,
#endif
                         0x3000);

#if BBCMICRO_DEBUGGER
    // Update the MOS DPO flags.

    // Switch HAZEL off to see the first 8K of MOS.
    for(size_t i=0;i<2;++i) {
        big_pages[MOS_BIG_PAGE_INDEX+i].dpo_mask&=~(uint32_t)~BBCMicroDebugPagingOverride_HAZEL;
        big_pages[MOS_BIG_PAGE_INDEX+i].dpo_value|=BBCMicroDebugPagingOverride_OverrideHAZEL;
    }

    // Switch IO off to see all of the last 4K of MOS.
    //
    // Might as well, since the hardware lets you...
    big_pages[MOS_BIG_PAGE_INDEX+3].dpo_value|=BBCMicroDebugPagingOverride_OverrideOS|BBCMicroDebugPagingOverride_OS;
#endif

    return big_pages;
}

#if BBCMICRO_DEBUGGER
static bool ParsePrefixCharMaster(uint32_t *dpo,char c) {
    if(ParseROMPrefixChar(dpo,c)) {
        // ...
    } else if(c=='s'||c=='S') {
        *dpo|=BBCMicroDebugPagingOverride_OverrideShadow|BBCMicroDebugPagingOverride_Shadow;
    } else if(c=='m'||c=='M') {
        *dpo|=BBCMicroDebugPagingOverride_OverrideShadow;
        *dpo&=~(uint32_t)BBCMicroDebugPagingOverride_Shadow;
    } else if(c=='h'||c=='H') {
        *dpo|=BBCMicroDebugPagingOverride_OverrideHAZEL|BBCMicroDebugPagingOverride_HAZEL;
    } else if(c=='n'||c=='N') {
        *dpo|=BBCMicroDebugPagingOverride_OverrideANDY|BBCMicroDebugPagingOverride_ANDY;
    } else if(c=='o'||c=='O') {
        *dpo|=BBCMicroDebugPagingOverride_OverrideHAZEL|BBCMicroDebugPagingOverride_OverrideOS|BBCMicroDebugPagingOverride_OS;
        *dpo&=~(uint32_t)BBCMicroDebugPagingOverride_HAZEL;
    } else if(c=='i'||c=='I') {
        *dpo|=BBCMicroDebugPagingOverride_OverrideOS;
        *dpo&=~(uint32_t)BBCMicroDebugPagingOverride_OS;
    } else {
        return false;
    }

    return true;
}
#endif

const BBCMicroType BBC_MICRO_TYPE_MASTER={
    BBCMicroTypeID_Master,//type_id
    "Master 128",
    &M6502_cmos6502_config,//m6502_config
    65536,//ram_buffer_size
    DiscDriveType_133mm,//default_disc_drive_type
#if BBCMICRO_DEBUGGER
    (BBCMicroDebugPagingOverride_ROM|
     BBCMicroDebugPagingOverride_OverrideROM|
     BBCMicroDebugPagingOverride_ANDY|
     BBCMicroDebugPagingOverride_OverrideANDY|
     BBCMicroDebugPagingOverride_HAZEL|
     BBCMicroDebugPagingOverride_OverrideHAZEL|
     BBCMicroDebugPagingOverride_Shadow|
     BBCMicroDebugPagingOverride_OverrideShadow|
     BBCMicroDebugPagingOverride_OS|
     BBCMicroDebugPagingOverride_OverrideOS),//dpo_mask
#endif
    GetBigPagesMetadataMaster(),
    &GetMemBigPagesTablesMaster,//get_mem_big_page_tables_fn,
#if BBCMICRO_DEBUGGER
    &ApplyDPOMaster,//apply_dpo_fn
    &GetDPOMaster,//get_dpo_fn
#endif
    0x8f,//romsel_mask,
    0xff,//acccon_mask,
    (BBCMicroTypeFlag_HasShadowRAM|
     BBCMicroTypeFlag_HasRTC|
     BBCMicroTypeFlag_HasNumericKeypad),//flags
    {{0x00,0x1f},{0x28,0x2b},{0x40,0x7f},},//sheila_cycle_stretch_regions
#if BBCMICRO_DEBUGGER
    &ParsePrefixCharMaster,//parse_prefix_char_fn
#endif
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static const BBCMicroType *const BBC_MICRO_TYPES[]={
    &BBC_MICRO_TYPE_B,
    &BBC_MICRO_TYPE_B_PLUS,
    &BBC_MICRO_TYPE_MASTER,
};
static const size_t NUM_BBC_MICRO_TYPES=sizeof BBC_MICRO_TYPES/sizeof BBC_MICRO_TYPES[0];

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

size_t GetNumBBCMicroTypes() {
    return NUM_BBC_MICRO_TYPES;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

const BBCMicroType *GetBBCMicroTypeByIndex(size_t index) {
    ASSERT(index<GetNumBBCMicroTypes());
    return BBC_MICRO_TYPES[index];
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

const BBCMicroType *GetBBCMicroTypeForTypeID(BBCMicroTypeID type_id) {
    for(size_t i=0;i<NUM_BBC_MICRO_TYPES;++i) {
        if(BBC_MICRO_TYPES[i]->type_id==type_id) {
            return BBC_MICRO_TYPES[i];
        }
    }

    ASSERT(false);
    return &BBC_MICRO_TYPE_B;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if BBCMICRO_DEBUGGER
bool ParseAddressPrefix(uint32_t *dpo_ptr,
                        const BBCMicroType *type,
                        const char *prefix_begin,
                        const char *prefix_end,
                        Log *log)
{
    uint32_t dpo=*dpo_ptr;

    for(const char *c=prefix_begin;c!=prefix_end;++c) {
        if(!(*type->parse_prefix_char_fn)(&dpo,*c)) {
            if(log) {
                log->f("'%c': unknown address prefix",*c);
            }

            return false;
        }
    }

    *dpo_ptr=dpo;

    return true;
}
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
