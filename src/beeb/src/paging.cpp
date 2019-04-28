#include <shared/system.h>
#include <beeb/paging.h>
#include <beeb/6502.h>

#include <shared/enum_def.h>
#include <beeb/paging.inl>
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

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

PagingAccess::PagingAccess(const BBCMicroType *type,ROMSEL romsel,ACCCON acccon):
m_type(type),
m_romsel(romsel),
m_acccon(acccon)
{
    switch(m_type->type_id) {
        case BBCMicroTypeID_B:
            for(size_t i=0;i<16;++i) {
                m_pc_big_pages[i]=0;
            }
            break;

        case BBCMicroTypeID_BPlus:
            m_pc_big_pages[0xa]=1;
            m_pc_big_pages[0xc]=1;
            m_pc_big_pages[0xd]=1;
            break;

        case BBCMicroTypeID_Master:
            m_pc_big_pages[0xc]=1;
            m_pc_big_pages[0xd]=1;
            break;
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void PagingAccess::SetROMSEL(ROMSEL romsel) {
    if(romsel.value!=m_romsel.value) {
        m_romsel=romsel;
        m_dirty=true;
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void PagingAccess::SetACCCON(ACCCON acccon) {
    if(acccon.value!=m_acccon.value) {
        m_acccon=acccon;
        m_dirty=true;
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

const BigPageType *PagingAccess::GetBigPageTypeForAccess(M6502Word pc,M6502Word addr) const {
    if(!m_type) {
        return nullptr;
    }

    this->Update();

    if(addr.b.h>=0xfc&&addr.b.h<=0xfe&&m_io) {
        return &IO_BIG_PAGE_TYPE;
    } else {
        return m_big_pages[m_pc_big_pages[pc.p.p]][addr.p.p];
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void PagingAccess::Update() const {
    if(!m_dirty) {
        return;
    }

    switch(m_type->type_id) {
        case BBCMicroTypeID_B:
        {
            // 0x0000 - 0x7fff
            for(size_t i=0x0;i<0x8;++i) {
                m_big_pages[0][i]=&MAIN_RAM_BIG_PAGE_TYPE;
            }

            // 0x8000-0xbfff
            uint8_t rom;
            if(m_dpo&BBCMicroDebugPagingOverride_OverrideROM) {
                rom=m_dpo&BBCMicroDebugPagingOverride_ROM;
            } else {
                rom=m_romsel.b_bits.pr;
            }

            for(size_t i=0x8;i<0xc;++i) {
                m_big_pages[0][i]=&ROM_BIG_PAGE_TYPES[rom];
            }

            // 0xc000-0xffff
            for(size_t i=0xc;i<0x10;++i) {
                m_big_pages[0][i]=&MOS_BIG_PAGE_TYPE;
            }

            m_io=true;
        }
            break;

        case BBCMicroTypeID_BPlus:
        {
            // 0x0000-0x2fff
            for(size_t i=0x0;i<0x3;++i) {
                m_big_pages[0][i]=m_big_pages[1][i]=&MAIN_RAM_BIG_PAGE_TYPE;
            }

            // 0x3000-0x7fff
            for(size_t i=0x3;i<0x8;++i) {
                m_big_pages[0][i]=&MAIN_RAM_BIG_PAGE_TYPE;
                m_big_pages[1][i]=&SHADOW_RAM_BIG_PAGE_TYPE;
            }

            // 0x8000-0xafff
            uint8_t rom;
            if(m_dpo&BBCMicroDebugPagingOverride_OverrideROM) {
                rom=m_dpo&BBCMicroDebugPagingOverride_ROM;
            } else {
                rom=m_romsel.bplus_bits.pr;
            }

            bool ram;
            if(m_dpo&BBCMicroDebugPagingOverride_OverrideANDY) {
                ram=!!(m_dpo&BBCMicroDebugPagingOverride_ANDY);
            } else {
                ram=!!m_romsel.bplus_bits.ram;
            }

            for(size_t i=0x8;i<0xb;++i) {
                const BigPageType *type;
                if(ram) {
                    type=&ANDY_BIG_PAGE_TYPE;
                } else {
                    type=&ROM_BIG_PAGE_TYPES[rom];
                }

                m_big_pages[0][i]=m_big_pages[1][i]=type;
            }

            m_big_pages[0][0xb]=m_big_pages[1][0xb]=&ROM_BIG_PAGE_TYPES[rom];

            // 0xc000-0xffff
            for(size_t i=0xc;i<0x10;++i) {
                m_big_pages[0][i]=m_big_pages[1][i]=&MOS_BIG_PAGE_TYPE;
            }

            m_io=true;
        }
            break;

        case BBCMicroTypeID_Master:
        {
            // 0x0000-0x2fff
            for(size_t i=0x0;i<0x3;++i) {
                m_big_pages[0][i]=m_big_pages[1][i]=&MAIN_RAM_BIG_PAGE_TYPE;
            }

            bool shadow;
            if(m_dpo&BBCMicroDebugPagingOverride_OverrideShadow) {
                shadow=!!(m_dpo&BBCMicroDebugPagingOverride_Shadow);
            } else {
                shadow=!!m_acccon.m128_bits.e;
            }

            bool hazel;
            if(m_dpo&BBCMicroDebugPagingOverride_OverrideHAZEL) {
                hazel=!!(m_dpo&BBCMicroDebugPagingOverride_HAZEL);
            } else {
                hazel=!!m_acccon.m128_bits.y;
            }

            // 0x3000-0x7fff
            for(size_t i=0x3;i<0x8;++i) {
                if(shadow) {
                    m_big_pages[0][i]=&SHADOW_RAM_BIG_PAGE_TYPE;
                } else {
                    m_big_pages[0][i]=&MAIN_RAM_BIG_PAGE_TYPE;
                }

                // Not sure how sensible the override logic is here.
                if(hazel?!!m_acccon.m128_bits.x:shadow) {
                    m_big_pages[1][i]=&SHADOW_RAM_BIG_PAGE_TYPE;
                } else {
                    m_big_pages[1][i]=&MAIN_RAM_BIG_PAGE_TYPE;
                }
            }

            // 0x8000-0xbfff
            uint8_t rom;
            if(m_dpo&BBCMicroDebugPagingOverride_OverrideROM) {
                rom=m_dpo&BBCMicroDebugPagingOverride_ROM;
            } else {
                rom=m_romsel.m128_bits.pm;
            }

            for(size_t i=0x8;i<0xc;++i) {
                m_big_pages[0][i]=m_big_pages[1][i]=&ROM_BIG_PAGE_TYPES[m_romsel.m128_bits.pm];
            }

            // 0x8000 (ANDY)
            bool ram;
            if(m_dpo&BBCMicroDebugPagingOverride_OverrideANDY) {
                ram=!!(m_dpo&BBCMicroDebugPagingOverride_ANDY);
            } else {
                ram=!!m_romsel.m128_bits.ram;
            }
            if(m_romsel.m128_bits.ram) {
                m_big_pages[0][0x8]=m_big_pages[1][0x8]=&ANDY_BIG_PAGE_TYPE;
            }

            // 0xc000-0xffff
            for(size_t i=0xc;i<0x10;++i) {
                m_big_pages[0][i]=m_big_pages[1][i]=&MOS_BIG_PAGE_TYPE;
            }

            // 0xc000-0xdfff (HAZEL)
            if(hazel) {
                for(size_t i=0xc;i<0xe;++i) {
                    m_big_pages[0][i]=m_big_pages[1][i]=&HAZEL_BIG_PAGE_TYPE;
                }
            }

            if(m_dpo&BBCMicroDebugPagingOverride_OverrideOS) {
                m_io=!(m_dpo&BBCMicroDebugPagingOverride_OS);
            } else {
                m_io=m_acccon.m128_bits.tst==0;
            }
        }
            break;
    }

    m_dirty=false;
}
