#include <shared/system.h>
#include <shared/debug.h>
#include <beeb/type.h>
#include <6502/6502.h>
#include <beeb/paging.h>

#include <shared/enum_def.h>
#include <beeb/type.inl>
#include <shared/enum_end.h>

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static void ApplyROMDPO(ROMSEL *romsel,uint32_t dpo) {
    if(dpo&BBCMicroDebugPagingOverride_OverrideROM) {
        romsel->b_bits.pr=dpo&BBCMicroDebugPagingOverride_ROM;
    }
}

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

static void GetMemBigPageTablesB(uint8_t *usr,
                                 uint8_t *mos,
                                 uint8_t *mos_pc_mem_big_pages,
                                 bool *io,
                                 bool *crt_shadow,
                                 ROMSEL romsel,
                                 ACCCON acccon)
{
    (void)mos;//not relevant for B
    (void)acccon;//not relevant for B

    usr[0]=MAIN_BIG_PAGE_INDEX+0;
    usr[1]=MAIN_BIG_PAGE_INDEX+1;
    usr[2]=MAIN_BIG_PAGE_INDEX+2;
    usr[3]=MAIN_BIG_PAGE_INDEX+3;
    usr[4]=MAIN_BIG_PAGE_INDEX+4;
    usr[5]=MAIN_BIG_PAGE_INDEX+5;
    usr[6]=MAIN_BIG_PAGE_INDEX+6;
    usr[7]=MAIN_BIG_PAGE_INDEX+7;

    uint8_t rom=ROM0_BIG_PAGE_INDEX+romsel.b_bits.pr*NUM_ROM_BIG_PAGES;
    usr[0x8]=rom+0;
    usr[0x9]=rom+1;
    usr[0xa]=rom+2;
    usr[0xb]=rom+3;

    usr[0xc]=MOS_BIG_PAGE_INDEX+0;
    usr[0xd]=MOS_BIG_PAGE_INDEX+1;
    usr[0xe]=MOS_BIG_PAGE_INDEX+2;
    usr[0xf]=MOS_BIG_PAGE_INDEX+3;

    memset(mos_pc_mem_big_pages,0,16);
    *io=true;
    *crt_shadow=false;
}

static void ApplyDPOB(ROMSEL *romsel,ACCCON *acccon,uint32_t dpo) {
    (void)acccon;

    ApplyROMDPO(romsel,dpo);
}

static std::vector<const BigPageType *> GetBigPageTypesB() {
    std::vector<const BigPageType *> types=GetBigPageTypesCommon();

    return types;
}

