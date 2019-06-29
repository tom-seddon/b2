#include <shared/system.h>
#include <shared/debug.h>
#include <beeb/conf.h>
#include <beeb/type.h>
#include <6502/6502.h>
#include <string.h>

#include <shared/enum_def.h>
#include <beeb/type.inl>
#include <shared/enum_end.h>

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

//#define ANDY_OFFSET (0x8000u+0u)
//#define NUM_ANDY_PAGES (0x10u)
//#define HAZEL_OFFSET (0x8000+0x1000u)
//#define NUM_HAZEL_PAGES (0x20u)
//#define SHADOW_OFFSET (0x8000+0x3000u)
//#define NUM_SHADOW_PAGES (0x30u)

const BigPageType ROM_BIG_PAGE_TYPES[16]={
    {'0',"ROM 0"},
    {'1',"ROM 1"},
    {'2',"ROM 2"},
    {'3',"ROM 3"},
    {'4',"ROM 4"},
    {'5',"ROM 5"},
    {'6',"ROM 6"},
    {'7',"ROM 7"},
    {'8',"ROM 8"},
    {'9',"ROM 9"},
    {'a',"ROM a"},
    {'b',"ROM b"},
    {'c',"ROM c"},
    {'d',"ROM d"},
    {'e',"ROM e"},
    {'f',"ROM f"},
};

const BigPageType MAIN_RAM_BIG_PAGE_TYPE={'m',"Main RAM"};
const BigPageType SHADOW_RAM_BIG_PAGE_TYPE={'s',"Shadow RAM"};
const BigPageType ANDY_BIG_PAGE_TYPE={'n',"ANDY"};
const BigPageType HAZEL_BIG_PAGE_TYPE={'h',"HAZEL"};
const BigPageType MOS_BIG_PAGE_TYPE={'o',"MOS ROM"};
const BigPageType IO_BIG_PAGE_TYPE={'i',"I/O area"};
const BigPageType INVALID_BIG_PAGE_TYPE={'?',"Invalid"};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if BBCMICRO_DEBUGGER
static void ApplyROMDPO(ROMSEL *romsel,uint32_t dpo) {
    if(dpo&BBCMicroDebugPagingOverride_OverrideROM) {
        romsel->b_bits.pr=dpo&BBCMicroDebugPagingOverride_ROM;
    }
}
#endif

static void SetBigPageTypes(std::vector<const BigPageType *> *types,
                            size_t index,
                            size_t n,
                            const BigPageType *type)
{
    for(size_t i=index;i<index+n;++i) {
        ASSERT(!(*types)[i]);
        (*types)[i]=type;
    }
}

static std::vector<const BigPageType *> GetBigPageTypesCommon() {
    std::vector<const BigPageType *> types;
    types.resize(NUM_BIG_PAGES,nullptr);

    SetBigPageTypes(&types,MAIN_BIG_PAGE_INDEX,NUM_MAIN_BIG_PAGES,&MAIN_RAM_BIG_PAGE_TYPE);

    for(size_t i=0;i<16;++i) {
        SetBigPageTypes(&types,ROM0_BIG_PAGE_INDEX+i*NUM_ROM_BIG_PAGES,NUM_ROM_BIG_PAGES,&ROM_BIG_PAGE_TYPES[i]);
    }

    SetBigPageTypes(&types,MOS_BIG_PAGE_INDEX,NUM_MOS_BIG_PAGES,&MOS_BIG_PAGE_TYPE);

    return types;
}

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

static std::vector<const BigPageType *> GetBigPageTypesB() {
    std::vector<const BigPageType *> types=GetBigPageTypesCommon();

    return types;
}

const BBCMicroType BBC_MICRO_TYPE_B={
    BBCMicroTypeID_B,//type_id
    &M6502_nmos6502_config,//m6502_config
    32768,//ram_buffer_size
    DiscDriveType_133mm,//default_disc_drive_type
#if BBCMICRO_DEBUGGER
    (BBCMicroDebugPagingOverride_OverrideROM|
     BBCMicroDebugPagingOverride_ROM),//dpo_mask
#endif
    GetBigPageTypesB(),
    &GetMemBigPageTablesB,//get_mem_big_page_tables_fn,
#if BBCMICRO_DEBUGGER
    &ApplyDPOB,//apply_dpo_fn
    &GetDPOB,//get_dpo_fn
#endif
    0x0f,//romsel_mask,
    0x00,//acccon_mask,
    (BBCMicroTypeFlag_CanDisplayTeletext3c00),//flags
    {{0x00,0x1f},{0x40,0x7f},{0xc0,0xdf},},//sheila_cycle_stretch_regions
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

    memcpy(tables->mem_big_pages[1]+8,tables->mem_big_pages[0]+8,8);

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

static std::vector<const BigPageType *> GetBigPageTypesBPlus() {
    std::vector<const BigPageType *> types=GetBigPageTypesCommon();

    SetBigPageTypes(&types,ANDY_BIG_PAGE_INDEX,NUM_ANDY_BIG_PAGES+NUM_HAZEL_BIG_PAGES,&ANDY_BIG_PAGE_TYPE);
    SetBigPageTypes(&types,SHADOW_BIG_PAGE_INDEX,NUM_SHADOW_BIG_PAGES,&SHADOW_RAM_BIG_PAGE_TYPE);

    return types;
}

const BBCMicroType BBC_MICRO_TYPE_B_PLUS={
    BBCMicroTypeID_BPlus,//type_id
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
    GetBigPageTypesBPlus(),
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

    memcpy(tables->mem_big_pages[1]+8,tables->mem_big_pages[0]+8,8);

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

static std::vector<const BigPageType *> GetBigPageTypesMaster() {
    std::vector<const BigPageType *> types=GetBigPageTypesCommon();

    SetBigPageTypes(&types,ANDY_BIG_PAGE_INDEX,NUM_ANDY_BIG_PAGES,&ANDY_BIG_PAGE_TYPE);
    SetBigPageTypes(&types,HAZEL_BIG_PAGE_INDEX,NUM_HAZEL_BIG_PAGES,&HAZEL_BIG_PAGE_TYPE);
    SetBigPageTypes(&types,SHADOW_BIG_PAGE_INDEX,NUM_SHADOW_BIG_PAGES,&SHADOW_RAM_BIG_PAGE_TYPE);

    return types;
}

const BBCMicroType BBC_MICRO_TYPE_MASTER={
    BBCMicroTypeID_Master,//type_id
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
    GetBigPageTypesMaster(),
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
