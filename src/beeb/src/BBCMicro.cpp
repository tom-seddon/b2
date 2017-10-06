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
#include <beeb/Trace.h>
#include <memory>
#include <vector>
#include <beeb/DiscImage.h>
#include <map>
#include <limits.h>
#include <algorithm>

#include <shared/enum_decl.h>
#include "BBCMicro_private.inl"
#include <shared/enum_end.h>

#include <shared/enum_def.h>
#include <beeb/BBCMicro.inl>
#include "BBCMicro_private.inl"
#include <shared/enum_end.h>

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#define BIT(X,N) (((X)&1<<N)?'1':'0')
#define BITS(X) {BIT(X,7),BIT(X,6),BIT(X,5),BIT(X,4),BIT(X,3),BIT(X,2),BIT(X,1),BIT(X,0),0},
#define AABBCCDD(A,Y,Z,W) BITS(((A)<<6|(Y)<<4|(Z)<<2|(W)))
#define AABBCC__(A,B,C) AABBCCDD(A,B,C,0) AABBCCDD(A,B,C,1) AABBCCDD(A,B,C,2) AABBCCDD(A,B,C,3)
#define AABB____(A,B)   AABBCC__(A,B,0)   AABBCC__(A,B,1)   AABBCC__(A,B,2)   AABBCC__(A,B,3) 
#define AA______(A)     AABB____(A,0)     AABB____(A,1)     AABB____(A,2)     AABB____(A,3)

static const char BINARY_BYTE_STRINGS[256][9]={AA______(0) AA______(1) AA______(2) AA______(3)};

#undef AA______
#undef AABB____
#undef AABBCC__
#undef AABBCCDD
#undef BITS
#undef BIT

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static const std::unique_ptr<DiscImage> NULL_DISCIMAGE_PTR;
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
const TraceEventType BBCMicro::INITIAL_EVENT("BBCMicroInitial",sizeof(InitialTraceEvent));
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

