#include <shared/system.h>
#include <shared/log.h>
#include <shared/debug.h>
#include <beeb/BBCMicro.h>
#include <beeb/VideoULA.h>
#include <beeb/teletext.h>
#include <beeb/OutputData.h>
#include <beeb/sound.h>
#include <beeb/video.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <beeb/conf.h>
#include <errno.h>
#include <beeb/keys.h>
#include <beeb/conf.h>
#include <beeb/DiscInterface.h>
#include <beeb/ExtMem.h>
#include <beeb/Trace.h>
#include <memory>
#include <vector>
#include <beeb/DiscImage.h>
#include <map>
#include <limits.h>
#include <algorithm>
#include <inttypes.h>
#include <beeb/BeebLink.h>

#include <shared/enum_decl.h>
#include "BBCMicro_private.inl"
#include <shared/enum_end.h>

#include <shared/enum_def.h>
#include <beeb/BBCMicro.inl>
#include "BBCMicro_private.inl"
#include <shared/enum_end.h>

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static const std::shared_ptr<DiscImage> NULL_DISCIMAGE_PTR;
static std::map<DiscDriveType,std::array<std::vector<float>,DiscDriveSound_EndValue>> g_disc_drive_sounds;

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// The key to press to start the paste going.
static const BeebKey PASTE_START_KEY=BeebKey_Space;

// The corresponding char, so it can be removed when copying the BASIC
// listing.
const char BBCMicro::PASTE_START_CHAR=' ';

#if BBCMICRO_TRACE
const TraceEventType BBCMicro::INSTRUCTION_EVENT("BBCMicroInstruction",sizeof(InstructionTraceEvent));
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if BBCMICRO_DEBUGGER
// The async call thunk lives in an undefined area of FRED.
static const M6502Word ASYNC_CALL_THUNK_ADDR={0xfc50};
static const int ASYNC_CALL_TIMEOUT=1000000;
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#define VDU_RAM_OFFSET (0x8000u+0u)
#define NUM_VDU_RAM_PAGES (0x10u)
#define FS_RAM_OFFSET (0x8000+0x1000u)
#define NUM_FS_RAM_PAGES (0x20u)

#if BBCMICRO_DEBUGGER

// Layout of the m_debug_flags array.
//
// (This addressing scheme probably wants a proper name, but so far it
// doesn't really have one.)

// RAM area layout is same as the RAM array.
//static constexpr size_t DEBUG_MAIN_RAM_PAGE=0;
static constexpr size_t DEBUG_BPLUS_RAM_PAGE=VDU_RAM_OFFSET>>8;
static constexpr size_t DEBUG_VDU_RAM_PAGE=VDU_RAM_OFFSET>>8;
static constexpr size_t DEBUG_FS_RAM_PAGE=FS_RAM_OFFSET>>8;
static constexpr size_t DEBUG_SHADOW_RAM_PAGE=DEBUG_FS_RAM_PAGE+NUM_FS_RAM_PAGES;

// The ROMs come one after the other.
static constexpr size_t DEBUG_ROM0_PAGE=256;
static constexpr size_t DEBUG_NUM_ROM_PAGES=64;

// The OS ROM comes last.
static constexpr size_t DEBUG_OS_PAGE=DEBUG_ROM0_PAGE+16*DEBUG_NUM_ROM_PAGES;

// 1 char = 4K
const char DEBUG_PAGE_CODES[]=
"rrrrrrrrrrrrrrrr"//$0000
"rrrrrrrrrrrrrrrr"//$1000
"rrrrrrrrrrrrrrrr"//$2000
"rrrrrrrrrrrrrrrr"//$3000
"rrrrrrrrrrrrrrrr"//$4000
"rrrrrrrrrrrrrrrr"//$5000
"rrrrrrrrrrrrrrrr"//$6000
"rrrrrrrrrrrrrrrr"//$7000
"xxxxxxxxxxxxxxxx"//extra 12K RAM
"xxxxxxxxxxxxxxxx"//extra 12K RAM
"xxxxxxxxxxxxxxxx"//extra 12K RAM
"ssssssssssssssss"//$3000 (shadow RAM)
"ssssssssssssssss"//$4000 (shadow RAM)
"ssssssssssssssss"//$5000 (shadow RAM)
"ssssssssssssssss"//$6000 (shadow RAM)
"ssssssssssssssss"//$7000 (shadow RAM)
"0000000000000000"//$8000 (rom 0)
"0000000000000000"//$9000 (rom 0)
"0000000000000000"//$a000 (rom 0)
"0000000000000000"//$b000 (rom 0)
"1111111111111111"//$8000 (rom 1)
"1111111111111111"//$9000 (rom 1)
"1111111111111111"//$a000 (rom 1)
"1111111111111111"//$b000 (rom 1)
"2222222222222222"//$8000 (rom 2)
"2222222222222222"//$9000 (rom 2)
"2222222222222222"//$a000 (rom 2)
"2222222222222222"//$b000 (rom 2)
"3333333333333333"//$8000 (rom 3)
"3333333333333333"//$9000 (rom 3)
"3333333333333333"//$a000 (rom 3)
"3333333333333333"//$b000 (rom 3)
"4444444444444444"//$8000 (rom 4)
"4444444444444444"//$9000 (rom 4)
"4444444444444444"//$a000 (rom 4)
"4444444444444444"//$b000 (rom 4)
"5555555555555555"//$8000 (rom 5)
"5555555555555555"//$9000 (rom 5)
"5555555555555555"//$a000 (rom 5)
"5555555555555555"//$b000 (rom 5)
"6666666666666666"//$8000 (rom 6)
"6666666666666666"//$9000 (rom 6)
"6666666666666666"//$a000 (rom 6)
"6666666666666666"//$b000 (rom 6)
"7777777777777777"//$8000 (rom 7)
"7777777777777777"//$9000 (rom 7)
"7777777777777777"//$a000 (rom 7)
"7777777777777777"//$b000 (rom 7)
"8888888888888888"//$8000 (rom 8)
"8888888888888888"//$9000 (rom 8)
"8888888888888888"//$a000 (rom 8)
"8888888888888888"//$b000 (rom 8)
"9999999999999999"//$8000 (rom 9)
"9999999999999999"//$9000 (rom 9)
"9999999999999999"//$a000 (rom 9)
"9999999999999999"//$b000 (rom 9)
"aaaaaaaaaaaaaaaa"//$8000 (rom a)
"aaaaaaaaaaaaaaaa"//$9000 (rom a)
"aaaaaaaaaaaaaaaa"//$a000 (rom a)
"aaaaaaaaaaaaaaaa"//$b000 (rom a)
"bbbbbbbbbbbbbbbb"//$8000 (rom b)
"bbbbbbbbbbbbbbbb"//$9000 (rom b)
"bbbbbbbbbbbbbbbb"//$a000 (rom b)
"bbbbbbbbbbbbbbbb"//$b000 (rom b)
"cccccccccccccccc"//$8000 (rom c)
"cccccccccccccccc"//$9000 (rom c)
"cccccccccccccccc"//$a000 (rom c)
"cccccccccccccccc"//$b000 (rom c)
"dddddddddddddddd"//$8000 (rom d)
"dddddddddddddddd"//$9000 (rom d)
"dddddddddddddddd"//$a000 (rom d)
"dddddddddddddddd"//$b000 (rom d)
"eeeeeeeeeeeeeeee"//$8000 (rom e)
"eeeeeeeeeeeeeeee"//$9000 (rom e)
"eeeeeeeeeeeeeeee"//$a000 (rom e)
"eeeeeeeeeeeeeeee"//$b000 (rom e)
"ffffffffffffffff"//$8000 (rom f)
"ffffffffffffffff"//$9000 (rom f)
"ffffffffffffffff"//$a000 (rom f)
"ffffffffffffffff"//$b000 (rom f)
"oooooooooooooooo"//$c000 (OS)
"oooooooooooooooo"//$d000 (OS)
"oooooooooooooooo"//$e000 (OS)
"ooooooooooooiiio"//$f000 (OS)
;

static const BBCMicro::DebugState::ByteDebugFlags DUMMY_BYTE_DEBUG_FLAGS={};

#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static const BBCMicro::WriteMMIOFn g_R6522_write_fns[16]={
    &R6522::Write0,&R6522::Write1,&R6522::Write2,&R6522::Write3,&R6522::Write4,&R6522::Write5,&R6522::Write6,&R6522::Write7,
    &R6522::Write8,&R6522::Write9,&R6522::WriteA,&R6522::WriteB,&R6522::WriteC,&R6522::WriteD,&R6522::WriteE,&R6522::WriteF,
};

static const BBCMicro::ReadMMIOFn g_R6522_read_fns[16]={
    &R6522::Read0,&R6522::Read1,&R6522::Read2,&R6522::Read3,&R6522::Read4,&R6522::Read5,&R6522::Read6,&R6522::Read7,
    &R6522::Read8,&R6522::Read9,&R6522::ReadA,&R6522::ReadB,&R6522::ReadC,&R6522::ReadD,&R6522::ReadE,&R6522::ReadF,
};

static const BBCMicro::WriteMMIOFn g_WD1770_write_fns[]={&WD1770::Write0,&WD1770::Write1,&WD1770::Write2,&WD1770::Write3,};
static const BBCMicro::ReadMMIOFn g_WD1770_read_fns[]={&WD1770::Read0,&WD1770::Read1,&WD1770::Read2,&WD1770::Read3,};

static const uint8_t g_unmapped_rom_reads[256]={0,};
static uint8_t g_rom_writes[256];

