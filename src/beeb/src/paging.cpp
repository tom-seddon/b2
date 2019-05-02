#include <shared/system.h>
#include <shared/debug.h>
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
const BigPageType INVALID_BIG_PAGE_TYPE={'?',"Invalid"};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

PagingAccess::PagingAccess(const BBCMicroType *type,ROMSEL romsel,ACCCON acccon):
m_type(type),
m_romsel(romsel),
m_acccon(acccon)
{
    this->Update();
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

void PagingAccess::SetDebugPageOverrides(uint32_t dpo) {
    if(dpo!=m_dpo) {
        m_dpo=dpo;
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
        uint8_t big_page=m_big_pages[m_pc_big_pages[pc.p.p]][addr.p.p];
        ASSERT(big_page<NUM_BIG_PAGES);
        return m_type->big_page_types[big_page];
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void PagingAccess::Update() const {
    if(!m_dirty) {
        return;
    }

    ROMSEL romsel=m_romsel;
    ACCCON acccon=m_acccon;
    (*m_type->apply_dpo_fn)(&romsel,&acccon,m_dpo);

    bool crtc_shadow;
    (*m_type->get_mem_big_page_tables_fn)(m_big_pages[0],
                                          m_big_pages[1],
                                          m_pc_big_pages,
                                          &m_io,
                                          &crtc_shadow,
                                          m_romsel,
                                          m_acccon);

    m_dirty=false;
}