static const uint16_t SCREEN_WRAP_ADJUSTMENTS[]={
    0x4000>>3,
    0x2000>>3,
    0x5000>>3,
    0x2800>>3
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

BBCMicro::State::State(const BBCMicroType type,const std::vector<uint8_t> &nvram_contents,const tm *rtc_time) {
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

        if(nvram_contents.size()==MC146818::RAM_SIZE) {
            this->rtc.SetRAMContents(nvram_contents.data());
            //memcpy(&this->rtc.regs.bits.ram,nvram_contents.data(),nvram_contents.size());
        }

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

BBCMicro::BBCMicro(BBCMicroType type,const DiscInterfaceDef *def,const std::vector<uint8_t> &nvram_contents,const tm *rtc_time,bool video_nula):
    m_state(type,nvram_contents,rtc_time),
    m_type(type),
    m_disc_interface(def->create_fun()),
    m_video_nula(video_nula)
{
    this->InitStuff();
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

BBCMicro::BBCMicro(const BBCMicro &src):
    m_state(src.m_state),
    m_type(src.m_type),
    m_disc_interface(src.m_disc_interface?src.m_disc_interface->Clone():nullptr),
    m_video_nula(src.m_video_nula)
{
    for(int i=0;i<2;++i) {
        std::unique_lock<std::mutex> lock;
        std::shared_ptr<DiscImage> disc_image=DiscImage::Clone(src.GetDiscImage(&lock,i));
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

std::unique_ptr<BBCMicro> BBCMicro::Clone() const {
    return std::unique_ptr<BBCMicro>(new BBCMicro(*this));
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if BBCMICRO_TRACE
void BBCMicro::SetTrace(std::shared_ptr<Trace> trace,uint32_t trace_flags) {
    m_trace_ptr=std::move(trace);
    m_trace=m_trace_ptr.get();
    m_trace_current_instruction=NULL;
    m_trace_flags=trace_flags;

    if(m_trace) {
        m_trace->SetTime(&m_state.num_2MHz_cycles);

        auto e=(InitialTraceEvent *)m_trace->AllocEvent(INITIAL_EVENT);
        e->config=m_state.cpu.config;
    }

    m_state.fdc.SetTrace(trace_flags&BBCMicroTraceFlag_1770?m_trace:NULL);
    m_state.rtc.SetTrace(trace_flags&BBCMicroTraceFlag_RTC?m_trace:NULL);
    m_state.crtc.SetTrace(
        (trace_flags&(BBCMicroTraceFlag_6845VSync|BBCMicroTraceFlag_6845Scanlines))?m_trace:NULL,
        !!(trace_flags&BBCMicroTraceFlag_6845Scanlines));
    m_state.system_via.SetTrace(trace_flags&BBCMicroTraceFlag_SystemVIA?m_trace:NULL);
    m_state.user_via.SetTrace(trace_flags&BBCMicroTraceFlag_UserVIA?m_trace:NULL);

    this->UpdateCPUDataBusFn();
}
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BBCMicro::SetPages(uint8_t page_,size_t num_pages,const uint8_t *read_data,size_t read_page_stride,uint8_t *write_data,size_t write_page_stride) {
    ASSERT(read_page_stride==256||read_page_stride==0);
    ASSERT(write_page_stride==256||write_page_stride==0);

    uint8_t page=page_;

    if(m_shadow_pages) {
        for(size_t i=0;i<num_pages;++i) {
            m_shadow_pages->r[page]=m_pages.r[page]=read_data;
            m_shadow_pages->w[page]=m_pages.w[page]=write_data;

            ++page;
            read_data+=read_page_stride;
            write_data+=write_page_stride;
        }
    } else {
        for(size_t i=0;i<num_pages;++i) {
            m_pages.r[page]=read_data;
            m_pages.w[page]=write_data;

            ++page;
            read_data+=read_page_stride;
            write_data+=write_page_stride;
        }
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BBCMicro::SetOSPages(uint8_t dest_page,uint8_t src_page,uint8_t num_pages) {
    if(!!m_state.os_buffer) {
        const uint8_t *data=m_state.os_buffer->data();
        this->SetPages(dest_page,num_pages,data+src_page*256,256,g_rom_writes,0);
    } else {
        this->SetPages(dest_page,num_pages,g_unmapped_rom_reads,0,g_rom_writes,0);
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BBCMicro::SetROMPages(uint8_t bank,uint8_t page,size_t src_page,size_t num_pages) {
    ASSERT(bank<16);
    if(!!m_state.sideways_rom_buffers[bank]) {
        const uint8_t *data=m_state.sideways_rom_buffers[bank]->data()+src_page*256;
        this->SetPages(page,num_pages,data,256,g_rom_writes,0);
    } else if(!m_state.sideways_ram_buffers[bank].empty()) {
        uint8_t *data=m_state.sideways_ram_buffers[bank].data()+src_page*256;
        this->SetPages(page,num_pages,data,256,data,256);
    } else {
        this->SetPages(page,num_pages,g_unmapped_rom_reads,0,g_rom_writes,0);
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
        m->SetPages(0x80,0x30,m->m_ram+0x8000,256,m->m_ram+0x8000,256);
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

#define VDU_RAM_OFFSET (0x8000u+0u)
#define NUM_VDU_RAM_PAGES (0x10u)
#define FS_RAM_OFFSET (0x8000+0x1000u)
#define NUM_FS_RAM_PAGES (0x20u)

void BBCMicro::UpdateMaster128ROMSELPages(BBCMicro *m) {
    if(m->m_state.romsel.m128_bits.ram) {
        m->SetPages(0x80,NUM_VDU_RAM_PAGES,m->m_ram+VDU_RAM_OFFSET,256,m->m_ram+VDU_RAM_OFFSET,256);
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
            m->SetPages(0xc0,NUM_FS_RAM_PAGES,m->m_ram+FS_RAM_OFFSET,256,m->m_ram+FS_RAM_OFFSET,256);
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

void BBCMicro::CallNVRAMCallback(size_t offset,uint8_t value) {
    if(m_nvram_changed_fn) {
        (*m_nvram_changed_fn)(this,offset,value,m_nvram_changed_context);
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if BBCMICRO_TRACE
void BBCMicro::TracePortB(SystemVIAPB pb) {
    if(m_trace) {
        {
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
        }

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

    //printf("PA=0x%02X addressable latch: !sndw=%d !kbw=%d base=$%04X\n",value,m_state.addressable_latch.bits.not_sound_write,m_state.addressable_latch.bits.not_kb_write,SCREEN_BASES[m_state.addressable_latch.bits.screen_base].w);
}
#endif

void BBCMicro::HandleSystemVIAB(R6522 *via,uint8_t value,uint8_t old_value,void *m_) {
    (void)via,(void)old_value;

    auto m=(BBCMicro *)m_;

    SystemVIAPB pb;
    pb.value=value;

    SystemVIAPB old_pb;
    old_pb.value=old_value;

    uint8_t mask=1<<pb.bits.latch_index;

    m->m_state.addressable_latch.value&=~mask;
    if(pb.bits.latch_value) {
        m->m_state.addressable_latch.value|=mask;
    }

#if BBCMICRO_TRACE
    m->TracePortB(pb);
#endif

    if(pb.m128_bits.rtc_chip_select) {
        uint8_t x=m->m_state.system_via.a.p;

        if(old_pb.m128_bits.rtc_address_strobe==1&&pb.m128_bits.rtc_address_strobe==0) {
            m->m_state.rtc.SetAddress(x);
        }

        AddressableLatch test;
        test.value=m->m_state.old_addressable_latch.value^m->m_state.addressable_latch.value;
        if(test.m128_bits.rtc_data_strobe) {
            if(m->m_state.addressable_latch.m128_bits.rtc_data_strobe) {
                // 0->1
                if(m->m_state.addressable_latch.m128_bits.rtc_read) {
                    m->m_state.system_via.a.p=m->m_state.rtc.Read();
                }
            } else {
                // 1->0
                if(!m->m_state.addressable_latch.m128_bits.rtc_read) {
                    int ram_address=m->m_state.rtc.SetData(x);
                    if(ram_address>=0) {
                        m->CallNVRAMCallback((size_t)ram_address,x);
                    }
                }
            }
        }
    }
}

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

const uint64_t *BBCMicro::GetNum2MHzCycles() {
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

uint8_t BBCMicro::ReadMemory(uint16_t address) {
    M6502Word addr={address};
    if(addr.b.h>=0xfc&&addr.b.h<0xff) {
        return 0;
    } else if(m_pc_pages) {
        return m_pc_pages[0]->r[addr.b.h][addr.b.l];
    } else {
        return m_pages.r[addr.b.h][addr.b.l];
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

const uint8_t *BBCMicro::GetRAM() {
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
            //printf("%d keys down\n",m_num_keys_down);

            return true;
        } else if(old_state&&!new_state) {
            ASSERT(m_state.num_keys_down>0);
            --m_state.num_keys_down;
            *column&=~mask;
            //printf("%d keys down\n",m_num_keys_down);

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

uint32_t BBCMicro::GetDebugFlags() {
    uint32_t result=0;

    if(m_state.saa5050.IsDebug()) {
        result|=BBCMicroDebugFlag_TeletextDebug;
    }

#if !BBCMICRO_FINER_TELETEXT
    if(m_state.saa5050.IsAA()) {
        result|=BBCMicroDebugFlag_TeletextInterlace;
    }
#endif

    return result;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BBCMicro::SetDebugFlags(uint32_t flags) {
    m_state.saa5050.SetDebug(!!(flags&BBCMicroDebugFlag_TeletextDebug));

#if !BBCMICRO_FINER_TELETEXT
    m_state.saa5050.SetAA(!!(flags&BBCMicroDebugFlag_TeletextInterlace));
#endif
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BBCMicro::UpdateKeyboardMatrix() {
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
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BBCMicro::UpdateJoysticks() {
    m_state.system_via.b.p|=1<<4|1<<5;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool BBCMicro::UpdateSound(SoundDataUnit *sound_unit) {
    if(m_state.addressable_latch.bits.not_sound_write==0&&m_state.old_addressable_latch.bits.not_sound_write==1) {
        m_state.sn76489.Write(m_state.system_via.a.p);
    }

    if((m_state.num_2MHz_cycles&((1<<SOUND_CLOCK_SHIFT)-1))==0) {
        sound_unit->sn_output=m_state.sn76489.Update();

#if BBCMICRO_ENABLE_DISC_DRIVE_SOUND
        // The disc drive sounds are pretty quiet. 
        sound_unit->disc_drive_sound=this->UpdateDiscDriveSound(&m_state.drives[0]);
        sound_unit->disc_drive_sound+=this->UpdateDiscDriveSound(&m_state.drives[1]);
#endif
        return true;
    } else {
        return false;
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool BBCMicro::PreUpdateCPU(uint8_t num_stretch_cycles) {
    if(m_state.stretched_cycles_left>0) {
        --m_state.stretched_cycles_left;
    } else {
        (*m_state.cpu.tfn)(&m_state.cpu);

        uint8_t mmio_page=m_state.cpu.abus.b.h-0xfc;
        if(mmio_page<3) {
            if(m_state.cpu.read) {
                m_state.stretched_cycles_left=num_stretch_cycles&m_mmio_stretch[mmio_page][m_state.cpu.abus.b.l];
            } else {
                m_state.stretched_cycles_left=num_stretch_cycles&m_hw_mmio_stretch[mmio_page][m_state.cpu.abus.b.l];
            }
        }
    }

    if(m_state.stretched_cycles_left>0) {
        return false;
    }

    uint8_t mmio_page=m_state.cpu.abus.b.h-0xfc;
    if(mmio_page<3) {
        if(m_state.cpu.read) {
            ReadMMIOFn fn=m_rmmio_fns[mmio_page][m_state.cpu.abus.b.l];
            void *context=m_mmio_fn_contexts[mmio_page][m_state.cpu.abus.b.l];
            m_state.cpu.dbus=(*fn)(context,m_state.cpu.abus);
        } else {
            WriteMMIOFn fn=m_hw_wmmio_fns[mmio_page][m_state.cpu.abus.b.l];
            void *context=m_hw_mmio_fn_contexts[mmio_page][m_state.cpu.abus.b.l];
            (*fn)(context,m_state.cpu.abus,m_state.cpu.dbus);
        }

        return false;
    }

    return true;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BBCMicro::HandleCPUDataBusMainRAMOnly(BBCMicro *m) {
    if(m->m_state.cpu.read) {
        m->m_state.cpu.dbus=m->m_pages.r[m->m_state.cpu.abus.b.h][m->m_state.cpu.abus.b.l];
    } else {
        m->m_pages.w[m->m_state.cpu.abus.b.h][m->m_state.cpu.abus.b.l]=m->m_state.cpu.dbus;
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BBCMicro::HandleCPUDataBusWithShadowRAM(BBCMicro *m) {
    if(m->m_state.cpu.read) {
        m->m_state.cpu.dbus=m->m_pc_pages[m->m_state.cpu.opcode_pc.b.h]->r[m->m_state.cpu.abus.b.h][m->m_state.cpu.abus.b.l];
    } else {
        m->m_pc_pages[m->m_state.cpu.opcode_pc.b.h]->w[m->m_state.cpu.abus.b.h][m->m_state.cpu.abus.b.l]=m->m_state.cpu.dbus;
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BBCMicro::HandleCPUDataBusWithHacks(BBCMicro *m) {
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
    }
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

void BBCMicro::UpdateVideoHardware() {
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
            m_state.video_ula.Byte(m_ram[addr]);
        }

        if(output.cudisp) {
            m_state.cursor_pattern=CURSOR_PATTERNS[m_state.video_ula.control.bits.cursor];
        }
    }

    m_state.crtc_last_output=output;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BBCMicro::UpdateDisplayOutput(VideoDataHalfUnit *hu) {
    if(m_state.crtc_last_output.hsync) {
        hu->type=BeebControlPixel_HSync;
    } else if(m_state.crtc_last_output.vsync) {
        hu->type=BeebControlPixel_VSync;
    } else if(m_state.crtc_last_output.display) {
        if(m_state.video_ula.control.bits.teletext) {
            m_state.saa5050.EmitVideoDataHalfUnit(hu);
#if BBCMICRO_FINER_TELETEXT

            if(m_state.cursor_pattern&1) {
                hu->teletext.colours[0]^=7;
                hu->teletext.colours[1]^=7;
            }

#else
            goto xor_pixels_with_cursor;
#endif
        } else {
            if(m_state.crtc_last_output.raster<8) {
                (m_state.video_ula.*VideoULA::EMIT_MFNS[m_state.video_ula.control.bits.line_width])(hu);

#if !BBCMICRO_FINER_TELETEXT
                xor_pixels_with_cursor:;
#else
                ;//fix VC++ indentation bug
#endif
                if(m_state.cursor_pattern&1) {
                    for(size_t i=0;i<sizeof hu->bitmap.pixels;++i) {
                        hu->bitmap.pixels[i]^=0x0fff;
                    }
                }
            } else {
                hu->type=BeebControlPixel_Nothing^(m_state.cursor_pattern&1);
            }
        }
    } else {
        hu->type=BeebControlPixel_Nothing;
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool BBCMicro::Update(VideoDataUnit *video_unit,SoundDataUnit *sound_unit) {
    // Cycle 1
    ++m_state.num_2MHz_cycles;
    if(this->PreUpdateCPU(1)) {
        (*m_handle_cpu_data_bus_fn)(this);
    }

    if(m_state.video_ula.control.bits.fast_6845) {
        this->UpdateVideoHardware();
    }
    this->UpdateDisplayOutput(&video_unit->a);

    // Cycle 2
    ++m_state.num_2MHz_cycles;
    if(this->PreUpdateCPU(2)) {
        (*m_handle_cpu_data_bus_fn)(this);
    }

    this->UpdateVideoHardware();
    this->UpdateDisplayOutput(&video_unit->b);
    this->UpdateKeyboardMatrix();
    this->UpdateJoysticks();
    M6502_SetDeviceIRQ(&m_state.cpu,BBCMicroIRQDevice_SystemVIA,m_state.system_via.Update());
    M6502_SetDeviceIRQ(&m_state.cpu,BBCMicroIRQDevice_UserVIA,m_state.user_via.Update());

    if(m_has_rtc) {
        m_state.rtc.Update();
    }

    M6502_SetDeviceNMI(&m_state.cpu,BBCMicroNMIDevice_1770,m_state.fdc.Update().value);
    bool sound=this->UpdateSound(sound_unit);

    m_state.old_addressable_latch=m_state.addressable_latch;

    return sound;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

//void BBCMicro::SetDiscDriveCallbacks(const struct DiscDriveCallbacks *callbacks) {
//    m_disc_drive_callbacks=*callbacks;
//    //for(int i=0;i<NUM_DRIVES;++i) {
//    //    DiscDrive_SetCallbacks(&m_state.drives[i],callbacks);
//    //}
//}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if BBCMICRO_TURBO_DISC
bool BBCMicro::GetTurboDisc() {
    return !!m_state.fdc.IsTurbo();
}
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if BBCMICRO_TURBO_DISC
void BBCMicro::HandleTurboRTI(M6502 *cpu) {
    BBCMicro *m=(BBCMicro *)cpu->context;

    ASSERT(m->m_state.fdc.IsTurbo());
    m->m_state.fdc.TurboAck();
}
#endif

#if BBCMICRO_TURBO_DISC
void BBCMicro::SetTurboDisc(int turbo) {
    if(turbo) {
        m_state.cpu.rti_fn=&HandleTurboRTI;
    } else {
        m_state.cpu.rti_fn=NULL;
    }

    m_state.fdc.SetTurbo(!!turbo);
}
#endif

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

const uint8_t *BBCMicro::GetNVRAM() {
    if(m_has_rtc) {
        return m_state.rtc.GetRAM();
    } else {
        return NULL;
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

size_t BBCMicro::GetNVRAMSize() const {
    if(m_has_rtc) {
        return MC146818::RAM_SIZE;
    } else {
        return 0;
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BBCMicro::SetNVRAMCallback(NVRAMChangedFn nvram_changed_fn,void *context) {
    m_nvram_changed_fn=nvram_changed_fn;
    m_nvram_changed_context=context;

    const uint8_t *p=this->GetNVRAM();

    if(m_has_rtc) {
        for(size_t i=0;i<MC146818::RAM_SIZE;++i) {
            this->CallNVRAMCallback(i,p[i]);
        }
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
void BBCMicro::StartTrace(uint32_t trace_flags) {
    this->StopTrace();

    this->SetTrace(std::make_shared<Trace>(),trace_flags);
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

std::shared_ptr<DiscImage> BBCMicro::GetMutableDiscImage(std::unique_lock<std::mutex> *lock,int drive) {
    if(drive>=0&&drive<NUM_DRIVES) {
        *lock=this->GetDiscLock();
        return m_disc_images[drive];
    } else {
        return nullptr;
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

std::shared_ptr<const DiscImage> BBCMicro::GetDiscImage(std::unique_lock<std::mutex> *lock,int drive) const {
    if(drive>=0&&drive<NUM_DRIVES) {
        *lock=this->GetDiscLock();
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

    auto lock=this->GetDiscLock();

    m_disc_images[drive]=std::move(disc_image);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BBCMicro::SetDiscMutex(std::mutex *mutex) {
    m_disc_mutex=mutex;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool BBCMicro::GetAndResetDiscAccessFlag() {
    std::unique_lock<std::mutex> lock=this->GetDiscLock();

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

void BBCMicro::StartPaste(std::shared_ptr<std::string> text) {
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

void BBCMicro::InitStuff() {
    CHECK_SIZEOF(AddressableLatch,1);
    CHECK_SIZEOF(ROMSEL,1);
    CHECK_SIZEOF(ACCCON,1);
    CHECK_SIZEOF(SystemVIAPB,1);

    m_ram=m_state.ram_buffer.data();

    m_state.cpu.context=this;

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
    }

    for(int i=0;i<128;++i) {
        m_pages.w[0x80+i]=g_rom_writes;
        m_pages.r[0x80+i]=g_unmapped_rom_reads;
    }

    // initially no I/O
    for(uint16_t i=0xfc00;i<0xff00;++i) {
        this->SetMMIOFns(i,nullptr,nullptr,nullptr);
        this->SetMMIOCycleStretch(i,i<0xfe00);
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

    // VIA callbacks.
    m_state.system_via.b.fn=&HandleSystemVIAB;
    m_state.system_via.b.fn_context=this;

    // Debugging aid.
    m_state.system_via.tag="SystemVIA";
    m_state.user_via.tag="UserVIA";

    // Fill in shadow RAM stuff.
    if(m_state.ram_buffer.size()>=65536) {
        m_shadow_pages=new MemoryPages(m_pages);

        for(uint8_t page=0x30;page<0x80;++page) {
            m_shadow_pages->r[page]+=0x8000;
            m_shadow_pages->w[page]+=0x8000;
        }

        m_pc_pages=new const MemoryPages *[256];

        for(size_t i=0;i<256;++i) {
            m_pc_pages[i]=&m_pages;
        }

        m_default_handle_cpu_data_bus_fn=&HandleCPUDataBusWithShadowRAM;
    } else {
        m_default_handle_cpu_data_bus_fn=&HandleCPUDataBusMainRAMOnly;
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

#if BBCMICRO_TURBO_DISC
    this->SetTurboDisc(m_state.fdc.IsTurbo());
#endif

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
    std::unique_lock<std::mutex> disc_lock;
    if(DiscDrive *dd=this->GetDiscDrive(&disc_lock)) {
        return dd->track==0;
    }

    return false;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BBCMicro::StepOut() {
    std::unique_lock<std::mutex> disc_lock;
    if(DiscDrive *dd=this->GetDiscDrive(&disc_lock)) {
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
    std::unique_lock<std::mutex> disc_lock;
    if(DiscDrive *dd=this->GetDiscDrive(&disc_lock)) {
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
    std::unique_lock<std::mutex> disc_lock;
    if(DiscDrive *dd=this->GetDiscDrive(&disc_lock)) {
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
    std::unique_lock<std::mutex> disc_lock;
    if(DiscDrive *dd=this->GetDiscDrive(&disc_lock)) {
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
    std::unique_lock<std::mutex> disc_lock;
    if(this->GetDiscDrive(&disc_lock)) {
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

bool BBCMicro::GetByte(uint8_t *value,uint8_t track,uint8_t sector,size_t offset) {
    std::unique_lock<std::mutex> disc_lock;
    if(DiscDrive *dd=this->GetDiscDrive(&disc_lock)) {
        m_disc_access=true;

        if(dd->track==track) {
            if(m_disc_images[m_state.disc_control.drive]) {
                if(m_disc_images[m_state.disc_control.drive]->Read(value,m_state.disc_control.side,track,sector,offset)) {
                    return true;
                }
            }
        }
    }

    return false;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool BBCMicro::SetByte(uint8_t track,uint8_t sector,size_t offset,uint8_t value) {
    std::unique_lock<std::mutex> disc_lock;
    if(DiscDrive *dd=this->GetDiscDrive(&disc_lock)) {
        m_disc_access=true;

        if(dd->track==track) {
            if(m_disc_images[m_state.disc_control.drive]) {
                if(m_disc_images[m_state.disc_control.drive]->Write(m_state.disc_control.side,track,sector,offset,value)) {
                    return true;
                }
            }
        }
    }

    return false;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool BBCMicro::GetSectorDetails(uint8_t *side,size_t *size,uint8_t track,uint8_t sector,bool double_density) {
    std::unique_lock<std::mutex> disc_lock;
    if(DiscDrive *dd=this->GetDiscDrive(&disc_lock)) {
        m_disc_access=true;

        if(dd->track==track) {
            if(m_disc_images[m_state.disc_control.drive]) {
                if(m_disc_images[m_state.disc_control.drive]->GetDiscSectorSize(size,m_state.disc_control.side,track,sector,double_density)) {
                    *side=m_state.disc_control.side;
                    return true;
                }
            }
        }
    }

    return false;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

std::unique_lock<std::mutex> BBCMicro::GetDiscLock() const {
    if(m_disc_mutex) {
        return std::unique_lock<std::mutex>(*m_disc_mutex);
    } else {
        return std::unique_lock<std::mutex>();
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

BBCMicro::DiscDrive *BBCMicro::GetDiscDrive(std::unique_lock<std::mutex> *lock) {
    if(m_state.disc_control.drive>=0&&m_state.disc_control.drive<NUM_DRIVES) {
        *lock=this->GetDiscLock();
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
        m_disc_drive_sounds[i]=it->second[i];
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
        const std::vector<float> *spin_sound=&m_disc_drive_sounds[dd->spin_sound];

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
        const std::vector<float> *seek_sound=&m_disc_drive_sounds[dd->seek_sound];

        acc+=(*seek_sound)[dd->seek_sound_index];

        ++dd->seek_sound_index;
        if((size_t)dd->seek_sound_index>=seek_sound->size()) {
            dd->seek_sound=DiscDriveSound_EndValue;
        }
    } else if(dd->step_sound_index>=0) {
        const std::vector<float> *step_sound=&m_disc_drive_sounds[DiscDriveSound_Step];

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
    if(m_state.hack_flags!=0||m_trace||!m_instruction_fns.empty()) {
        m_handle_cpu_data_bus_fn=&HandleCPUDataBusWithHacks;
    } else {
        m_handle_cpu_data_bus_fn=m_default_handle_cpu_data_bus_fn;
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