const uint16_t BBCMicro::SCREEN_WRAP_ADJUSTMENTS[]={
    0x4000>>3,
    0x2000>>3,
    0x5000>>3,
    0x2800>>3
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

BBCMicro::MemoryPages::MemoryPages() {
#if BBCMICRO_DEBUGGER
    for(int i=0;i<256;++i) {
        this->debug_page_index[i]=DebugState::INVALID_PAGE_INDEX;
    }
#endif
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

BBCMicro::State::State(const BBCMicroType type,
                       const std::vector<uint8_t> &nvram_contents,
                       const tm *rtc_time,
                       uint64_t initial_num_2MHz_cycles):
num_2MHz_cycles(initial_num_2MHz_cycles)
{
    switch(type) {
    default:
        ASSERT(false);
        // fall through
    case BBCMicroType_B:
        this->ram_buffer.resize(32768);
        M6502_Init(&this->cpu,&M6502_nmos6502_config);
        break;

    case BBCMicroType_BPlus:
        this->ram_buffer.resize(65536);
        M6502_Init(&this->cpu,&M6502_nmos6502_config);
        break;

    case BBCMicroType_Master:
        this->ram_buffer.resize(65536);
        M6502_Init(&this->cpu,&M6502_cmos6502_config);
        this->rtc.SetRAMContents(nvram_contents);

        if(rtc_time) {
            this->rtc.SetTime(rtc_time);
        }

        break;
    }

    //for(int i=0;i<NUM_DRIVES;++i) {
    //    DiscDrive_Init(&this->drives[i],i);
    //}

    this->sn76489.Reset();
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

BBCMicro::BBCMicro(BBCMicroType type,
                   const DiscInterfaceDef *def,
                   const std::vector<uint8_t> &nvram_contents,
                   const tm *rtc_time,
                   bool video_nula,
                   bool ext_mem,
                   BeebLinkHandler *beeblink_handler,
                   uint64_t initial_num_2MHz_cycles):
m_state(type,nvram_contents,rtc_time,initial_num_2MHz_cycles),
m_type(type),
m_disc_interface(def?def->create_fun():nullptr),
m_video_nula(video_nula),
m_ext_mem(ext_mem),
m_beeblink_handler(beeblink_handler)
{
    this->InitStuff();
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

BBCMicro::BBCMicro(const BBCMicro &src):
    m_state(src.m_state),
    m_type(src.m_type),
    m_disc_interface(src.m_disc_interface?src.m_disc_interface->Clone():nullptr),
    m_video_nula(src.m_video_nula),
    m_ext_mem(src.m_ext_mem)
{
    ASSERT(src.GetCloneImpediments()==0);

    for(int i=0;i<NUM_DRIVES;++i) {
        std::shared_ptr<DiscImage> disc_image=DiscImage::Clone(src.GetDiscImage(i));
        this->SetDiscImage(i,std::move(disc_image));
    }

    this->InitStuff();
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

BBCMicro::~BBCMicro() {
#if BBCMICRO_TRACE
    this->StopTrace();
#endif

    delete m_disc_interface;

    //for(int i=0;i<16;++i) {
    //    SetROMContents(this,&m_state.roms[i],nullptr,0);
    //}

    //SetROMContents(this,&m_state.os,nullptr,0);

    delete m_shadow_pages;
    m_shadow_pages=nullptr;

    delete[] m_pc_pages;
    m_pc_pages=nullptr;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

uint32_t BBCMicro::GetCloneImpediments() const {
    uint32_t result=0;

    for(int i=0;i<NUM_DRIVES;++i) {
        if(!!m_disc_images[i]) {
            if(!m_disc_images[i]->CanClone()) {
                result|=(uint32_t)BBCMicroCloneImpediment_Drive0<<i;
            }
        }
    }

    if(m_beeblink_handler) {
        result|=BBCMicroCloneImpediment_BeebLink;
    }

    return result;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

std::unique_ptr<BBCMicro> BBCMicro::Clone() const {
    if(this->GetCloneImpediments()!=0) {
        return nullptr;
    }

    return std::unique_ptr<BBCMicro>(new BBCMicro(*this));
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if BBCMICRO_TRACE
void BBCMicro::SetTrace(std::shared_ptr<Trace> trace,uint32_t trace_flags) {
    m_trace_ptr=std::move(trace);
    m_trace=m_trace_ptr.get();
    m_trace_current_instruction=nullptr;
    m_trace_flags=trace_flags;

    if(m_trace) {
        m_trace->SetTime(&m_state.num_2MHz_cycles);

        m_trace->AllocM6502ConfigEvent(m_state.cpu.config);
    }

    m_state.fdc.SetTrace(trace_flags&BBCMicroTraceFlag_1770?m_trace:nullptr);
    m_state.rtc.SetTrace(trace_flags&BBCMicroTraceFlag_RTC?m_trace:nullptr);
    m_state.crtc.SetTrace(
        (trace_flags&(BBCMicroTraceFlag_6845VSync|BBCMicroTraceFlag_6845Scanlines))?m_trace:nullptr,
        !!(trace_flags&BBCMicroTraceFlag_6845Scanlines));
    m_state.system_via.SetTrace(trace_flags&BBCMicroTraceFlag_SystemVIA?m_trace:nullptr);
    m_state.user_via.SetTrace(trace_flags&BBCMicroTraceFlag_UserVIA?m_trace:nullptr);
    m_state.video_ula.SetTrace(trace_flags&BBCMicroTraceFlag_VideoULA?m_trace:nullptr);
    m_state.sn76489.SetTrace(trace_flags&BBCMicroTraceFlag_SN76489?m_trace:nullptr);

    if(!!m_beeblink) {
        m_beeblink->SetTrace(trace_flags&BBCMicroTraceFlag_BeebLink?m_trace:nullptr);
    }

    this->UpdateCPUDataBusFn();
}
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BBCMicro::SetPages(uint8_t page_,size_t num_pages,
                        const uint8_t *read_data,size_t read_page_stride,
                        uint8_t *write_data,size_t write_page_stride
#if BBCMICRO_DEBUGGER////////////////////////////////////////////////////////<--note
                        ,uint16_t debug_page//////////////////////<--note
#endif///////////////////////////////////////////////////////////////////////<--note
)////////////////////////////////////////////////////////////////////////////<--note
{
    ASSERT(read_page_stride==256||read_page_stride==0);
    ASSERT(write_page_stride==256||write_page_stride==0);
    uint8_t page=page_;

    if(m_shadow_pages) {
        for(size_t i=0;i<num_pages;++i) {
            m_shadow_pages->r[page]=m_pages.r[page]=read_data;
            read_data+=read_page_stride;

            m_shadow_pages->w[page]=m_pages.w[page]=write_data;
            write_data+=write_page_stride;

#if BBCMICRO_DEBUGGER

            m_shadow_pages->debug_page_index[page]=m_pages.debug_page_index[page]=debug_page;

            if(m_debug) {
                m_shadow_pages->debug[page]=m_pages.debug[page]=m_debug->pages[debug_page];
            }

            ++debug_page;
#endif

            ++page;
        }
    } else {
        for(size_t i=0;i<num_pages;++i) {
            m_pages.r[page]=read_data;
            read_data+=read_page_stride;

            m_pages.w[page]=write_data;
            write_data+=write_page_stride;

#if BBCMICRO_DEBUGGER
            m_pages.debug_page_index[page]=debug_page;

            if(m_debug) {
                m_pages.debug[page]=m_debug->pages[debug_page];
            }

            ++debug_page;
#endif

            ++page;
        }
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BBCMicro::SetOSPages(uint8_t dest_page,uint8_t src_page,uint8_t num_pages) {
    if(!!m_state.os_buffer) {
        const uint8_t *data=m_state.os_buffer->data();
        this->SetPages(dest_page,num_pages,
                       data+src_page*256,256,
                       g_rom_writes,0
#if BBCMICRO_DEBUGGER
                       ,DEBUG_OS_PAGE
#endif
        );
    } else {
        this->SetPages(dest_page,num_pages,
                       g_unmapped_rom_reads,0,
                       g_rom_writes,0
#if BBCMICRO_DEBUGGER
                       ,DEBUG_OS_PAGE
#endif
        );
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if BBCMICRO_DEBUGGER
#define SET_READ_ONLY_PAGES(PAGE,NUM_PAGES,DATA,DEBUG) SetPages((PAGE),(NUM_PAGES),(DATA),256,g_rom_writes,0,(DEBUG))
#define SET_READWRITE_PAGES(PAGE,NUM_PAGES,DATA,DEBUG) SetPages((PAGE),(NUM_PAGES),(DATA),256,(DATA),256,(DEBUG))
#else
#define SET_READ_ONLY_PAGES(PAGE,NUM_PAGES,DATA,DEBUG) SetPages((PAGE),(NUM_PAGES),(DATA),256,g_rom_writes,0)
#define SET_READWRITE_PAGES(PAGE,NUM_PAGES,DATA,DEBUG) SetPages((PAGE),(NUM_PAGES),(DATA),256,(DATA),256)
#endif

void BBCMicro::SetROMPages(uint8_t bank,uint8_t page,size_t src_page,size_t num_pages) {
    ASSERT(bank<16);
    if(!!m_state.sideways_rom_buffers[bank]) {
        const uint8_t *data=m_state.sideways_rom_buffers[bank]->data()+src_page*256;
        this->SET_READ_ONLY_PAGES(page,num_pages,data,DEBUG_ROM0_PAGE+bank*DEBUG_NUM_ROM_PAGES);
    } else if(!m_state.sideways_ram_buffers[bank].empty()) {
        uint8_t *data=m_state.sideways_ram_buffers[bank].data()+src_page*256;
        this->SET_READWRITE_PAGES(page,num_pages,data,DEBUG_ROM0_PAGE+bank*DEBUG_NUM_ROM_PAGES);
    } else {
        this->SET_READ_ONLY_PAGES(page,num_pages,g_unmapped_rom_reads,DEBUG_ROM0_PAGE+bank*DEBUG_NUM_ROM_PAGES);
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BBCMicro::UpdateBROMSELPages(BBCMicro *m) {
    m->SetROMPages(m->m_state.romsel.b_bits.pr,0x80,0x00,0x40);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BBCMicro::UpdateBACCCONPages(BBCMicro *m,const ACCCON *old) {
    (void)m,(void)old;

    // This function only exists to save on a couple of NULL checks.
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BBCMicro::UpdateBPlusROMSELPages(BBCMicro *m) {
    if(m->m_state.romsel.bplus_bits.ram) {
        m->SET_READWRITE_PAGES(0x80,0x30,m->m_ram+0x8000,DEBUG_BPLUS_RAM_PAGE);
        m->SetROMPages(m->m_state.romsel.bplus_bits.pr,0xb0,0x30,0x10);
    } else {
        m->SetROMPages(m->m_state.romsel.bplus_bits.pr,0x80,0x00,0x40);
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BBCMicro::UpdateBPlusACCCONPages(BBCMicro *m,const ACCCON *old) {
    (void)old;

    const MemoryPages *pages;
    if(m->m_state.acccon.bplus_bits.shadow) {
        pages=m->m_shadow_pages;
        m->m_state.shadow_select_mask=0x8000;
    } else {
        pages=&m->m_pages;
        m->m_state.shadow_select_mask=0x0000;
    }

    // This is what BeebEm does! Also mentioned in passing in
    // http://beebwiki.mdfs.net/Paged_ROM - the NAUG on the other hand
    // says nothing.
    for(uint8_t i=0xa0;i<0xb0;++i) {
        m->m_pc_pages[i]=pages;
    }

    for(uint8_t i=0xc0;i<0xe0;++i) {
        m->m_pc_pages[i]=pages;
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BBCMicro::UpdateMaster128ROMSELPages(BBCMicro *m) {
    if(m->m_state.romsel.m128_bits.ram) {
        m->SET_READWRITE_PAGES(0x80,NUM_VDU_RAM_PAGES,m->m_ram+VDU_RAM_OFFSET,DEBUG_VDU_RAM_PAGE);
        m->SetROMPages(m->m_state.romsel.m128_bits.pm,0x80+NUM_VDU_RAM_PAGES,NUM_VDU_RAM_PAGES,0x40-NUM_VDU_RAM_PAGES);
    } else {
        m->SetROMPages(m->m_state.romsel.m128_bits.pm,0x80,0x00,0x40);
    }
}

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
// Usr Shadow = E
//
// MOS Shadow = (Y AND X) OR (NOT Y AND E)

bool BBCMicro::DoesMOSUseShadow(ACCCON acccon) {
    if(acccon.m128_bits.y) {
        return acccon.m128_bits.x;
    } else {
        return acccon.m128_bits.e;
    }
}

void BBCMicro::UpdateMaster128ACCCONPages(BBCMicro *m,const ACCCON *old_) {
    ACCCON old;
    if(old_) {
        old=*old_;
    } else {
        old.value=~m->m_state.acccon.value;
    }

    ACCCON diff;
    diff.value=m->m_state.acccon.value^old.value;

    if(diff.m128_bits.y) {
        ASSERT(m->m_state.acccon.m128_bits.y!=old.m128_bits.y);
        if(m->m_state.acccon.m128_bits.y) {
            // 8K FS RAM at 0xC000
            m->SET_READWRITE_PAGES(0xc0,NUM_FS_RAM_PAGES,
                                   m->m_ram+FS_RAM_OFFSET,
                                   DEBUG_FS_RAM_PAGE);
        } else {
            // MOS at 0xC0000
            m->SetOSPages(0xc0,0x00,NUM_FS_RAM_PAGES);
        }
    }

    int usr_shadow=!!m->m_state.acccon.m128_bits.x;
    int mos_shadow=DoesMOSUseShadow(m->m_state.acccon);

    int old_usr_shadow=!!old.m128_bits.x;
    int old_mos_shadow=DoesMOSUseShadow(old);

    if(usr_shadow!=old_usr_shadow||mos_shadow!=old_mos_shadow) {
        const MemoryPages *usr_pages;
        if(usr_shadow) {
            usr_pages=m->m_shadow_pages;
        } else {
            usr_pages=&m->m_pages;
        }

        const MemoryPages *mos_pages;
        if(mos_shadow) {
            mos_pages=m->m_shadow_pages;
        } else {
            mos_pages=&m->m_pages;
        }

        size_t page=0;

        while(page<0xc0) {
            m->m_pc_pages[page++]=usr_pages;
        }

        while(page<0xe0) {
            m->m_pc_pages[page++]=mos_pages;
        }

        while(page<0x100) {
            m->m_pc_pages[page++]=usr_pages;
        }
    }

    if(m->m_state.acccon.m128_bits.d) {
        m->m_state.shadow_select_mask=0x8000;
    } else {
        m->m_state.shadow_select_mask=0;
    }

    if(diff.m128_bits.tst) {
        if(m->m_state.acccon.m128_bits.tst) {
            for(int i=0;i<3;++i) {
                m->m_rmmio_fns[i]=m->m_rom_rmmio_fns.data();
                m->m_mmio_fn_contexts[i]=m->m_rom_mmio_fn_contexts.data();
                m->m_mmio_stretch[i]=m->m_rom_mmio_stretch.data();
            }
        } else {
            for(int i=0;i<3;++i) {
                m->m_rmmio_fns[i]=m->m_hw_rmmio_fns[i].data();
                m->m_mmio_fn_contexts[i]=m->m_hw_mmio_fn_contexts[i].data();
                m->m_mmio_stretch[i]=m->m_hw_mmio_stretch[i].data();
            }
        }
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BBCMicro::InitROMPages() {
    this->SetOSPages(0xc0,0x00,0x40);

    (*m_update_romsel_pages_fn)(this);

    // Need to check ACCCON again - updating the OS pages may have
    // made a mess on the M128.
    (*m_update_acccon_pages_fn)(this,NULL);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BBCMicro::Write1770ControlRegister(void *m_,M6502Word a,uint8_t value) {
    auto m=(BBCMicro *)m_;
    (void)a;

    ASSERT(m->m_disc_interface);
    m->m_state.disc_control=m->m_disc_interface->GetControlFromByte(value);

#if BBCMICRO_TRACE
    if(m->m_trace) {
        m->m_trace->AllocStringf("1770 - Control Register: Reset=%d; DDEN=%d; drive=%d, side=%d\n",m->m_state.disc_control.reset,m->m_state.disc_control.dden,m->m_state.disc_control.drive,m->m_state.disc_control.side);
    }
#endif

    LOGF(1770,"Write control register: 0x%02X: Reset=%d; DDEN=%d; drive=%d, side=%d\n",value,m->m_state.disc_control.reset,m->m_state.disc_control.dden,m->m_state.disc_control.drive,m->m_state.disc_control.side);

    if(m->m_state.disc_control.reset) {
        m->m_state.fdc.Reset();
    }

    m->m_state.fdc.SetDDEN(!!m->m_state.disc_control.dden);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

uint8_t BBCMicro::Read1770ControlRegister(void *m_,M6502Word a) {
    auto m=(BBCMicro *)m_;
    (void)a;

    ASSERT(m->m_disc_interface);

    uint8_t value=m->m_disc_interface->GetByteFromControl(m->m_state.disc_control);
    return value;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if BBCMICRO_TRACE
void BBCMicro::TracePortB(SystemVIAPB pb) {
    Log log("",m_trace->GetLogPrinter(1000));

    log.f("PORTB - PB = $%02X (%%%s): ",pb.value,BINARY_BYTE_STRINGS[pb.value]);

    if(m_type==BBCMicroType_Master) {
        log.f("RTC AS=%u; RTC CS=%u; ",pb.m128_bits.rtc_address_strobe,pb.m128_bits.rtc_chip_select);
    }

    const char *name=nullptr;
    bool value=pb.bits.latch_value;

    switch(pb.bits.latch_index) {
        case 0:
            name="Sound Write";
            value=!value;
        print_bool:;
            log.f("%s=%s\n",name,BOOL_STR(value));
            break;

        case 1:
            name=m_type==BBCMicroType_Master?"RTC Read":"Speech Read";
            goto print_bool;

        case 2:
            name=m_type==BBCMicroType_Master?"RTC DS":"Speech Write";
            goto print_bool;

        case 3:
            name="KB Read";
            goto print_bool;

        case 4:
        case 5:
            log.f("Screen Wrap Adjustment=$%04x\n",SCREEN_WRAP_ADJUSTMENTS[m_state.addressable_latch.bits.screen_base]);
            break;

        case 6:
            name="Caps Lock LED";
            goto print_bool;

        case 7:
            name="Shift Lock LED";
            goto print_bool;
    }

    m_trace->FinishLog(&log);

    //Trace_AllocStringf(m_trace,
    //    "PORTB - PB = $%02X (
    //    "PORTB - PB = $%02X (Latch = $%02X - Snd=%d; Kb=%d; Caps=%d; Shf=%d; RTCdat=%d; RTCrd=%d) (RTCsel=%d; RTCaddr=%d)",
    //    pb.value,
    //    m_state.addressable_latch.value,
    //    !m_state.addressable_latch.bits.not_sound_write,
    //    !m_state.addressable_latch.bits.not_kb_write,
    //    m_state.addressable_latch.bits.caps_lock_led,
    //    m_state.addressable_latch.bits.shift_lock_led,
    //    m_state.addressable_latch.m128_bits.rtc_data_strobe,
    //    m_state.addressable_latch.m128_bits.rtc_read,
    //    pb.m128_bits.rtc_chip_select,
    //    pb.m128_bits.rtc_address_strobe);
}
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BBCMicro::WriteUnmappedMMIO(void *m_,M6502Word a,uint8_t value) {
    (void)m_,(void)a,(void)value;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

uint8_t BBCMicro::ReadUnmappedMMIO(void *m_,M6502Word a) {
    (void)m_,(void)a;

    return 0;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

uint8_t BBCMicro::ReadROMMMIO(void *m_,M6502Word a) {
    auto m=(BBCMicro *)m_;

    return m->m_pages.r[a.b.h][a.b.l];
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

uint8_t BBCMicro::ReadROMSEL(void *m_,M6502Word a) {
    auto m=(BBCMicro *)m_;
    (void)a;

    return m->m_state.romsel.value;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BBCMicro::WriteROMSEL(void *m_,M6502Word a,uint8_t value) {
    auto m=(BBCMicro *)m_;
    (void)a;

    if((m->m_state.romsel.value^value)&m->m_romsel_mask) {
        m->m_state.romsel.value=value&m->m_romsel_mask;

        (*m->m_update_romsel_pages_fn)(m);
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

uint8_t BBCMicro::ReadACCCON(void *m_,M6502Word a) {
    auto m=(BBCMicro *)m_;
    (void)a;

    return m->m_state.acccon.value;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BBCMicro::WriteACCCON(void *m_,M6502Word a,uint8_t value) {
    auto m=(BBCMicro *)m_;
    (void)a;

    if((m->m_state.acccon.value^value)&m->m_acccon_mask) {
        ACCCON old=m->m_state.acccon;

        m->m_state.acccon.value=value&m->m_acccon_mask;

        (*m->m_update_acccon_pages_fn)(m,&old);
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

BBCMicroType BBCMicro::GetType() const {
    return m_type;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

const uint64_t *BBCMicro::GetNum2MHzCycles() const {
    return &m_state.num_2MHz_cycles;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

uint8_t BBCMicro::GetKeyState(BeebKey key) {
    ASSERT(key>=0&&(int)key<128);

    uint8_t *column=&m_state.key_columns[key&0x0f];
    uint8_t mask=1<<(key>>4);

    return !!(*column&mask);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

//uint8_t BBCMicro::ReadMemory(uint16_t address) {
//    M6502Word addr={address};
//    if(addr.b.h>=0xfc&&addr.b.h<0xff) {
//        return 0;
//    } else if(m_pc_pages) {
//        return m_pc_pages[0]->r[addr.b.h][addr.b.l];
//    } else {
//        return m_pages.r[addr.b.h][addr.b.l];
//    }
//}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

const uint8_t *BBCMicro::GetRAM() const {
    return m_ram;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool BBCMicro::SetKeyState(BeebKey key,bool new_state) {
    ASSERT(key>=0&&(int)key<128);

    uint8_t *column=&m_state.key_columns[key&0x0f];
    uint8_t mask=1<<(key>>4);
    bool old_state=(*column&mask)!=0;

    if(key==BeebKey_Break) {
        if(new_state!=m_state.resetting) {
            m_state.resetting=new_state;

            if(new_state) {
                M6502_Halt(&m_state.cpu);
            } else {
                M6502_Reset(&m_state.cpu);
                this->StopPaste();
            }

            return true;
        }
    } else {
        if(!old_state&&new_state) {
            ASSERT(m_state.num_keys_down<256);
            ++m_state.num_keys_down;
            *column|=mask;

            return true;
        } else if(old_state&&!new_state) {
            ASSERT(m_state.num_keys_down>0);
            --m_state.num_keys_down;
            *column&=~mask;

            return true;
        }
    }

    return false;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool BBCMicro::HasNumericKeypad() const {
    return m_type==BBCMicroType_Master;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if BBCMICRO_DEBUGGER
bool BBCMicro::GetTeletextDebug() const {
    return m_state.saa5050.IsDebug();
}
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if BBCMICRO_DEBUGGER
void BBCMicro::SetTeletextDebug(bool teletext_debug) {
    m_state.saa5050.SetDebug(teletext_debug);
}
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if BBCMICRO_DEBUGGER
uint8_t BBCMicro::ReadAsyncCallThunk(void *m_,M6502Word a) {
    auto m=(BBCMicro *)m_;

    ASSERT(a.w>=ASYNC_CALL_THUNK_ADDR.w);
    size_t offset=(size_t)(a.w-ASYNC_CALL_THUNK_ADDR.w);
    ASSERT(offset<sizeof m->m_state.async_call_thunk_buf);

    //LOGF(OUTPUT,"%s: type=%u a=$%04x v=$%02x cycles=%" PRIu64 "\n",__func__,m->m_state.cpu.read,a.w,m->m_state.async_call_thunk_buf[offset],m->m_state.num_2MHz_cycles);

    return m->m_state.async_call_thunk_buf[offset];
}
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if BBCMICRO_DEBUGGER
void BBCMicro::HandleReadByteDebugFlags(uint8_t read,DebugState::ByteDebugFlags *flags) {
    if(flags->bits.break_execute) {
        if(read==M6502ReadType_Opcode) {
            this->DebugHalt("execute: $%04x",m_state.cpu.abus.w);
        }
    } else if(flags->bits.temp_execute) {
        if(read==M6502ReadType_Opcode) {
            this->DebugHalt("single step");
        }
    }

    if(flags->bits.break_read) {
        if(read<=M6502ReadType_LastInterestingDataRead) {
            this->DebugHalt("data read: $%04x",m_state.cpu.abus.w);
        }
    }
}
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if BBCMICRO_DEBUGGER
void BBCMicro::HandleInterruptBreakpoints() {
    if(M6502_IsProbablyIRQ(&m_state.cpu)) {
        if((m_state.system_via.ifr.value&m_state.system_via.ier.value&m_debug->hw.system_via_irq_breakpoints.value)||
            (m_state.user_via.ifr.value&m_state.user_via.ier.value&m_debug->hw.user_via_irq_breakpoints.value))
        {
            this->SetDebugStepType(BBCMicroStepType_StepIntoIRQHandler);
        }
    }
}
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

//
//if(mmio_page<3) {
//    if(m->m_state.cpu.read) {
//        ReadMMIOFn fn=m->m_rmmio_fns[mmio_page][m->m_state.cpu.abus.b.l];
//        void *context=m->m_mmio_fn_contexts[mmio_page][m->m_state.cpu.abus.b.l];
//        m->m_state.cpu.dbus=(*fn)(context,m->m_state.cpu.abus);
//    } else {
//        WriteMMIOFn fn=m->m_hw_wmmio_fns[mmio_page][m->m_state.cpu.abus.b.l];
//        void *context=m->m_hw_mmio_fn_contexts[mmio_page][m->m_state.cpu.abus.b.l];
//        (*fn)(context,m->m_state.cpu.abus,m->m_state.cpu.dbus);
//    }
//} else {

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BBCMicro::HandleCPUDataBusMainRAMOnly(BBCMicro *m) {
    uint8_t mmio_page=m->m_state.cpu.abus.b.h-0xfc;
    if(const uint8_t read=m->m_state.cpu.read) {
        if(mmio_page<3) {
            ReadMMIOFn fn=m->m_rmmio_fns[mmio_page][m->m_state.cpu.abus.b.l];
            void *context=m->m_mmio_fn_contexts[mmio_page][m->m_state.cpu.abus.b.l];
            m->m_state.cpu.dbus=(*fn)(context,m->m_state.cpu.abus);
        } else {
            m->m_state.cpu.dbus=m->m_pages.r[m->m_state.cpu.abus.b.h][m->m_state.cpu.abus.b.l];
        }
    } else {
        if(mmio_page<3) {
            WriteMMIOFn fn=m->m_hw_wmmio_fns[mmio_page][m->m_state.cpu.abus.b.l];
            void *context=m->m_hw_mmio_fn_contexts[mmio_page][m->m_state.cpu.abus.b.l];
            (*fn)(context,m->m_state.cpu.abus,m->m_state.cpu.dbus);
        } else {
            m->m_pages.w[m->m_state.cpu.abus.b.h][m->m_state.cpu.abus.b.l]=m->m_state.cpu.dbus;
        }
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if BBCMICRO_DEBUGGER
void BBCMicro::HandleCPUDataBusMainRAMOnlyDebug(BBCMicro *m) {
    uint8_t mmio_page=m->m_state.cpu.abus.b.h-0xfc;
    if(const uint8_t read=m->m_state.cpu.read) {
        if(mmio_page<3) {
            ReadMMIOFn fn=m->m_rmmio_fns[mmio_page][m->m_state.cpu.abus.b.l];
            void *context=m->m_mmio_fn_contexts[mmio_page][m->m_state.cpu.abus.b.l];
            m->m_state.cpu.dbus=(*fn)(context,m->m_state.cpu.abus);
        } else {
            m->m_state.cpu.dbus=m->m_pages.r[m->m_state.cpu.abus.b.h][m->m_state.cpu.abus.b.l];
        }

        DebugState::ByteDebugFlags *flags=&m->m_pages.debug[m->m_state.cpu.abus.b.h][m->m_state.cpu.abus.b.l];
        if(flags->value!=0) {
            m->HandleReadByteDebugFlags(read,flags);
        }

        if(read==M6502ReadType_Interrupt) {
            m->HandleInterruptBreakpoints();
        }
    } else {
        if(mmio_page<3) {
            WriteMMIOFn fn=m->m_hw_wmmio_fns[mmio_page][m->m_state.cpu.abus.b.l];
            void *context=m->m_hw_mmio_fn_contexts[mmio_page][m->m_state.cpu.abus.b.l];
            (*fn)(context,m->m_state.cpu.abus,m->m_state.cpu.dbus);
        } else {
            m->m_pages.w[m->m_state.cpu.abus.b.h][m->m_state.cpu.abus.b.l]=m->m_state.cpu.dbus;
        }

        if(m->m_pages.debug[m->m_state.cpu.abus.b.h][m->m_state.cpu.abus.b.l].bits.break_write) {
            m->DebugHalt("data write: $%04x",m->m_state.cpu.abus.w);
        }
    }
}
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BBCMicro::HandleCPUDataBusWithShadowRAM(BBCMicro *m) {
    uint8_t mmio_page=m->m_state.cpu.abus.b.h-0xfc;
    if(const uint8_t read=m->m_state.cpu.read) {
        if(mmio_page<3) {
            ReadMMIOFn fn=m->m_rmmio_fns[mmio_page][m->m_state.cpu.abus.b.l];
            void *context=m->m_mmio_fn_contexts[mmio_page][m->m_state.cpu.abus.b.l];
            m->m_state.cpu.dbus=(*fn)(context,m->m_state.cpu.abus);
        } else {
            m->m_state.cpu.dbus=m->m_pc_pages[m->m_state.cpu.opcode_pc.b.h]->r[m->m_state.cpu.abus.b.h][m->m_state.cpu.abus.b.l];
        }
    } else {
        if(mmio_page<3) {
            WriteMMIOFn fn=m->m_hw_wmmio_fns[mmio_page][m->m_state.cpu.abus.b.l];
            void *context=m->m_hw_mmio_fn_contexts[mmio_page][m->m_state.cpu.abus.b.l];
            (*fn)(context,m->m_state.cpu.abus,m->m_state.cpu.dbus);
        } else {
            m->m_pc_pages[m->m_state.cpu.opcode_pc.b.h]->w[m->m_state.cpu.abus.b.h][m->m_state.cpu.abus.b.l]=m->m_state.cpu.dbus;
        }
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if BBCMICRO_DEBUGGER
void BBCMicro::HandleCPUDataBusWithShadowRAMDebug(BBCMicro *m) {
    uint8_t mmio_page=m->m_state.cpu.abus.b.h-0xfc;
    if(const uint8_t read=m->m_state.cpu.read) {
        if(mmio_page<3) {
            ReadMMIOFn fn=m->m_rmmio_fns[mmio_page][m->m_state.cpu.abus.b.l];
            void *context=m->m_mmio_fn_contexts[mmio_page][m->m_state.cpu.abus.b.l];
            m->m_state.cpu.dbus=(*fn)(context,m->m_state.cpu.abus);
        } else {
            m->m_state.cpu.dbus=m->m_pc_pages[m->m_state.cpu.opcode_pc.b.h]->r[m->m_state.cpu.abus.b.h][m->m_state.cpu.abus.b.l];
        }

        DebugState::ByteDebugFlags *flags=&m->m_pc_pages[m->m_state.cpu.opcode_pc.b.h]->debug[m->m_state.cpu.abus.b.h][m->m_state.cpu.abus.b.l];
        if(flags->value!=0) {
            m->HandleReadByteDebugFlags(read,flags);
        }

        if(read==M6502ReadType_Interrupt) {
            m->HandleInterruptBreakpoints();
        }
    } else {
        if(mmio_page<3) {
            WriteMMIOFn fn=m->m_hw_wmmio_fns[mmio_page][m->m_state.cpu.abus.b.l];
            void *context=m->m_hw_mmio_fn_contexts[mmio_page][m->m_state.cpu.abus.b.l];
            (*fn)(context,m->m_state.cpu.abus,m->m_state.cpu.dbus);
        } else {
            m->m_pc_pages[m->m_state.cpu.opcode_pc.b.h]->w[m->m_state.cpu.abus.b.h][m->m_state.cpu.abus.b.l]=m->m_state.cpu.dbus;
        }

        if(m->m_pc_pages[m->m_state.cpu.opcode_pc.b.h]->debug[m->m_state.cpu.abus.b.h][m->m_state.cpu.abus.b.l].bits.break_write) {
            m->DebugHalt("data write: $%04x",m->m_state.cpu.abus.w);
        }
    }
}
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BBCMicro::HandleCPUDataBusWithHacks(BBCMicro *m) {
#if BBCMICRO_DEBUGGER
    if(m->m_state.async_call_address.w!=INVALID_ASYNC_CALL_ADDRESS) {
        if(m->m_state.cpu.read==M6502ReadType_Interrupt&&M6502_IsProbablyIRQ(&m->m_state.cpu)) {
            TRACEF(m->m_trace,"Enqueuing async call: address=$%04x, A=%03u ($%02x) X=%03u ($%02x) Y=%03u ($%02X) C=%s\n",
                   m->m_state.async_call_address.w,
                   m->m_state.async_call_a,m->m_state.async_call_a,
                   m->m_state.async_call_x,m->m_state.async_call_x,
                   m->m_state.async_call_y,m->m_state.async_call_y,
                   BOOL_STR(m->m_state.async_call_c));

            // Already on the stack is the actual return that the
            // thunk will RTI to.

            // Manually push current PC and status register.
            {
                M6502P p=M6502_GetP(&m->m_state.cpu);

                // Add the thunk call address that the IRQ routine will RTI to.
                m->SetMemory(m->m_state.cpu.s,m->m_state.cpu.pc.b.h);
                --m->m_state.cpu.s.b.l;

                m->SetMemory(m->m_state.cpu.s,m->m_state.cpu.pc.b.l);
                --m->m_state.cpu.s.b.l;

                m->SetMemory(m->m_state.cpu.s,p.value);
                --m->m_state.cpu.s.b.l;

                // Set up CPU as if it's about to execute the thunk, so
                // the IRQ routine will return to the desired place.
                p.bits.c=m->m_state.async_call_c;
                M6502_SetP(&m->m_state.cpu,p.value);
                m->m_state.cpu.pc=ASYNC_CALL_THUNK_ADDR;
            }

            // Set up thunk.
            {
                uint8_t *p=m->m_state.async_call_thunk_buf;

                *p++=0x48;//pha
                *p++=0x8a;//txa
                *p++=0x48;//pha
                *p++=0x98;//tya
                *p++=0x48;//pha
                *p++=0xa9;
                *p++=m->m_state.async_call_a;
                *p++=0xa2;
                *p++=m->m_state.async_call_x;
                *p++=0xa0;
                *p++=m->m_state.async_call_y;
                *p++=m->m_state.async_call_c?0x38:0x18;//sec:clc
                *p++=0x20;//jsr abs
                *p++=m->m_state.async_call_address.b.l;
                *p++=m->m_state.async_call_address.b.h;
                *p++=0x68;//pla
                *p++=0xa8;//tay
                *p++=0x68;//pla
                *p++=0xaa;//tax
                *p++=0x68;//pla
                *p++=0x40;//rti

                ASSERT((size_t)(p-m->m_state.async_call_thunk_buf)<=sizeof m->m_state.async_call_thunk_buf);
            }

            m->FinishAsyncCall(true);
        } else {
            --m->m_state.async_call_timeout;
            if(m->m_state.async_call_timeout<0) {
                m->FinishAsyncCall(false);
            }
        }
    }
#endif

    (*m->m_default_handle_cpu_data_bus_fn)(m);

    if(M6502_IsAboutToExecute(&m->m_state.cpu)) {
        if(!m->m_instruction_fns.empty()) {

            // This is a bit bizarre, but I just can't stomach the
            // idea of literally like 1,000,000 std::vector calls per
            // second. But this way, it's hopefully more like only
            // 300,000.

            auto *fn=m->m_instruction_fns.data();
            auto *fns_end=fn+m->m_instruction_fns.size();
            bool removed=false;

            while(fn!=fns_end) {
                if((*fn->first)(m,&m->m_state.cpu,fn->second)) {
                    ++fn;
                } else {
                    removed=true;
                    *fn=*--fns_end;
                }
            }

            if(removed) {
                m->m_instruction_fns.resize((size_t)(fns_end-m->m_instruction_fns.data()));

                m->UpdateCPUDataBusFn();
            }
        }

        if(m->m_state.hack_flags&BBCMicroHackFlag_Paste) {
            ASSERT(m->m_state.paste_state!=BBCMicroPasteState_None);

            if(m->m_state.cpu.pc.w==0xffe1) {
                // OSRDCH

                // Put next byte in A.
                switch(m->m_state.paste_state) {
                case BBCMicroPasteState_None:
                    ASSERT(false);
                    break;

                case BBCMicroPasteState_Wait:
                    m->SetKeyState(PASTE_START_KEY,false);
                    m->m_state.paste_state=BBCMicroPasteState_Delete;
                    // fall through
                case BBCMicroPasteState_Delete:
                    m->m_state.cpu.a=127;
                    m->m_state.paste_state=BBCMicroPasteState_Paste;
                    break;

                case BBCMicroPasteState_Paste:
                    ASSERT(m->m_state.paste_index<m->m_state.paste_text->size());
                    m->m_state.cpu.a=(uint8_t)m->m_state.paste_text->at(m->m_state.paste_index);

                    ++m->m_state.paste_index;
                    if(m->m_state.paste_index==m->m_state.paste_text->size()) {
                        m->StopPaste();
                    }
                    break;
                }

                // No Escape.
                m->m_state.cpu.p.bits.c=0;

                // Pretend the instruction was RTS.
                m->m_state.cpu.dbus=0x60;
            }
        }

#if BBCMICRO_TRACE
        if(m->m_trace) {
            InstructionTraceEvent *e;

            // Fill out results of last instruction.
            if((e=m->m_trace_current_instruction)!=NULL) {
                e->a=m->m_state.cpu.a;
                e->x=m->m_state.cpu.x;
                e->y=m->m_state.cpu.y;
                e->p=m->m_state.cpu.p.value;
                e->data=m->m_state.cpu.data;
                e->opcode=m->m_state.cpu.opcode;
                e->s=m->m_state.cpu.s.b.l;
                //e->pc=m_state.cpu.pc.w;//...for next instruction
                e->ad=m->m_state.cpu.ad.w;
                e->ia=m->m_state.cpu.ia.w;
            }

            // Allocate event for next instruction.
            e=m->m_trace_current_instruction=(InstructionTraceEvent *)m->m_trace->AllocEvent(INSTRUCTION_EVENT);

            if(e) {
                e->pc=m->m_state.cpu.abus.w;

                /* doesn't matter if the last instruction ends up
                * bogus... there are no invalid values.
                */
            }
        }
#endif
    }

#if BBCMICRO_DEBUGGER
    if(m->m_debug) {
        if(m->m_state.cpu.read>=M6502ReadType_Opcode) {
            switch(m->m_debug->step_type) {
            default:
                ASSERT(false);
                // fall through
            case BBCMicroStepType_None:
                break;

            case BBCMicroStepType_StepIn:
                {
                    if(m->m_state.cpu.read==M6502ReadType_Opcode) {
                        // Done.
                        m->DebugHalt("single step");
                    } else {
                        ASSERT(m->m_state.cpu.read==M6502ReadType_Interrupt);
                        // The instruction was interrupted, so set a temp
                        // breakpoint in the right place.
                        m->DebugAddTempBreakpoint(m->m_state.cpu.pc);
                    }

                    m->SetDebugStepType(BBCMicroStepType_None);
                }
                break;

            case BBCMicroStepType_StepIntoIRQHandler:
                {
                    ASSERT(m->m_state.cpu.read==M6502ReadType_Opcode||m->m_state.cpu.read==M6502ReadType_Interrupt);
                    if(m->m_state.cpu.read==M6502ReadType_Opcode) {
                        m->SetDebugStepType(BBCMicroStepType_None);
                        m->DebugHalt("IRQ/NMI");
                    }
                }
                break;
            }
        }
    }
#endif
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// The cursor pattern is spread over the next 4 displayed columns.
//
// <pre>
// b7 b6 b5  Shape
// -- -- --  -----
//  0  0  0  <none>
//  0  0  1    __
//  0  1  0   _  
//  0  1  1   ___
//  1  0  0  _
//  1  0  1  _ __ 
//  1  1  0  __
//  1  1  1  ____
// </pre>
//
// Bit 7 control the first column, bit 6 controls the second column,
// and bit 7 controls the 3rd and 4th.
//
// Type 2 is the one used in Mode 7. The pattern in the table here is
// wrong - it's been shifted one column to the left, so the cursor
// appears in the right place. (I assumed this was something to do
// with the Mode 7 delay, and how the emulator doesn't handle this
// properly, but the OS sets the cursor delay to 2! So... I'm
// confused.)
static const uint8_t CURSOR_PATTERNS[8]={
    0+0+0+0,
    0+0+4+8,
    1+0+0+0,//0+2+0+0,
    0+2+4+8,
    1+0+0+0,
    1+0+4+8,
    1+2+0+0,
    1+2+4+8,
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if BBCMICRO_DEBUGGER
uint16_t BBCMicro::DebugGetBeebAddressFromCRTCAddress(uint8_t h,uint8_t l) const {
    M6502Word addr;
    addr.b.h=h;
    addr.b.l=l;

    if(addr.w&0x2000) {
        return (addr.w&0x3ff)|m_teletext_bases[addr.w>>11&1];
    } else {
        if(addr.w&0x1000) {
            addr.w-=SCREEN_WRAP_ADJUSTMENTS[m_state.addressable_latch.bits.screen_base];
            addr.w&=~0x1000;
        }

        return addr.w<<3;
    }
}
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool BBCMicro::Update(VideoDataUnit *video_unit,SoundDataUnit *sound_unit) {
    uint8_t odd_cycle=m_state.num_2MHz_cycles&1;
    bool sound=false;

    ++m_state.num_2MHz_cycles;

    // Update video hardware.
    if(m_state.video_ula.control.bits.fast_6845|odd_cycle) {
        CRTC::Output output=m_state.crtc.Update(m_state.video_ula.control.bits.fast_6845);

        m_state.system_via.a.c1=output.vsync;
        m_state.cursor_pattern>>=1;

        if(output.hsync) {
            if(!m_state.crtc_last_output.hsync) {
                if(output.display) {
                    m_state.saa5050.HSync();
                }
            }
        } else if(output.vsync) {
            if(!m_state.crtc_last_output.vsync) {
                m_state.last_frame_2MHz_cycles=m_state.num_2MHz_cycles-m_state.last_vsync_2MHz_cycles;
                m_state.last_vsync_2MHz_cycles=m_state.num_2MHz_cycles;

                m_state.saa5050.VSync(output.odd_frame);
            }
        } else if(output.display) {
            uint16_t addr=(uint16_t)output.address;

            if(addr&0x2000) {
                addr=(addr&0x3ff)|m_teletext_bases[addr>>11&1];
            } else {
                if(addr&0x1000) {
                    addr-=SCREEN_WRAP_ADJUSTMENTS[m_state.addressable_latch.bits.screen_base];
                    addr&=~0x1000;
                }

                addr<<=3;

                // When output.raster>=8, this address is bogus. There's a
                // check later.
                addr|=output.raster&7;
            }

            ASSERTF(addr<32768,"output: hsync=%u vsync=%u display=%u address=0x%x raster=%u; addr=0x%x; latch screen_base=%u\n",
                    output.hsync,output.vsync,output.display,output.address,output.raster,
                    addr,
                    m_state.addressable_latch.bits.screen_base);
            addr|=m_state.shadow_select_mask;
            if(m_state.video_ula.control.bits.teletext) {
                m_state.saa5050.Byte(m_ram[addr]);
            } else {
                if(!m_state.crtc_last_output.display) {
                    m_state.video_ula.DisplayEnabled();
                }

                m_state.video_ula.Byte(m_ram[addr]);
            }

            if(output.cudisp) {
                m_state.cursor_pattern=CURSOR_PATTERNS[m_state.video_ula.control.bits.cursor];
            }

#if VIDEO_TRACK_METADATA
            m_last_video_access_address=addr;
#endif
        }

        m_state.crtc_last_output=output;
    }

    // Update display output.
    if(m_state.crtc_last_output.hsync) {
        video_unit->type.x=VideoDataType_HSync;
    } else if(m_state.crtc_last_output.vsync) {
        video_unit->type.x=VideoDataType_VSync;
    } else if(m_state.crtc_last_output.display) {
        if(m_state.video_ula.control.bits.teletext) {
            m_state.saa5050.EmitVideoDataUnit(video_unit);
#if VIDEO_TRACK_METADATA
            video_unit->teletext.metadata.addr=m_last_video_access_address;
#endif

            if(m_state.cursor_pattern&1) {
                video_unit->teletext.colours[0]^=7;
                video_unit->teletext.colours[1]^=7;
            }
        } else {
            if(m_state.crtc_last_output.raster<8) {
                m_state.video_ula.EmitPixels(video_unit);
#if VIDEO_TRACK_METADATA
                video_unit->bitmap.metadata.addr=m_last_video_access_address;
#endif
                //(m_state.video_ula.*VideoULA::EMIT_MFNS[m_state.video_ula.control.bits.line_width])(hu);

                if(m_state.cursor_pattern&1) {
                    video_unit->values[0]^=0x0fff0fff0fff0fffull;
                    video_unit->values[1]^=0x0fff0fff0fff0fffull;
                }
            } else {
                video_unit->type.x=VideoDataType_Nothing^(m_state.cursor_pattern&1);
            }
        }
    } else {
        video_unit->type.x=VideoDataType_Nothing;
    }

    if(odd_cycle) {
        // Update keyboard.
        if(m_state.addressable_latch.bits.not_kb_write) {
            if(m_state.key_columns[m_state.key_scan_column]&0xfe) {
                m_state.system_via.a.c2=1;
            } else {
                m_state.system_via.a.c2=0;
            }

            ++m_state.key_scan_column;
            m_state.key_scan_column&=0x0f;
        } else {
            // manual scan
            BeebKey key=(BeebKey)(m_state.system_via.a.p&0x7f);
            uint8_t kcol=key&0x0f;
            uint8_t krow=(uint8_t)(key>>4);

            uint8_t *column=&m_state.key_columns[kcol];

            // row 0 doesn't cause an interrupt
            if(*column&0xfe) {
                m_state.system_via.a.c2=1;
            } else {
                m_state.system_via.a.c2=0;
            }

            m_state.system_via.a.p&=0x7f;
            if(*column&1<<krow) {
                m_state.system_via.a.p|=0x80;
            }

            //if(key==m_state.auto_reset_key) {
            //    //*column&=~(1<<krow);
            //    m_state.auto_reset_key=BeebKey_None;
            //}
        }

        // Update joysticks.
        m_state.system_via.b.p|=1<<4|1<<5;

        if(m_beeblink_handler) {
            // Update BeebLink.
            m_beeblink->Update(&m_state.user_via);
        } else {
            // Nothing connected to the user port.
            m_state.user_via.b.p=255;
            m_state.user_via.b.c1=1;
        }

        // Update IRQs.
        if(m_state.system_via.Update()) {
            M6502_SetDeviceIRQ(&m_state.cpu,BBCMicroIRQDevice_SystemVIA,1);
        } else {
            M6502_SetDeviceIRQ(&m_state.cpu,BBCMicroIRQDevice_SystemVIA,0);
        }

        if(m_state.user_via.Update()) {
            M6502_SetDeviceIRQ(&m_state.cpu,BBCMicroIRQDevice_UserVIA,1);
        } else {
            M6502_SetDeviceIRQ(&m_state.cpu,BBCMicroIRQDevice_UserVIA,0);
        }

        // Update addressable latch and RTC.
        if(m_state.old_system_via_pb!=m_state.system_via.b.p) {
            SystemVIAPB pb;
            pb.value=m_state.system_via.b.p;

            SystemVIAPB old_pb;
            old_pb.value=m_state.old_system_via_pb;

            uint8_t mask=1<<pb.bits.latch_index;

            m_state.addressable_latch.value&=~mask;
            if(pb.bits.latch_value) {
                m_state.addressable_latch.value|=mask;
            }

#if BBCMICRO_TRACE
            if(m_trace) {
                if(m_trace_flags&BBCMicroTraceFlag_SystemVIA) {
                    TracePortB(pb);
                }
            }
#endif

            if(pb.m128_bits.rtc_chip_select) {
                uint8_t x=m_state.system_via.a.p;

                if(old_pb.m128_bits.rtc_address_strobe==1&&pb.m128_bits.rtc_address_strobe==0) {
                    m_state.rtc.SetAddress(x);
                }

                AddressableLatch test;
                test.value=m_state.old_addressable_latch.value^m_state.addressable_latch.value;
                if(test.m128_bits.rtc_data_strobe) {
                    if(m_state.addressable_latch.m128_bits.rtc_data_strobe) {
                        // 0->1
                        if(m_state.addressable_latch.m128_bits.rtc_read) {
                            m_state.system_via.a.p=m_state.rtc.Read();
                        }
                    } else {
                        // 1->0
                        if(!m_state.addressable_latch.m128_bits.rtc_read) {
                            m_state.rtc.SetData(x);
                        }
                    }
                }
            }

            m_state.old_system_via_pb=m_state.system_via.b.p;
        }

        // Update RTC.
        if(m_has_rtc) {
            m_state.rtc.Update();
        }

        // Update NMI.
        M6502_SetDeviceNMI(&m_state.cpu,BBCMicroNMIDevice_1770,m_state.fdc.Update().value);

        // Update sound.
        if((m_state.num_2MHz_cycles&((1<<SOUND_CLOCK_SHIFT)-1))==0) {
            sound_unit->sn_output=m_state.sn76489.Update(!m_state.addressable_latch.bits.not_sound_write,
                                                         m_state.system_via.a.p);

#if BBCMICRO_ENABLE_DISC_DRIVE_SOUND
            // The disc drive sounds are pretty quiet. 
            sound_unit->disc_drive_sound=this->UpdateDiscDriveSound(&m_state.drives[0]);
            sound_unit->disc_drive_sound+=this->UpdateDiscDriveSound(&m_state.drives[1]);
#endif
            sound=true;
        }

        m_state.old_addressable_latch=m_state.addressable_latch;
    }

    // Update CPU.
    if(m_state.stretched_cycles_left>0) {
        --m_state.stretched_cycles_left;
        if(m_state.stretched_cycles_left==0) {
            ASSERT(odd_cycle);
        }
    } else {
        (*m_state.cpu.tfn)(&m_state.cpu);

        uint8_t mmio_page=m_state.cpu.abus.b.h-0xfc;
        if(mmio_page<3) {
            uint8_t num_stretch_cycles=1+odd_cycle;

            uint8_t stretch;
            if(m_state.cpu.read) {
                stretch=m_mmio_stretch[mmio_page][m_state.cpu.abus.b.l];
            } else {
                stretch=m_hw_mmio_stretch[mmio_page][m_state.cpu.abus.b.l];
            }

            m_state.stretched_cycles_left=num_stretch_cycles&stretch;
        }
    }

    if(m_state.stretched_cycles_left==0) {
        (*m_handle_cpu_data_bus_fn)(this);
    }

    return sound;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if BBCMICRO_ENABLE_DISC_DRIVE_SOUND
void BBCMicro::SetDiscDriveSound(DiscDriveType type,DiscDriveSound sound,std::vector<float> samples) {
    ASSERT(sound>=0&&sound<DiscDriveSound_EndValue);
    ASSERT(g_disc_drive_sounds[type][sound].empty());
    ASSERT(samples.size()<=INT_MAX);
    g_disc_drive_sounds[type][sound]=std::move(samples);
}

//void BBCMicro::SetDiscDriveSound(int drive,DiscDriveSound sound,const float *samples,size_t num_samples) {
//    ASSERT(drive>=0&&drive<NUM_DRIVES);
//    DiscDrive_SetSoundData(&m_state.drives[drive],sound,samples,num_samples);
//}
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

uint32_t BBCMicro::GetLEDs() {
    uint32_t leds=0;

    if(!(m_state.addressable_latch.bits.caps_lock_led)) {
        leds|=BBCMicroLEDFlag_CapsLock;
    }

    if(!(m_state.addressable_latch.bits.shift_lock_led)) {
        leds|=BBCMicroLEDFlag_ShiftLock;
    }

    for(int i=0;i<NUM_DRIVES;++i) {
        if(m_state.drives[i].motor) {
            leds|=(uint32_t)BBCMicroLEDFlag_Drive0<<i;
        }
    }

    return leds;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

std::vector<uint8_t> BBCMicro::GetNVRAM() const {
    if(m_has_rtc) {
        return m_state.rtc.GetRAMContents();
    } else {
        return std::vector<uint8_t>();
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BBCMicro::SetOSROM(std::shared_ptr<const ROMData> data) {
    m_state.os_buffer=std::move(data);
    this->InitROMPages();
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BBCMicro::SetSidewaysROM(uint8_t bank,std::shared_ptr<const ROMData> data) {
    ASSERT(bank<16);

    m_state.sideways_ram_buffers[bank].clear();

    m_state.sideways_rom_buffers[bank]=std::move(data);

    this->InitROMPages();
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BBCMicro::SetSidewaysRAM(uint8_t bank,std::shared_ptr<const ROMData> data) {
    ASSERT(bank<16);

    if(data) {
        m_state.sideways_ram_buffers[bank]=std::vector<uint8_t>(data->begin(),data->end());
    } else {
        m_state.sideways_ram_buffers[bank]=std::vector<uint8_t>(16384);
    }

    m_state.sideways_rom_buffers[bank]=nullptr;

    this->InitROMPages();
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if BBCMICRO_TRACE
void BBCMicro::StartTrace(uint32_t trace_flags,size_t max_num_bytes) {
    this->StopTrace();

    this->SetTrace(std::make_shared<Trace>(max_num_bytes),trace_flags);
}
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if BBCMICRO_TRACE
std::shared_ptr<Trace> BBCMicro::StopTrace() {
    std::shared_ptr<Trace> old_trace=m_trace_ptr;

    if(m_trace) {
        if(m_trace_current_instruction) {
            m_trace->CancelEvent(INSTRUCTION_EVENT,m_trace_current_instruction);
            m_trace_current_instruction=NULL;
        }

        this->SetTrace(nullptr,0);
    }

    return old_trace;
}
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if BBCMICRO_TRACE
int BBCMicro::GetTraceStats(struct TraceStats *stats) {
    if(!m_trace) {
        return 0;
    }

    m_trace->GetStats(stats);
    return 1;
}
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BBCMicro::AddInstructionFn(InstructionFn fn,void *context) {
    ASSERT(std::find(m_instruction_fns.begin(),m_instruction_fns.end(),std::make_pair(fn,context))==m_instruction_fns.end());

    m_instruction_fns.emplace_back(fn,context);

    this->UpdateCPUDataBusFn();
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BBCMicro::SetMMIOFns(uint16_t addr,ReadMMIOFn read_fn,WriteMMIOFn write_fn,void *context) {
    ASSERT(addr>=0xfc00&&addr<=0xfeff);

    M6502Word tmp;
    tmp.w=addr;
    tmp.b.h-=0xfc;

    m_hw_rmmio_fns[tmp.b.h][tmp.b.l]=read_fn?read_fn:&ReadUnmappedMMIO;
    m_hw_wmmio_fns[tmp.b.h][tmp.b.l]=write_fn?write_fn:&WriteUnmappedMMIO;
    m_hw_mmio_fn_contexts[tmp.b.h][tmp.b.l]=context;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BBCMicro::SetMMIOCycleStretch(uint16_t addr,bool stretch) {
    ASSERT(addr>=0xfc00&&addr<=0xfeff);

    M6502Word tmp;
    tmp.w=addr;
    tmp.b.h-=0xfc;

    m_hw_mmio_stretch[tmp.b.h][tmp.b.l]=stretch?0xff:0x00;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

std::shared_ptr<DiscImage> BBCMicro::GetMutableDiscImage(int drive) {
    if(drive>=0&&drive<NUM_DRIVES) {
        return m_disc_images[drive];
    } else {
        return nullptr;
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

std::shared_ptr<const DiscImage> BBCMicro::GetDiscImage(int drive) const {
    if(drive>=0&&drive<NUM_DRIVES) {
        return m_disc_images[drive];
    } else {
        return nullptr;
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BBCMicro::SetDiscImage(int drive,std::shared_ptr<DiscImage> disc_image) {
    if(drive<0||drive>=NUM_DRIVES) {
        return;
    }

    m_disc_images[drive]=std::move(disc_image);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool BBCMicro::GetAndResetDiscAccessFlag() {
    bool result=m_disc_access;

    m_disc_access=false;

    return result;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool BBCMicro::IsPasting() const {
    return (m_state.hack_flags&BBCMicroHackFlag_Paste)!=0;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BBCMicro::StartPaste(std::shared_ptr<const std::string> text) {
    this->StopPaste();

    m_state.hack_flags|=BBCMicroHackFlag_Paste;
    m_state.paste_state=BBCMicroPasteState_Wait;
    m_state.paste_text=std::move(text);
    m_state.paste_index=0;
    m_state.paste_wait_end=m_state.num_2MHz_cycles+2000000;

    this->SetKeyState(PASTE_START_KEY,true);

    this->UpdateCPUDataBusFn();
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BBCMicro::StopPaste() {
    m_state.paste_state=BBCMicroPasteState_None;
    m_state.paste_index=0;
    m_state.paste_text.reset();

    m_state.hack_flags&=(uint32_t)~BBCMicroHackFlag_Paste;
    this->UpdateCPUDataBusFn();
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

const M6502 *BBCMicro::GetM6502() const {
    return &m_state.cpu;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if BBCMICRO_DEBUGGER
const CRTC *BBCMicro::DebugGetCRTC() const {
    return &m_state.crtc;
}
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if BBCMICRO_DEBUGGER

const ExtMem *BBCMicro::DebugGetExtMem() const {
    if(m_ext_mem) {
        return &m_state.ext_mem;
    } else {
        return nullptr;
    }
}

#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if BBCMICRO_DEBUGGER
const VideoULA *BBCMicro::DebugGetVideoULA() const {
    return &m_state.video_ula;
}
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if BBCMICRO_DEBUGGER
const BBCMicro::AddressableLatch BBCMicro::DebugGetAddressableLatch() const {
    return m_state.addressable_latch;
}
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if BBCMICRO_DEBUGGER
const R6522 *BBCMicro::DebugGetSystemVIA() const {
    return &m_state.system_via;
}
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if BBCMICRO_DEBUGGER
const R6522 *BBCMicro::DebugGetUserVIA() const {
    return &m_state.user_via;
}
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if BBCMICRO_DEBUGGER
uint16_t BBCMicro::DebugGetFlatPage(uint8_t page) const {
    if(m_pc_pages) {
        return m_pc_pages[m_state.cpu.opcode_pc.b.h]->debug_page_index[page];
    } else {
        return m_pages.debug_page_index[page];
    }
}
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if BBCMICRO_DEBUGGER
void BBCMicro::DebugCopyMemory(void *bytes_dest_,DebugState::ByteDebugFlags *debug_dest,M6502Word addr_,uint16_t num_bytes) const {
    M6502Word addr=addr_;
    auto bytes_dest=(char *)bytes_dest_;
    size_t num_bytes_left=num_bytes;

    const uint8_t *const *bytes_pages;
    const DebugState::ByteDebugFlags *const *debug_pages;
    if(m_pc_pages) {
        bytes_pages=m_pc_pages[m_state.cpu.opcode_pc.b.h]->r;
        debug_pages=m_pc_pages[m_state.cpu.opcode_pc.b.h]->debug;
    } else {
        bytes_pages=m_pages.r;
        debug_pages=m_pages.debug;
    }

    while(num_bytes_left>0) {
        uint8_t page=addr.b.h;
        uint8_t offset=addr.b.l;

        uint16_t n=256-offset;
        if(n>num_bytes_left) {
            n=(uint16_t)num_bytes_left;
        }

        memcpy(bytes_dest,bytes_pages[page]+offset,n);
        bytes_dest+=n;

        if(debug_dest) {
            static_assert(sizeof *debug_dest==1,"");
            if(m_debug) {
                memcpy(debug_dest,&debug_pages[page][offset],n);
            } else {
                memset(debug_dest,0,n);
            }
            debug_dest+=n;
        }

        addr.w+=n;

        ASSERT(num_bytes_left>=n);
        num_bytes_left-=n;
    }
}
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if BBCMICRO_DEBUGGER
// <pre>
// Address     &0000-&7FFF   &8000-&BFFF   &C000-&FBFF  &FC00-&FDFF   address
// <&FFxxxxxx   lang memory   lang memory   lang memory  lang memory  b23-b21+4
// &FF0rxxxx   main memory   SROM/SRAM r   FS RAM         MOS ROM       0100
// &FF2rxxxx   main memory   SROM/SRAM r   FS RAM           I/O         0110
// &FF4rxxxx   main memory   VDU RAM       MOS ROM        MOS ROM       1000
// &FF6rxxxx   main memory   VDU RAM       MOS ROM          I/O         1010
// &FF8rxxxx   main memory   VDU RAM       FS RAM         MOS ROM       1100
// &FFArxxxx   main memory   VDU RAM       FS RAM           I/O         1110
// &FFCrxxxx   main memory   SROM/SRAM r   MOS ROM        MOS ROM       0000
// &FFErxxxx   main memory   SROM/SRAM r   MOS ROM          I/O         0010
// &FFFDxxxx   shadow mem    SROM/SRAM r   MOS ROM          I/O         |||
// &FFFExxxx   display mem   SROM/SRAM r   MOS ROM          I/O         |||
// &FFFFxxxx   main memory   SROM/SRAM r   MOS ROM          I/O         |||
//                                                             SROM/VDU-+||
//                                                             MOS/FSRAM-+|
//                                                             MOS/IO-----+
void BBCMicro::DebugGetBytePointers(uint8_t **write_ptr,const uint8_t **read_ptr,uint16_t *debug_page_ptr,uint32_t full_addr) {
    uint8_t *write=nullptr;
    const uint8_t *read=nullptr;
    uint16_t debug_page=DebugState::INVALID_PAGE_INDEX;

    if(full_addr<0xff000000) {
        // With no Tube support, this just maps to a copy of
        // whatever's current in the I/O processor.

        const M6502Word addr={(uint16_t)full_addr};

        if(addr.w>=0xfc00&&addr.w<0xff00) {
            // Ignore...
        } else {
            // Just pick whatever's paged in right now.
            if(m_pc_pages) {
                read=m_pc_pages[0]->r[addr.b.h]+addr.b.l;
                write=m_pc_pages[0]->w[addr.b.h]+addr.b.l;
                debug_page=m_pc_pages[0]->debug_page_index[addr.b.h];
            } else {
                read=m_pages.r[addr.b.h]+addr.b.l;
                write=m_pages.w[addr.b.h]+addr.b.l;
                debug_page=m_pages.debug_page_index[addr.b.h];
            }
        }
    } else {
        const M6502Word addr={(uint16_t)full_addr};
        uint8_t bbb=((full_addr>>20)+4)&0xf;
        uint8_t r=(full_addr>>16)&0xf;
        bool parasite=full_addr>=0xff000000;

        switch(addr.w>>12&0xf) {
        case 0x0:
        case 0x1:
        case 0x2:
            // Always main memory.
        main_memory:;
            read=write=&m_state.ram_buffer[addr.w];
            debug_page=addr.b.h;
            break;

        case 0x3:
        case 0x4:
        case 0x5:
        case 0x6:
        case 0x7:
            if(parasite) {
                goto main_memory;
            }

            // I/O memory, display memory or shadow RAM.
            switch(full_addr>>16&0xf) {
            case 0xf:
                // I/O memory.
                read=write=&m_state.ram_buffer[addr.w];
                debug_page=addr.b.h;
                break;

            case 0xe:
                // Currently displayed screen.
                {
                    M6502Word disp_addr={(uint16_t)(addr.w|m_state.shadow_select_mask)};
                    read=write=&m_state.ram_buffer[disp_addr.w];
                    debug_page=disp_addr.b.h;
                }
                break;

            default:
                // Shadow RAM.
                {
                    M6502Word shadow_addr={(uint16_t)(addr.w|0x8000)};
                    read=write=&m_state.ram_buffer[shadow_addr.w];
                    debug_page=shadow_addr.b.h;
                }
                break;
            }
            break;

        case 0x8:
        case 0x9:
        case 0xa:
            // Sideways ROM/extra B+ or M128 RAM
            if(!parasite&&(bbb&4)&&m_state.ram_buffer.size()>32768) {
                read=write=&m_state.ram_buffer[addr.w];
                debug_page=addr.b.h;
            } else {
            sideways:
                if(!!m_state.sideways_rom_buffers[r]) {
                    read=&m_state.sideways_rom_buffers[r]->at(addr.w-0x8000u);
                    write=nullptr;
                    debug_page=DEBUG_ROM0_PAGE+r*DEBUG_NUM_ROM_PAGES;
                } else if(!m_state.sideways_ram_buffers[r].empty()) {
                    read=write=&m_state.sideways_ram_buffers[r][addr.w-0x8000u];
                    debug_page=DEBUG_ROM0_PAGE+r*DEBUG_NUM_ROM_PAGES;
                } else {
                    read=nullptr;
                    write=nullptr;
                    debug_page=DebugState::INVALID_PAGE_INDEX;
                }
            }
            break;

        case 0xb:
            // can't put the case straight in the right place. VC++ cocks
            // up the indentation.
            goto sideways;

        case 0xc:
        case 0xd:
            if(!parasite&&(bbb&2)&&m_type==BBCMicroType_Master) {
                M6502Word fs_ram_addr={(uint16_t)(addr.w-0xc000u+FS_RAM_OFFSET)};
                read=write=&m_state.ram_buffer[fs_ram_addr.w];
                debug_page=fs_ram_addr.b.h;
            } else {
            mos:;
                M6502Word os_addr={(uint16_t)(addr.w-0xc000)};
                read=&m_state.os_buffer->at(os_addr.w);
                write=nullptr;
                debug_page=DEBUG_OS_PAGE+os_addr.b.h;
            }
            break;

        case 0xe:
            goto mos;

        case 0xf:
            if(addr.w>=0xfc00&&addr.w<=0xfeff) {
                if(parasite||(bbb&1)) {
                    // I/O not accessible.
                } else {
                    goto mos;
                }
            } else {
                goto mos;
            }
            break;
        }
    }

    if(read_ptr) {
        *read_ptr=read;
    }

    if(write_ptr) {
        *write_ptr=write;
    }

    if(debug_page_ptr) {
        *debug_page_ptr=debug_page;
    }
}
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if BBCMICRO_DEBUGGER
void BBCMicro::FinishAsyncCall(bool called) {
    if(m_async_call_fn) {
        (*m_async_call_fn)(called,m_async_call_context);
    }

    m_state.async_call_address.w=INVALID_ASYNC_CALL_ADDRESS;
    m_state.async_call_timeout=0;
    m_async_call_fn=nullptr;
    m_async_call_context=nullptr;

    this->UpdateCPUDataBusFn();
}
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if BBCMICRO_DEBUGGER
void BBCMicro::DebugSetByte(uint32_t addr,uint8_t value) {
    uint8_t *write;
    this->DebugGetBytePointers(&write,nullptr,nullptr,addr);
    if(write) {
        *write=value;
    }
}
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if BBCMICRO_DEBUGGER
int BBCMicro::DebugGetByte(uint32_t addr) const {
    const uint8_t *read;
    const_cast<BBCMicro *>(this)->DebugGetBytePointers(nullptr,&read,nullptr,addr);//ugh.
    if(read) {
        return *read;
    } else {
        return -1;
    }
}
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if BBCMICRO_DEBUGGER
void BBCMicro::SetMemory(M6502Word addr,uint8_t value) {
    if(m_pc_pages) {
        m_pc_pages[m_state.cpu.opcode_pc.b.h]->w[addr.b.h][addr.b.l]=value;
    } else {
        m_pages.w[addr.b.h][addr.b.l]=value;
    }
}
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if BBCMICRO_DEBUGGER

void BBCMicro::SetExtMemory(uint32_t addr, uint8_t value) {
    ExtMem::WriteMemory(&m_state.ext_mem, addr, value);
}

#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if BBCMICRO_DEBUGGER
void BBCMicro::DebugHalt(const char *fmt,...) {
    if(m_debug) {
        m_debug->is_halted=true;

        if(fmt) {
            va_list v;

            va_start(v,fmt);
            vsnprintf(m_debug->halt_reason,sizeof m_debug->halt_reason,fmt,v);
            va_end(v);
        } else {
            m_debug->halt_reason[0]=0;
        }

        for(DebugState::ByteDebugFlags *flags:m_debug->temp_execute_breakpoints) {
            ASSERT(flags>=(void *)m_debug->pages);
            ASSERT(flags<(void *)((char *)m_debug->pages+sizeof m_debug->pages));
            ASSERT(flags->bits.temp_execute);
            flags->bits.temp_execute=0;
        }

        m_debug->temp_execute_breakpoints.clear();

        this->SetDebugStepType(BBCMicroStepType_None);
    }
}
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if BBCMICRO_DEBUGGER
bool BBCMicro::DebugIsHalted() const {
    if(!m_debug) {
        return false;
    }

    return m_debug->is_halted;
}
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if BBCMICRO_DEBUGGER
const char *BBCMicro::DebugGetHaltReason() const {
    if(!m_debug) {
        return nullptr;
    }

    if(m_debug->halt_reason[0]==0) {
        return nullptr;
    }

    return m_debug->halt_reason;
}
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if BBCMICRO_DEBUGGER
void BBCMicro::DebugRun() {
    if(!m_debug) {
        return;
    }

    m_debug->is_halted=false;
}
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if BBCMICRO_DEBUGGER
BBCMicro::DebugState::ByteDebugFlags BBCMicro::DebugGetByteFlags(M6502Word addr) const {
    if(!m_debug) {
        return DUMMY_BYTE_DEBUG_FLAGS;
    }

    if(m_pc_pages) {
        return m_pc_pages[m_state.cpu.opcode_pc.b.h]->debug[addr.b.h][addr.b.l];
    } else {
        return m_pages.debug[addr.b.h][addr.b.l];
    }
}
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if BBCMICRO_DEBUGGER
void BBCMicro::DebugSetByteFlags(M6502Word addr,DebugState::ByteDebugFlags flags) {
    if(!m_debug) {
        return;
    }

    if(m_pc_pages) {
        m_pc_pages[m_state.cpu.opcode_pc.b.h]->debug[addr.b.h][addr.b.l]=flags;
    } else {
        m_pages.debug[addr.b.h][addr.b.l]=flags;
    }
}
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if BBCMICRO_DEBUGGER
void BBCMicro::DebugAddTempBreakpoint(M6502Word addr) {
    if(!m_debug) {
        return;
    }

    DebugState::ByteDebugFlags *flags;
    if(m_pc_pages) {
        flags=&m_pc_pages[m_state.cpu.opcode_pc.b.h]->debug[addr.b.h][addr.b.l];
    } else {
        flags=&m_pages.debug[addr.b.h][addr.b.l];
    }

    if(flags->bits.temp_execute) {
        ASSERT(std::find(m_debug->temp_execute_breakpoints.begin(),
                         m_debug->temp_execute_breakpoints.end(),
                         flags)!=m_debug->temp_execute_breakpoints.end());
    } else {
        flags->bits.temp_execute=1;
        m_debug->temp_execute_breakpoints.push_back(flags);
    }
}
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if BBCMICRO_DEBUGGER
void BBCMicro::DebugStepIn() {
    if(!m_debug) {
        return;
    }

    this->SetDebugStepType(BBCMicroStepType_StepIn);
}
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if BBCMICRO_DEBUGGER
char BBCMicro::DebugGetFlatPageCode(uint16_t flat_page) {
    static_assert(sizeof DEBUG_PAGE_CODES==DebugState::NUM_DEBUG_FLAGS_PAGES+1,"");
    if(flat_page==DebugState::INVALID_PAGE_INDEX) {
        return '!';
    } else {
        ASSERT(flat_page<DebugState::NUM_DEBUG_FLAGS_PAGES);
        return DEBUG_PAGE_CODES[flat_page];
    }
}
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if BBCMICRO_DEBUGGER
bool BBCMicro::HasDebugState() const {
    return !!m_debug;
}
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if BBCMICRO_DEBUGGER
std::unique_ptr<BBCMicro::DebugState> BBCMicro::TakeDebugState() {
    std::unique_ptr<BBCMicro::DebugState> debug=std::move(m_debug_ptr);

    m_debug=nullptr;

    this->UpdateDebugState();

    return debug;
}
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if BBCMICRO_DEBUGGER
void BBCMicro::SetDebugState(std::unique_ptr<DebugState> debug) {
    m_debug_ptr=std::move(debug);
    m_debug=m_debug_ptr.get();

    this->UpdateDebugState();
}
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if BBCMICRO_DEBUGGER
BBCMicro::HardwareDebugState BBCMicro::GetHardwareDebugState() const {
    if(!m_debug) {
        return HardwareDebugState();
    }

    return m_debug->hw;
}
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if BBCMICRO_DEBUGGER
void BBCMicro::SetHardwareDebugState(const HardwareDebugState &hw) {
    if(!m_debug) {
        return;
    }

    m_debug->hw=hw;
}
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if BBCMICRO_DEBUGGER
void BBCMicro::DebugSetAsyncCall(uint16_t address,uint8_t a,uint8_t x,uint8_t y,bool c,DebugAsyncCallFn fn,void *context) {
    this->FinishAsyncCall(false);

    m_state.async_call_address.w=address;
    m_state.async_call_timeout=ASYNC_CALL_TIMEOUT;
    m_state.async_call_a=a;
    m_state.async_call_x=x;
    m_state.async_call_y=y;
    m_state.async_call_c=c;
    m_async_call_fn=fn;
    m_async_call_context=context;

    this->UpdateCPUDataBusFn();
}
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BBCMicro::SendBeebLinkResponse(std::vector<uint8_t> data) {
    ASSERT(!!m_beeblink);
    m_beeblink->SendResponse(std::move(data));
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if BBCMICRO_DEBUGGER
void BBCMicro::UpdateDebugPages(MemoryPages *pages) {
    for(size_t i=0;i<256;++i) {
        if(m_debug) {
            ASSERT(pages->debug_page_index[i]<DebugState::NUM_DEBUG_FLAGS_PAGES);
            pages->debug[i]=m_debug->pages[pages->debug_page_index[i]];
        } else {
            pages->debug[i]=nullptr;
        }
    }
}
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if BBCMICRO_DEBUGGER
void BBCMicro::UpdateDebugState() {
    this->UpdateCPUDataBusFn();

    // Update the debug page pointers.
    if(m_shadow_pages) {
        this->UpdateDebugPages(m_shadow_pages);
    }

    this->UpdateDebugPages(&m_pages);
}
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if BBCMICRO_DEBUGGER
void BBCMicro::SetDebugStepType(BBCMicroStepType step_type) {
    if(m_debug) {
        if(m_debug->step_type!=step_type) {
            m_debug->step_type=step_type;
            this->UpdateCPUDataBusFn();
        }
    }
}
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BBCMicro::InitStuff() {
    CHECK_SIZEOF(AddressableLatch,1);
    CHECK_SIZEOF(ROMSEL,1);
    CHECK_SIZEOF(ACCCON,1);
    CHECK_SIZEOF(SystemVIAPB,1);

    m_ram=m_state.ram_buffer.data();

    m_state.cpu.context=this;

#if BBCMICRO_DEBUGGER
    //for(uint16_t i=0;i<NUM_DEBUG_FLAGS_PAGES;++i) {
    //    m_debug_flags_pages[i].flat_page=i;
    //}
#endif

    for(int i=0;i<3;++i) {
        m_hw_rmmio_fns[i]=std::vector<ReadMMIOFn>(256);
        m_hw_wmmio_fns[i]=std::vector<WriteMMIOFn>(256);
        m_hw_mmio_fn_contexts[i]=std::vector<void *>(256);
        m_hw_mmio_stretch[i]=std::vector<uint8_t>(256);

        // Assume hardware is mapped. It will get fixed up later if
        // not.
        m_rmmio_fns[i]=m_hw_rmmio_fns[i].data();
        m_mmio_fn_contexts[i]=m_hw_mmio_fn_contexts[i].data();
        m_mmio_stretch[i]=m_hw_mmio_stretch[i].data();
    }

    for(int i=0;i<128;++i) {
        m_pages.r[0x00+i]=m_pages.w[0x00+i]=&m_ram[i*256];
#if BBCMICRO_DEBUGGER
        m_pages.debug_page_index[0x00+i]=(uint16_t)i;
#endif
    }

    //for(int i=0;i<128;++i) {
    //    m_pages.w[0x80+i]=g_rom_writes;
    //    m_pages.r[0x80+i]=g_unmapped_rom_reads;
    //}

    // initially no I/O
    for(uint16_t i=0xfc00;i<0xff00;++i) {
        this->SetMMIOFns(i,nullptr,nullptr,nullptr);
        this->SetMMIOCycleStretch(i,i<0xfe00);
    }

#if BBCMICRO_DEBUGGER
    for(size_t i=0;i<sizeof m_state.async_call_thunk_buf;++i) {
        this->SetMMIOFns((uint16_t)(ASYNC_CALL_THUNK_ADDR.w+i),&ReadAsyncCallThunk,nullptr,this);
    }
#endif

    if(m_ext_mem) {
        m_state.ext_mem.AllocateBuffer();

        this->SetMMIOFns(0xfc00,nullptr,&ExtMem::WriteAddressL,&m_state.ext_mem);
        this->SetMMIOFns(0xfc01,nullptr,&ExtMem::WriteAddressH,&m_state.ext_mem);
        this->SetMMIOFns(0xfc02,&ExtMem::ReadAddressL,nullptr,&m_state.ext_mem);
        this->SetMMIOFns(0xfc03,&ExtMem::ReadAddressH,nullptr,&m_state.ext_mem);

        for (uint16_t i=0xfd00;i<=0xfdff;++i) {
            this->SetMMIOFns(i,&ExtMem::ReadData,&ExtMem::WriteData,&m_state.ext_mem);
        }
    }

    // I/O: VIAs
    for(uint16_t i=0;i<32;++i) {
        this->SetMMIOFns(0xfe40+i,g_R6522_read_fns[i&15],g_R6522_write_fns[i&15],&m_state.system_via);
        this->SetMMIOCycleStretch(0xfe40+i,1);

        this->SetMMIOFns(0xfe60+i,g_R6522_read_fns[i&15],g_R6522_write_fns[i&15],&m_state.user_via);
        this->SetMMIOCycleStretch(0xfe60+i,1);
    }

    // I/O: 6845
    for(int i=0;i<8;i+=2) {
        this->SetMMIOFns((uint16_t)(0xfe00+i+0),&CRTC::ReadAddress,&CRTC::WriteAddress,&m_state.crtc);
        this->SetMMIOFns((uint16_t)(0xfe00+i+1),&CRTC::ReadData,&CRTC::WriteData,&m_state.crtc);
    }

    // I/O: Video ULA
    for(int i=0;i<2;++i) {
        this->SetMMIOFns((uint16_t)(0xfe20+i*2),nullptr,&VideoULA::WriteControlRegister,&m_state.video_ula);
        this->SetMMIOFns((uint16_t)(0xfe21+i*2),nullptr,&VideoULA::WritePalette,&m_state.video_ula);
    }

    if(m_video_nula) {
        this->SetMMIOFns(0xfe22,nullptr,&VideoULA::WriteNuLAControlRegister,&m_state.video_ula);
        this->SetMMIOFns(0xfe23,nullptr,&VideoULA::WriteNuLAPalette,&m_state.video_ula);
    }

    // I/O: disc interface
    if(m_disc_interface) {
        m_state.fdc.SetHandler(this);
        m_state.fdc.SetNoINTRQ(!!(m_disc_interface->flags&DiscInterfaceFlag_NoINTRQ));
        m_state.fdc.Set1772(!!(m_disc_interface->flags&DiscInterfaceFlag_1772));

        M6502Word c={m_disc_interface->control_addr};
        c.b.h-=0xfc;
        ASSERT(c.b.h<3);

        M6502Word f={m_disc_interface->fdc_addr};
        f.b.h-=0xfc;
        ASSERT(f.b.h<3);

        for(int i=0;i<4;++i) {
            this->SetMMIOFns((uint16_t)(m_disc_interface->fdc_addr+i),g_WD1770_read_fns[i],g_WD1770_write_fns[i],&m_state.fdc);

            if(m_disc_interface->flags&DiscInterfaceFlag_CycleStretch) {
                this->SetMMIOCycleStretch((uint16_t)(m_disc_interface->fdc_addr+i),1);
            }
        }

        this->SetMMIOFns(m_disc_interface->control_addr,&Read1770ControlRegister,&Write1770ControlRegister,this);

        m_disc_interface->InstallExtraHardware(this);
    } else {
        m_state.fdc.SetHandler(nullptr);
    }

    // I/O: additional cycle-stretched regions.
    for(uint16_t a=0xfe00;a<0xfe20;++a) {
        this->SetMMIOCycleStretch(a,1);
    }

    m_state.system_via.SetID(BBCMicroVIAID_SystemVIA,"SystemVIA");
    m_state.user_via.SetID(BBCMicroVIAID_UserVIA,"UserVIA");

    m_state.old_system_via_pb=m_state.system_via.b.p;

    // Fill in shadow RAM stuff.
    if(m_state.ram_buffer.size()>=65536) {
        m_shadow_pages=new MemoryPages(m_pages);

        for(uint8_t page=0x30;page<0x80;++page) {
            m_shadow_pages->r[page]+=0x8000;
            m_shadow_pages->w[page]+=0x8000;
#if BBCMICRO_DEBUGGER
            m_shadow_pages->debug_page_index[page]=DEBUG_SHADOW_RAM_PAGE+page-0x30;
#endif
        }

        m_pc_pages=new const MemoryPages *[256];

        for(size_t i=0;i<256;++i) {
            m_pc_pages[i]=&m_pages;
        }
    }

    if(m_beeblink_handler) {
        m_beeblink=std::make_unique<BeebLink>(m_beeblink_handler);
    }

    this->UpdateCPUDataBusFn();

    switch(m_type) {
    default:
        ASSERT(false);
        // fall through
    case BBCMicroType_B:
        m_update_romsel_pages_fn=&UpdateBROMSELPages;
        m_romsel_mask=0x0f;
        m_update_acccon_pages_fn=&UpdateBACCCONPages;
        m_acccon_mask=0;
        m_teletext_bases[0]=0x3c00;
        m_teletext_bases[1]=0x7c00;
        for(uint16_t i=0;i<16;++i) {
            this->SetMMIOFns((uint16_t)(0xfe30+i),&ReadROMSEL,&WriteROMSEL,this);
        }
        break;

    case BBCMicroType_BPlus:
        m_update_romsel_pages_fn=&UpdateBPlusROMSELPages;
        m_romsel_mask=0x8f;
        m_update_acccon_pages_fn=&UpdateBPlusACCCONPages;
        m_acccon_mask=0x80;
        m_teletext_bases[0]=0x3c00;
        m_teletext_bases[1]=0x7c00;
    romsel_and_acccon:
        for(uint16_t i=0;i<4;++i) {
            this->SetMMIOFns((uint16_t)(0xfe30+i),&ReadROMSEL,&WriteROMSEL,this);
            this->SetMMIOFns((uint16_t)(0xfe34+i),&ReadACCCON,&WriteACCCON,this);
        }
        break;

    case BBCMicroType_Master:
        for(int i=0;i<3;++i) {
            m_rom_rmmio_fns=std::vector<ReadMMIOFn>(256,&ReadROMMMIO);
            m_rom_mmio_fn_contexts=std::vector<void *>(256,this);
            m_rom_mmio_stretch=std::vector<uint8_t>(256,0x00);
        }

        m_has_rtc=true;
        m_update_romsel_pages_fn=&UpdateMaster128ROMSELPages;
        m_romsel_mask=0x8f;
        m_update_acccon_pages_fn=&UpdateMaster128ACCCONPages;
        m_acccon_mask=0xff;
        m_teletext_bases[0]=0x7c00;
        m_teletext_bases[1]=0x7c00;
        goto romsel_and_acccon;
    }

    // Page in current ROM bank and sort out ACCCON.
    this->InitROMPages();

#if BBCMICRO_ENABLE_DISC_DRIVE_SOUND
    switch(m_type) {
    default:
        ASSERT(false);
        // fall through
    case BBCMicroType_B:
    case BBCMicroType_BPlus:
    case BBCMicroType_Master:
        this->InitDiscDriveSounds(DiscDriveType_133mm);
        break;
    }
#endif

#if BBCMICRO_TRACE
    this->SetTrace(nullptr,0);
#endif

    for(int i=0;i<3;++i) {
        ASSERT(m_rmmio_fns[i]==m_hw_rmmio_fns[i].data()||m_rmmio_fns[i]==m_rom_rmmio_fns.data());
        ASSERT(m_mmio_fn_contexts[i]==m_hw_mmio_fn_contexts[i].data()||m_mmio_fn_contexts[i]==m_rom_mmio_fn_contexts.data());
        ASSERT(m_mmio_stretch[i]==m_hw_mmio_stretch[i].data()||m_mmio_stretch[i]==m_rom_mmio_stretch.data());
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool BBCMicro::IsTrack0() {
    if(DiscDrive *dd=this->GetDiscDrive()) {
        return dd->track==0;
    }

    return false;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BBCMicro::StepOut() {
    if(DiscDrive *dd=this->GetDiscDrive()) {
        if(dd->track>0) {
            --dd->track;

#if BBCMICRO_ENABLE_DISC_DRIVE_SOUND
            this->StepSound(dd);
#endif
        }
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BBCMicro::StepIn() {
    if(DiscDrive *dd=this->GetDiscDrive()) {
        if(dd->track<255) {
            ++dd->track;

#if BBCMICRO_ENABLE_DISC_DRIVE_SOUND
            this->StepSound(dd);
#endif
        }
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BBCMicro::SpinUp() {
    if(DiscDrive *dd=this->GetDiscDrive()) {
        dd->motor=true;

#if BBCMICRO_ENABLE_DISC_DRIVE_SOUND
        dd->spin_sound_index=0;
        dd->spin_sound=DiscDriveSound_SpinStartLoaded;
#endif
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BBCMicro::SpinDown() {
    if(DiscDrive *dd=this->GetDiscDrive()) {
        dd->motor=false;

#if BBCMICRO_ENABLE_DISC_DRIVE_SOUND
        dd->spin_sound_index=0;
        dd->spin_sound=DiscDriveSound_SpinEnd;
#endif
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool BBCMicro::IsWriteProtected() {
    if(this->GetDiscDrive()) {
        if(m_disc_images[m_state.disc_control.drive]) {
            if(m_disc_images[m_state.disc_control.drive]->IsWriteProtected()) {
                return true;
            }
        }
    }

    return false;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool BBCMicro::GetByte(uint8_t *value,uint8_t sector,size_t offset) {
    if(DiscDrive *dd=this->GetDiscDrive()) {
        m_disc_access=true;

        if(m_disc_images[m_state.disc_control.drive]) {
            if(m_disc_images[m_state.disc_control.drive]->Read(value,
                                                               m_state.disc_control.side,
                                                               dd->track,
                                                               sector,
                                                               offset))
            {
                return true;
            }
        }
    }

    return false;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool BBCMicro::SetByte(uint8_t sector,size_t offset,uint8_t value) {
    if(DiscDrive *dd=this->GetDiscDrive()) {
        m_disc_access=true;

        if(m_disc_images[m_state.disc_control.drive]) {
            if(m_disc_images[m_state.disc_control.drive]->Write(m_state.disc_control.side,
                                                                dd->track,
                                                                sector,
                                                                offset,
                                                                value))
            {
                return true;
            }
        }
    }

    return false;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool BBCMicro::GetSectorDetails(uint8_t *track,uint8_t *side,size_t *size,uint8_t sector,bool double_density) {
    if(DiscDrive *dd=this->GetDiscDrive()) {
        m_disc_access=true;

        if(m_disc_images[m_state.disc_control.drive]) {
            if(m_disc_images[m_state.disc_control.drive]->GetDiscSectorSize(size,m_state.disc_control.side,dd->track,sector,double_density)) {
                *track=dd->track;
                *side=m_state.disc_control.side;
                return true;
            }
        }
    }

    return false;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

BBCMicro::DiscDrive *BBCMicro::GetDiscDrive() {
    if(m_state.disc_control.drive>=0&&m_state.disc_control.drive<NUM_DRIVES) {
        return &m_state.drives[m_state.disc_control.drive];
    } else {
        return nullptr;
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if BBCMICRO_ENABLE_DISC_DRIVE_SOUND
void BBCMicro::InitDiscDriveSounds(DiscDriveType type) {
    auto &&it=g_disc_drive_sounds.find(type);
    if(it==g_disc_drive_sounds.end()) {
        return;
    }

    for(size_t i=0;i<DiscDriveSound_EndValue;++i) {
        m_disc_drive_sounds[i]=&it->second[i];
    }
}
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if BBCMICRO_ENABLE_DISC_DRIVE_SOUND

// As per http://www.ninerpedia.org/index.php?title=MAME_Floppy_sound_emulation

struct SeekSound {
    size_t clock_ticks;
    DiscDriveSound sound;
};

#define SEEK_SOUND(N) {SOUND_CLOCKS_FROM_MS(N),DiscDriveSound_Seek##N##ms,}

static const SeekSound g_seek_sounds[]={
    {SOUND_CLOCKS_FROM_MS(17),DiscDriveSound_Seek20ms,},
    {SOUND_CLOCKS_FROM_MS(10),DiscDriveSound_Seek12ms,},
    {SOUND_CLOCKS_FROM_MS(4),DiscDriveSound_Seek6ms,},
    {1,DiscDriveSound_Seek2ms,},
    {0},
};

void BBCMicro::StepSound(DiscDrive *dd) {
    if(dd->step_sound_index<0) {
        // step
        dd->step_sound_index=0;
    } else if(dd->seek_sound==DiscDriveSound_EndValue) {
        // skip a bit of the step sound
        dd->step_sound_index+=SOUND_CLOCK_HZ/100;

        // seek. Start with 20ms... it's as good a guess as any.
        dd->seek_sound=DiscDriveSound_Seek20ms;
        dd->seek_sound_index=0;
    } else {
        for(const SeekSound *seek_sound=g_seek_sounds;seek_sound->clock_ticks!=0;++seek_sound) {
            if(dd->seek_sound_index>=seek_sound->clock_ticks) {
                if(dd->seek_sound!=seek_sound->sound) {
                    dd->seek_sound=seek_sound->sound;
                    dd->seek_sound_index=0;
                    break;
                }
            }
        }
    }
}
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if BBCMICRO_ENABLE_DISC_DRIVE_SOUND
float BBCMicro::UpdateDiscDriveSound(DiscDrive *dd) {
    float acc=0.f;

    if(dd->spin_sound!=DiscDriveSound_EndValue) {
        ASSERT(dd->spin_sound>=0&&dd->spin_sound<DiscDriveSound_EndValue);
        const std::vector<float> *spin_sound=m_disc_drive_sounds[dd->spin_sound];

        acc+=(*spin_sound)[dd->spin_sound_index];

        ++dd->spin_sound_index;
        if(dd->spin_sound_index>=spin_sound->size()) {
            switch(dd->spin_sound) {
            case DiscDriveSound_SpinStartEmpty:
            case DiscDriveSound_SpinEmpty:
                dd->spin_sound=DiscDriveSound_SpinEmpty;
                break;

            case DiscDriveSound_SpinStartLoaded:
            case DiscDriveSound_SpinLoaded:
                dd->spin_sound=DiscDriveSound_SpinLoaded;
                break;

            default:
                dd->spin_sound=DiscDriveSound_EndValue;
                break;
            }

            dd->spin_sound_index=0;
        }
    }

    if(dd->seek_sound!=DiscDriveSound_EndValue) {
        const std::vector<float> *seek_sound=m_disc_drive_sounds[dd->seek_sound];

        acc+=(*seek_sound)[dd->seek_sound_index];

        ++dd->seek_sound_index;
        if((size_t)dd->seek_sound_index>=seek_sound->size()) {
            dd->seek_sound=DiscDriveSound_EndValue;
        }
    } else if(dd->step_sound_index>=0) {
        const std::vector<float> *step_sound=m_disc_drive_sounds[DiscDriveSound_Step];

        // check for end first as the playback position is adjusted in
        // StepSound.
        if((size_t)dd->step_sound_index>=step_sound->size()) {
            dd->step_sound_index=-1;
        } else {
            acc+=(*step_sound)[(size_t)dd->step_sound_index++];
        }
    }

    return acc;
}
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BBCMicro::UpdateCPUDataBusFn() {
    if(m_state.ram_buffer.size()>=65536) {
        m_default_handle_cpu_data_bus_fn=
#if BBCMICRO_DEBUGGER
            m_debug?&HandleCPUDataBusWithShadowRAMDebug:
#endif
            &HandleCPUDataBusWithShadowRAM;
    } else {
        m_default_handle_cpu_data_bus_fn=
#if BBCMICRO_DEBUGGER
            m_debug?&HandleCPUDataBusMainRAMOnlyDebug:
#endif
            &HandleCPUDataBusMainRAMOnly;
    }

    if(m_state.hack_flags!=0) {
        goto hack;
    }

#if BBCMICRO_TRACE
    if(m_trace) {
        goto hack;
    }
#endif

#if BBCMICRO_DEBUGGER
    if(m_debug) {
        if(m_debug->step_type!=BBCMicroStepType_None) {
            goto hack;
        }
    }
#endif

    if(!m_instruction_fns.empty()) {
        goto hack;
    }

#if BBCMICRO_DEBUGGER
    if(m_state.async_call_address.w!=INVALID_ASYNC_CALL_ADDRESS) {
        goto hack;
    }
#endif
    
    // No hacks.
    m_handle_cpu_data_bus_fn=m_default_handle_cpu_data_bus_fn;
    return;

hack:;
    m_handle_cpu_data_bus_fn=&HandleCPUDataBusWithHacks;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