const BBCMicroType BBC_MICRO_TYPE_B={
    BBCMicroTypeID_B,//type_id
    &M6502_nmos6502_config,//m6502_config
    32768,//ram_buffer_size
    DiscDriveType_133mm,//default_disc_drive_type
    (BBCMicroDebugPagingOverride_OverrideROM|
     BBCMicroDebugPagingOverride_ROM),//dpo_mask
    GetBigPageTypesB(),
    &GetMemBigPageTablesB,//get_mem_big_page_tables_fn,
    &ApplyDPOB,//apply_dpo_fn
    0x0f,//romsel_mask,
    0x00,//acccon_mask,
    (BBCMicroTypeFlag_CanDisplayTeletext3c00),//flags
    {{0x00,0x1f},{0x40,0x7f},{0xc0,0xdf},},//sheila_cycle_stretch_regions
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static void GetMemBigPageTablesBPlus(uint8_t *usr,
                                     uint8_t *mos,
                                     uint8_t *mos_pc_mem_big_pages,
                                     bool *io,
                                     bool *crt_shadow,
                                     ROMSEL romsel,
                                     ACCCON acccon)
{
    usr[0]=MAIN_BIG_PAGE_INDEX+0;
    usr[1]=MAIN_BIG_PAGE_INDEX+1;
    usr[2]=MAIN_BIG_PAGE_INDEX+2;
    usr[3]=MAIN_BIG_PAGE_INDEX+3;
    usr[4]=MAIN_BIG_PAGE_INDEX+4;
    usr[5]=MAIN_BIG_PAGE_INDEX+5;
    usr[6]=MAIN_BIG_PAGE_INDEX+6;
    usr[7]=MAIN_BIG_PAGE_INDEX+7;

    if(acccon.bplus_bits.shadow) {
        mos[0]=MAIN_BIG_PAGE_INDEX+0;
        mos[1]=MAIN_BIG_PAGE_INDEX+1;
        mos[2]=MAIN_BIG_PAGE_INDEX+2;
        mos[3]=SHADOW_BIG_PAGE_INDEX+0;
        mos[4]=SHADOW_BIG_PAGE_INDEX+1;
        mos[5]=SHADOW_BIG_PAGE_INDEX+2;
        mos[6]=SHADOW_BIG_PAGE_INDEX+3;
        mos[7]=SHADOW_BIG_PAGE_INDEX+4;
    } else {
        mos[0]=MAIN_BIG_PAGE_INDEX+0;
        mos[1]=MAIN_BIG_PAGE_INDEX+1;
        mos[2]=MAIN_BIG_PAGE_INDEX+2;
        mos[3]=MAIN_BIG_PAGE_INDEX+3;
        mos[4]=MAIN_BIG_PAGE_INDEX+4;
        mos[5]=MAIN_BIG_PAGE_INDEX+5;
        mos[6]=MAIN_BIG_PAGE_INDEX+6;
        mos[7]=MAIN_BIG_PAGE_INDEX+7;
    }

    uint8_t rom=ROM0_BIG_PAGE_INDEX+romsel.bplus_bits.pr*NUM_ROM_BIG_PAGES;
    if(romsel.bplus_bits.ram) {
        usr[0x8]=ANDY_BIG_PAGE_INDEX+0;
        usr[0x9]=ANDY_BIG_PAGE_INDEX+1;
        usr[0xa]=ANDY_BIG_PAGE_INDEX+2;
        usr[0xb]=rom+3;

        memset(mos_pc_mem_big_pages,0,16);
        mos_pc_mem_big_pages[0xa]=1;
        mos_pc_mem_big_pages[0xc]=1;
        mos_pc_mem_big_pages[0xd]=1;
    } else {
        usr[0x8]=rom+0;
        usr[0x9]=rom+1;
        usr[0xa]=rom+2;
        usr[0xb]=rom+3;

        memset(mos_pc_mem_big_pages,0,16);
        mos_pc_mem_big_pages[0xc]=1;
        mos_pc_mem_big_pages[0xd]=1;
    }

    usr[0xc]=MOS_BIG_PAGE_INDEX+0;
    usr[0xd]=MOS_BIG_PAGE_INDEX+1;
    usr[0xe]=MOS_BIG_PAGE_INDEX+2;
    usr[0xf]=MOS_BIG_PAGE_INDEX+3;

    memcpy(mos+8,usr+8,8);

    *crt_shadow=acccon.bplus_bits.shadow!=0;
    *io=true;
}

static void ApplyDPOBPlus(ROMSEL *romsel,ACCCON *acccon,uint32_t dpo) {
    ApplyROMDPO(romsel,dpo);

    if(dpo&BBCMicroDebugPagingOverride_OverrideANDY) {
        romsel->bplus_bits.ram=!!(dpo&BBCMicroDebugPagingOverride_ANDY);
    }

    if(dpo&BBCMicroDebugPagingOverride_OverrideShadow) {
        acccon->bplus_bits.shadow=!!(dpo&BBCMicroDebugPagingOverride_Shadow);
    }
}


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
    (BBCMicroDebugPagingOverride_ROM|
     BBCMicroDebugPagingOverride_OverrideROM|
     BBCMicroDebugPagingOverride_ANDY|
     BBCMicroDebugPagingOverride_OverrideANDY|
     BBCMicroDebugPagingOverride_Shadow|
     BBCMicroDebugPagingOverride_OverrideShadow),//dpo_mask
    GetBigPageTypesBPlus(),
    &GetMemBigPageTablesBPlus,//get_mem_big_page_tables_fn,
    &ApplyDPOBPlus,//apply_dpo_fn
    0x8f,//romsel_mask,
    0x80,//acccon_mask,
    (BBCMicroTypeFlag_CanDisplayTeletext3c00|
     BBCMicroTypeFlag_HasShadowRAM),//flags
    {{0x00,0x1f},{0x40,0x7f},{0xc0,0xdf},},//sheila_cycle_stretch_regions
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static void GetMemBigPagesTablesMaster(uint8_t *usr,
                                       uint8_t *mos,
                                       uint8_t *mos_pc_mem_big_pages,
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
        usr[0]=MAIN_BIG_PAGE_INDEX+0;
        usr[1]=MAIN_BIG_PAGE_INDEX+1;
        usr[2]=MAIN_BIG_PAGE_INDEX+2;
        usr[3]=SHADOW_BIG_PAGE_INDEX+0;
        usr[4]=SHADOW_BIG_PAGE_INDEX+1;
        usr[5]=SHADOW_BIG_PAGE_INDEX+2;
        usr[6]=SHADOW_BIG_PAGE_INDEX+3;
        usr[7]=SHADOW_BIG_PAGE_INDEX+4;
    } else {
        usr[0]=MAIN_BIG_PAGE_INDEX+0;
        usr[1]=MAIN_BIG_PAGE_INDEX+1;
        usr[2]=MAIN_BIG_PAGE_INDEX+2;
        usr[3]=MAIN_BIG_PAGE_INDEX+3;
        usr[4]=MAIN_BIG_PAGE_INDEX+4;
        usr[5]=MAIN_BIG_PAGE_INDEX+5;
        usr[6]=MAIN_BIG_PAGE_INDEX+6;
        usr[7]=MAIN_BIG_PAGE_INDEX+7;
    }

    if((!acccon.m128_bits.y&&acccon.m128_bits.x)||
       (!acccon.m128_bits.y&&acccon.m128_bits.e))
    {
        mos[0]=MAIN_BIG_PAGE_INDEX+0;
        mos[1]=MAIN_BIG_PAGE_INDEX+1;
        mos[2]=MAIN_BIG_PAGE_INDEX+2;
        mos[3]=SHADOW_BIG_PAGE_INDEX+0;
        mos[4]=SHADOW_BIG_PAGE_INDEX+1;
        mos[5]=SHADOW_BIG_PAGE_INDEX+2;
        mos[6]=SHADOW_BIG_PAGE_INDEX+3;
        mos[7]=SHADOW_BIG_PAGE_INDEX+4;
    } else {
        mos[0]=MAIN_BIG_PAGE_INDEX+0;
        mos[1]=MAIN_BIG_PAGE_INDEX+1;
        mos[2]=MAIN_BIG_PAGE_INDEX+2;
        mos[3]=MAIN_BIG_PAGE_INDEX+3;
        mos[4]=MAIN_BIG_PAGE_INDEX+4;
        mos[5]=MAIN_BIG_PAGE_INDEX+5;
        mos[6]=MAIN_BIG_PAGE_INDEX+6;
        mos[7]=MAIN_BIG_PAGE_INDEX+7;
    }

    uint8_t rom=ROM0_BIG_PAGE_INDEX+romsel.bplus_bits.pr*NUM_ROM_BIG_PAGES;
    if(romsel.m128_bits.ram) {
        usr[0x8]=ANDY_BIG_PAGE_INDEX+0;
        usr[0x9]=rom+1;
        usr[0xa]=rom+2;
        usr[0xb]=rom+3;
    } else {
        usr[0x8]=rom+0;
        usr[0x9]=rom+1;
        usr[0xa]=rom+2;
        usr[0xb]=rom+3;
    }

    if(acccon.m128_bits.y) {
        usr[0xc]=HAZEL_BIG_PAGE_INDEX+0;
        usr[0xd]=HAZEL_BIG_PAGE_INDEX+1;
        usr[0xe]=MOS_BIG_PAGE_INDEX+2;
        usr[0xf]=MOS_BIG_PAGE_INDEX+3;

        memset(mos_pc_mem_big_pages,0,16);
    } else {
        usr[0xc]=MOS_BIG_PAGE_INDEX+0;
        usr[0xd]=MOS_BIG_PAGE_INDEX+1;
        usr[0xe]=MOS_BIG_PAGE_INDEX+2;
        usr[0xf]=MOS_BIG_PAGE_INDEX+3;

        memset(mos_pc_mem_big_pages,0,16);
        mos_pc_mem_big_pages[0xc]=1;
        mos_pc_mem_big_pages[0xd]=1;
    }

    memcpy(mos+8,usr+8,8);

    *io=acccon.m128_bits.tst==0;
    *crt_shadow=acccon.m128_bits.d!=0;
}

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
    GetBigPageTypesMaster(),
    &GetMemBigPagesTablesMaster,//get_mem_big_page_tables_fn,
    &ApplyDPOMaster,//apply_dpo_fn
    0x8f,//romsel_mask,
    0xff,//acccon_mask,
    (BBCMicroTypeFlag_HasShadowRAM|
     BBCMicroTypeFlag_HasRTC),//flags
    {{0x00,0x1f},{0x28,0x2b},{0x40,0x7f},},//sheila_cycle_stretch_regions
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

