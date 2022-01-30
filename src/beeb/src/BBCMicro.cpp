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
static std::map<DiscDriveType, std::array<std::vector<float>, DiscDriveSound_EndValue>> g_disc_drive_sounds;

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// The key to press to start the paste going.
static const BeebKey PASTE_START_KEY = BeebKey_Space;

// The corresponding char, so it can be removed when copying the BASIC
// listing.
const char BBCMicro::PASTE_START_CHAR = ' ';

#if BBCMICRO_TRACE
const TraceEventType BBCMicro::INSTRUCTION_EVENT("BBCMicroInstruction", sizeof(InstructionTraceEvent));
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if BBCMICRO_DEBUGGER
// The async call thunk lives in an undefined area of FRED.
static const M6502Word ASYNC_CALL_THUNK_ADDR = {0xfc50};
static const int ASYNC_CALL_TIMEOUT = 1000000;
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static const BBCMicro::WriteMMIOFn g_R6522_write_fns[16] = {
    &R6522::Write0,
    &R6522::Write1,
    &R6522::Write2,
    &R6522::Write3,
    &R6522::Write4,
    &R6522::Write5,
    &R6522::Write6,
    &R6522::Write7,
    &R6522::Write8,
    &R6522::Write9,
    &R6522::WriteA,
    &R6522::WriteB,
    &R6522::WriteC,
    &R6522::WriteD,
    &R6522::WriteE,
    &R6522::WriteF,
};

static const BBCMicro::ReadMMIOFn g_R6522_read_fns[16] = {
    &R6522::Read0,
    &R6522::Read1,
    &R6522::Read2,
    &R6522::Read3,
    &R6522::Read4,
    &R6522::Read5,
    &R6522::Read6,
    &R6522::Read7,
    &R6522::Read8,
    &R6522::Read9,
    &R6522::ReadA,
    &R6522::ReadB,
    &R6522::ReadC,
    &R6522::ReadD,
    &R6522::ReadE,
    &R6522::ReadF,
};

static const BBCMicro::WriteMMIOFn g_WD1770_write_fns[] = {
    &WD1770::Write0,
    &WD1770::Write1,
    &WD1770::Write2,
    &WD1770::Write3,
};
static const BBCMicro::ReadMMIOFn g_WD1770_read_fns[] = {
    &WD1770::Read0,
    &WD1770::Read1,
    &WD1770::Read2,
    &WD1770::Read3,
};

static const uint8_t g_unmapped_reads[BBCMicro::BIG_PAGE_SIZE_BYTES] = {
    0,
};
static uint8_t g_unmapped_writes[BBCMicro::BIG_PAGE_SIZE_BYTES];

const uint16_t BBCMicro::SCREEN_WRAP_ADJUSTMENTS[] = {
    0x4000 >> 3,
    0x2000 >> 3,
    0x5000 >> 3,
    0x2800 >> 3};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

BBCMicro::State::State(const BBCMicroType *type,
                       const std::vector<uint8_t> &nvram_contents,
                       bool power_on_tone,
                       const tm *rtc_time,
                       uint64_t initial_num_2MHz_cycles)
    : num_2MHz_cycles(initial_num_2MHz_cycles) {
    M6502_Init(&this->cpu, type->m6502_config);
    this->ram_buffer.resize(type->ram_buffer_size);

    if (type->flags & BBCMicroTypeFlag_HasRTC) {
        this->rtc.SetRAMContents(nvram_contents);

        if (rtc_time) {
            this->rtc.SetTime(rtc_time);
        }
    }

    this->sn76489.Reset(power_on_tone);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

BBCMicro::BBCMicro(const BBCMicroType *type,
                   const DiscInterfaceDef *def,
                   const std::vector<uint8_t> &nvram_contents,
                   const tm *rtc_time,
                   bool video_nula,
                   bool ext_mem,
                   bool power_on_tone,
                   BeebLinkHandler *beeblink_handler,
                   uint64_t initial_num_2MHz_cycles)
    : m_state(type,
              nvram_contents,
              power_on_tone,
              rtc_time,
              initial_num_2MHz_cycles)
    , m_type(type)
    , m_disc_interface(def ? def->create_fun() : nullptr)
    , m_video_nula(video_nula)
    , m_ext_mem(ext_mem)
    , m_beeblink_handler(beeblink_handler) {
    this->InitStuff();
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

BBCMicro::BBCMicro(const BBCMicro &src)
    : m_state(src.m_state)
    , m_type(src.m_type)
    , m_disc_interface(src.m_disc_interface ? src.m_disc_interface->Clone() : nullptr)
    , m_video_nula(src.m_video_nula)
    , m_ext_mem(src.m_ext_mem) {
    ASSERT(src.GetCloneImpediments() == 0);

    for (int i = 0; i < NUM_DRIVES; ++i) {
        std::shared_ptr<DiscImage> disc_image = DiscImage::Clone(src.GetDiscImage(i));
        this->SetDiscImage(i, std::move(disc_image));
        m_is_drive_write_protected[i] = src.m_is_drive_write_protected[i];
    }

    this->InitStuff();
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

BBCMicro::~BBCMicro() {
#if BBCMICRO_TRACE
    this->StopTrace(nullptr);
#endif

    delete m_disc_interface;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

uint32_t BBCMicro::GetCloneImpediments() const {
    uint32_t result = 0;

    for (int i = 0; i < NUM_DRIVES; ++i) {
        if (!!m_disc_images[i]) {
            if (!m_disc_images[i]->CanClone()) {
                result |= (uint32_t)BBCMicroCloneImpediment_Drive0 << i;
            }
        }
    }

    if (m_beeblink_handler) {
        result |= BBCMicroCloneImpediment_BeebLink;
    }

    return result;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

std::unique_ptr<BBCMicro> BBCMicro::Clone() const {
    if (this->GetCloneImpediments() != 0) {
        return nullptr;
    }

    return std::unique_ptr<BBCMicro>(new BBCMicro(*this));
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if BBCMICRO_TRACE
void BBCMicro::SetTrace(std::shared_ptr<Trace> trace, uint32_t trace_flags) {
    m_trace_ptr = std::move(trace);
    m_trace = m_trace_ptr.get();
    m_trace_current_instruction = nullptr;
    m_trace_flags = trace_flags;

    if (m_trace) {
        m_trace->SetTime(&m_state.num_2MHz_cycles);
    }

    m_state.fdc.SetTrace(trace_flags & BBCMicroTraceFlag_1770 ? m_trace : nullptr);
    m_state.rtc.SetTrace(trace_flags & BBCMicroTraceFlag_RTC ? m_trace : nullptr);
    m_state.crtc.SetTrace((trace_flags & (BBCMicroTraceFlag_6845 | BBCMicroTraceFlag_6845Scanlines)) ? m_trace : nullptr,
                          !!(trace_flags & BBCMicroTraceFlag_6845Scanlines),
                          !!(trace_flags & BBCMicroTraceFlag_6845ScanlinesSeparators));
    m_state.system_via.SetTrace(trace_flags & BBCMicroTraceFlag_SystemVIA ? m_trace : nullptr,
                                !!(trace_flags & BBCMicroTraceFlag_SystemVIAExtra));
    m_state.user_via.SetTrace(trace_flags & BBCMicroTraceFlag_UserVIA ? m_trace : nullptr,
                              !!(trace_flags & BBCMicroTraceFlag_UserVIAExtra));
    m_state.video_ula.SetTrace(trace_flags & BBCMicroTraceFlag_VideoULA ? m_trace : nullptr);
    m_state.sn76489.SetTrace(trace_flags & BBCMicroTraceFlag_SN76489 ? m_trace : nullptr);

    if (!!m_beeblink) {
        m_beeblink->SetTrace(trace_flags & BBCMicroTraceFlag_BeebLink ? m_trace : nullptr);
    }

    this->UpdateCPUDataBusFn();
}
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BBCMicro::UpdatePaging() {
    MemoryBigPageTables tables;
    bool io;
    bool crt_shadow;
    (*m_type->get_mem_big_page_tables_fn)(&tables,
                                          &io,
                                          &crt_shadow,
                                          m_state.romsel,
                                          m_state.acccon);

    for (size_t i = 0; i < 2; ++i) {
        MemoryBigPages *mbp = &m_mem_big_pages[i];

        for (size_t j = 0; j < 16; ++j) {
            const BigPage *bp = &m_big_pages[tables.mem_big_pages[i][j]];

            mbp->w[j] = bp->w;
            mbp->r[j] = bp->r;
#if BBCMICRO_DEBUGGER
            mbp->debug[j] = bp->debug;
            mbp->bp[j] = bp;
#endif
        }
    }

    for (size_t i = 0; i < 16; ++i) {
        ASSERT(tables.pc_mem_big_pages_set[i] == 0 || tables.pc_mem_big_pages_set[i] == 1);
        m_pc_mem_big_pages[i] = &m_mem_big_pages[tables.pc_mem_big_pages_set[i]];
    }

    if (crt_shadow) {
        m_state.shadow_select_mask = 0x8000;
    } else {
        m_state.shadow_select_mask = 0;
    }

    if (io != m_rom_mmio) {
        if (io) {
            for (int i = 0; i < 3; ++i) {
                m_rmmio_fns[i] = m_hw_rmmio_fns[i].data();
                m_mmio_fn_contexts[i] = m_hw_mmio_fn_contexts[i].data();
                m_mmio_stretch[i] = m_hw_mmio_stretch[i].data();
            }
        } else {
            for (int i = 0; i < 3; ++i) {
                m_rmmio_fns[i] = m_rom_rmmio_fns.data();
                m_mmio_fn_contexts[i] = m_rom_mmio_fn_contexts.data();
                m_mmio_stretch[i] = m_rom_mmio_stretch.data();
            }
        }

        m_rom_mmio = io;
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BBCMicro::InitPaging() {
    for (BigPage &bp : m_big_pages) {
        bp = {};
    }

    for (size_t i = 0; i < 32; ++i) {
        size_t offset = i * BIG_PAGE_SIZE_BYTES;

        if (offset < m_state.ram_buffer.size()) {
            BigPage *bp = &m_big_pages[i];

            bp->r = bp->w = &m_state.ram_buffer[offset];
        }
    }

    for (size_t i = 0; i < 16; ++i) {
        for (size_t j = 0; j < NUM_ROM_BIG_PAGES; ++j) {
            BigPage *bp = &m_big_pages[ROM0_BIG_PAGE_INDEX + i * NUM_ROM_BIG_PAGES + j];

            if (!!m_state.sideways_rom_buffers[i]) {
                bp->r = m_state.sideways_rom_buffers[i]->data() + j * BIG_PAGE_SIZE_BYTES;
            } else if (!m_state.sideways_ram_buffers[i].empty()) {
                bp->r = bp->w = m_state.sideways_ram_buffers[i].data() + j * BIG_PAGE_SIZE_BYTES;
            } else {
                // not mapped...
            }
        }
    }

    for (size_t i = 0; i < NUM_MOS_BIG_PAGES; ++i) {
        if (!!m_state.os_buffer) {
            BigPage *bp = &m_big_pages[MOS_BIG_PAGE_INDEX + i];

            bp->r = m_state.os_buffer->data() + i * BIG_PAGE_SIZE_BYTES;
        }
    }

    // Fix up everything else.
    for (uint8_t i = 0; i < NUM_BIG_PAGES; ++i) {
        BigPage *bp = &m_big_pages[i];

        if (!bp->r) {
            bp->r = g_unmapped_reads;
        }

        if (!bp->w) {
            bp->w = g_unmapped_writes;
        }

        bp->metadata = &m_type->big_pages_metadata[i];
        bp->index = i;
    }

#if BBCMICRO_DEBUGGER
    this->UpdateDebugState();
#endif

    this->UpdatePaging();
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BBCMicro::Write1770ControlRegister(void *m_, M6502Word a, uint8_t value) {
    auto m = (BBCMicro *)m_;
    (void)a;

    ASSERT(m->m_disc_interface);
    m->m_state.disc_control = m->m_disc_interface->GetControlFromByte(value);

#if BBCMICRO_TRACE
    if (m->m_trace) {
        m->m_trace->AllocStringf("1770 - Control Register: Reset=%d; DDEN=%d; drive=%d, side=%d\n", m->m_state.disc_control.reset, m->m_state.disc_control.dden, m->m_state.disc_control.drive, m->m_state.disc_control.side);
    }
#endif

    LOGF(1770, "Write control register: 0x%02X: Reset=%d; DDEN=%d; drive=%d, side=%d\n", value, m->m_state.disc_control.reset, m->m_state.disc_control.dden, m->m_state.disc_control.drive, m->m_state.disc_control.side);

    if (m->m_state.disc_control.reset) {
        m->m_state.fdc.Reset();
    }

    m->m_state.fdc.SetDDEN(!!m->m_state.disc_control.dden);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

uint8_t BBCMicro::Read1770ControlRegister(void *m_, M6502Word a) {
    auto m = (BBCMicro *)m_;
    (void)a;

    ASSERT(m->m_disc_interface);

    uint8_t value = m->m_disc_interface->GetByteFromControl(m->m_state.disc_control);
    return value;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if BBCMICRO_TRACE
void BBCMicro::TracePortB(SystemVIAPB pb) {
    Log log("", m_trace->GetLogPrinter(1000));

    log.f("PORTB - PB = $%02X (%%%s): ", pb.value, BINARY_BYTE_STRINGS[pb.value]);

    bool has_rtc = !!(m_type->flags & BBCMicroTypeFlag_HasRTC);

    if (has_rtc) {
        log.f("RTC AS=%u; RTC CS=%u; ", pb.m128_bits.rtc_address_strobe, pb.m128_bits.rtc_chip_select);
    }

    const char *name = nullptr;
    bool value = pb.bits.latch_value;

    switch (pb.bits.latch_index) {
    case 0:
        name = "Sound Write";
        value = !value;
    print_bool:;
        log.f("%s=%s\n", name, BOOL_STR(value));
        break;

    case 1:
        name = has_rtc ? "RTC Read" : "Speech Read";
        goto print_bool;

    case 2:
        name = has_rtc ? "RTC DS" : "Speech Write";
        goto print_bool;

    case 3:
        name = "KB Read";
        goto print_bool;

    case 4:
    case 5:
        log.f("Screen Wrap Adjustment=$%04x\n", SCREEN_WRAP_ADJUSTMENTS[m_state.addressable_latch.bits.screen_base]);
        break;

    case 6:
        name = "Caps Lock LED";
        goto print_bool;

    case 7:
        name = "Shift Lock LED";
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

void BBCMicro::WriteUnmappedMMIO(void *m_, M6502Word a, uint8_t value) {
    (void)m_, (void)a, (void)value;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

uint8_t BBCMicro::ReadUnmappedMMIO(void *m_, M6502Word a) {
    (void)m_, (void)a;

    return 0;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

uint8_t BBCMicro::ReadROMMMIO(void *m_, M6502Word a) {
    auto m = (BBCMicro *)m_;

    return m->m_big_pages[MOS_BIG_PAGE_INDEX + 3].r[a.p.o];
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

uint8_t BBCMicro::ReadROMSEL(void *m_, M6502Word a) {
    auto m = (BBCMicro *)m_;
    (void)a;

    return m->m_state.romsel.value;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BBCMicro::WriteROMSEL(void *m_, M6502Word a, uint8_t value) {
    auto m = (BBCMicro *)m_;
    (void)a;

    if ((m->m_state.romsel.value ^ value) & m->m_romsel_mask) {
        m->m_state.romsel.value = value & m->m_romsel_mask;

        m->UpdatePaging();
        //(*m->m_update_romsel_pages_fn)(m);

#if BBCMICRO_TRACE
        if (m->m_trace) {
            m->m_trace->AllocWriteROMSELEvent(m->m_state.romsel);
        }
#endif
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

uint8_t BBCMicro::ReadACCCON(void *m_, M6502Word a) {
    auto m = (BBCMicro *)m_;
    (void)a;

    return m->m_state.acccon.value;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BBCMicro::WriteACCCON(void *m_, M6502Word a, uint8_t value) {
    auto m = (BBCMicro *)m_;
    (void)a;

    if ((m->m_state.acccon.value ^ value) & m->m_acccon_mask) {
        m->m_state.acccon.value = value & m->m_acccon_mask;
        m->UpdatePaging();

#if BBCMICRO_TRACE
        if (m->m_trace) {
            m->m_trace->AllocWriteACCCONEvent(m->m_state.acccon);
        }
#endif
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

const BBCMicroType *BBCMicro::GetType() const {
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
    ASSERT(key >= 0 && (int)key < 128);

    uint8_t *column = &m_state.key_columns[key & 0x0f];
    uint8_t mask = 1 << (key >> 4);

    return !!(*column & mask);
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

bool BBCMicro::SetKeyState(BeebKey key, bool new_state) {
    ASSERT(key >= 0 && (int)key < 128);

    uint8_t *column = &m_state.key_columns[key & 0x0f];
    uint8_t mask = 1 << (key >> 4);
    bool old_state = (*column & mask) != 0;

    if (key == BeebKey_Break) {
        if (new_state != m_state.resetting) {
            m_state.resetting = new_state;

            if (new_state) {
                M6502_Halt(&m_state.cpu);
            } else {
                M6502_Reset(&m_state.cpu);
                this->StopPaste();
            }

            return true;
        }
    } else {
        if (!old_state && new_state) {
            ASSERT(m_state.num_keys_down < 256);
            ++m_state.num_keys_down;
            *column |= mask;

            return true;
        } else if (old_state && !new_state) {
            ASSERT(m_state.num_keys_down > 0);
            --m_state.num_keys_down;
            *column &= ~mask;

            return true;
        }
    }

    return false;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool BBCMicro::HasNumericKeypad() const {
    return !!(m_type->flags & BBCMicroTypeFlag_HasNumericKeypad);
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
uint8_t BBCMicro::ReadAsyncCallThunk(void *m_, M6502Word a) {
    auto m = (BBCMicro *)m_;

    ASSERT(a.w >= ASYNC_CALL_THUNK_ADDR.w);
    size_t offset = (size_t)(a.w - ASYNC_CALL_THUNK_ADDR.w);
    ASSERT(offset < sizeof m->m_state.async_call_thunk_buf);

    //LOGF(OUTPUT,"%s: type=%u a=$%04x v=$%02x cycles=%" PRIu64 "\n",__func__,m->m_state.cpu.read,a.w,m->m_state.async_call_thunk_buf[offset],m->m_state.num_2MHz_cycles);

    return m->m_state.async_call_thunk_buf[offset];
}
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if BBCMICRO_DEBUGGER
void BBCMicro::HandleReadByteDebugFlags(uint8_t read, uint8_t flags) {
    if (flags & BBCMicroByteDebugFlag_BreakExecute) {
        if (read == M6502ReadType_Opcode) {
            this->DebugHalt("execute: $%04x", m_state.cpu.abus.w);
        }
    } else if (flags & BBCMicroByteDebugFlag_TempBreakExecute) {
        if (read == M6502ReadType_Opcode) {
            this->DebugHalt("single step");
        }
    }

    if (flags & BBCMicroByteDebugFlag_BreakRead) {
        if (read <= M6502ReadType_LastInterestingDataRead) {
            this->DebugHalt("data read: $%04x", m_state.cpu.abus.w);
        }
    }
}
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if BBCMICRO_DEBUGGER
void BBCMicro::HandleInterruptBreakpoints() {
    if (M6502_IsProbablyIRQ(&m_state.cpu)) {
        if ((m_state.system_via.ifr.value & m_state.system_via.ier.value & m_debug->hw.system_via_irq_breakpoints.value) ||
            (m_state.user_via.ifr.value & m_state.user_via.ier.value & m_debug->hw.user_via_irq_breakpoints.value)) {
            this->SetDebugStepType(BBCMicroStepType_StepIntoIRQHandler);
        }
    }
}
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BBCMicro::HandleCPUDataBusWithShadowRAM(BBCMicro *m) {
    uint8_t mmio_page = m->m_state.cpu.abus.b.h - 0xfc;
    if (const uint8_t read = m->m_state.cpu.read) {
        if (mmio_page < 3) {
            ReadMMIOFn fn = m->m_rmmio_fns[mmio_page][m->m_state.cpu.abus.b.l];
            void *context = m->m_mmio_fn_contexts[mmio_page][m->m_state.cpu.abus.b.l];
            m->m_state.cpu.dbus = (*fn)(context, m->m_state.cpu.abus);
        } else {
            m->m_state.cpu.dbus = m->m_pc_mem_big_pages[m->m_state.cpu.opcode_pc.p.p]->r[m->m_state.cpu.abus.p.p][m->m_state.cpu.abus.p.o];
        }
    } else {
        if (mmio_page < 3) {
            WriteMMIOFn fn = m->m_hw_wmmio_fns[mmio_page][m->m_state.cpu.abus.b.l];
            void *context = m->m_hw_mmio_fn_contexts[mmio_page][m->m_state.cpu.abus.b.l];
            (*fn)(context, m->m_state.cpu.abus, m->m_state.cpu.dbus);
        } else {
            m->m_pc_mem_big_pages[m->m_state.cpu.opcode_pc.p.p]->w[m->m_state.cpu.abus.p.p][m->m_state.cpu.abus.p.o] = m->m_state.cpu.dbus;
        }
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if BBCMICRO_DEBUGGER
void BBCMicro::HandleCPUDataBusWithShadowRAMDebug(BBCMicro *m) {
    uint8_t mmio_page = m->m_state.cpu.abus.b.h - 0xfc;
    if (const uint8_t read = m->m_state.cpu.read) {
        if (mmio_page < 3) {
            ReadMMIOFn fn = m->m_rmmio_fns[mmio_page][m->m_state.cpu.abus.b.l];
            void *context = m->m_mmio_fn_contexts[mmio_page][m->m_state.cpu.abus.b.l];
            m->m_state.cpu.dbus = (*fn)(context, m->m_state.cpu.abus);
        } else {
            m->m_state.cpu.dbus = m->m_pc_mem_big_pages[m->m_state.cpu.opcode_pc.p.p]->r[m->m_state.cpu.abus.p.p][m->m_state.cpu.abus.p.o];
        }

        uint8_t flags = (m->m_debug->address_debug_flags[m->m_state.cpu.abus.w] |
                         m->m_pc_mem_big_pages[m->m_state.cpu.opcode_pc.p.p]->debug[m->m_state.cpu.abus.p.p][m->m_state.cpu.abus.p.o]);
        if (flags != 0) {
            m->HandleReadByteDebugFlags(read, flags);
        }

        if (read == M6502ReadType_Interrupt) {
            m->HandleInterruptBreakpoints();
        }
    } else {
        if (mmio_page < 3) {
            WriteMMIOFn fn = m->m_hw_wmmio_fns[mmio_page][m->m_state.cpu.abus.b.l];
            void *context = m->m_hw_mmio_fn_contexts[mmio_page][m->m_state.cpu.abus.b.l];
            (*fn)(context, m->m_state.cpu.abus, m->m_state.cpu.dbus);
        } else {
            m->m_pc_mem_big_pages[m->m_state.cpu.opcode_pc.p.p]->w[m->m_state.cpu.abus.p.p][m->m_state.cpu.abus.p.o] = m->m_state.cpu.dbus;
        }

        uint8_t flags = (m->m_debug->address_debug_flags[m->m_state.cpu.abus.w] |
                         m->m_pc_mem_big_pages[m->m_state.cpu.opcode_pc.p.p]->debug[m->m_state.cpu.abus.p.p][m->m_state.cpu.abus.p.o]);

        if (flags & BBCMicroByteDebugFlag_BreakWrite) {
            m->DebugHalt("data write: $%04x", m->m_state.cpu.abus.w);
        }
    }
}
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BBCMicro::HandleCPUDataBusWithHacks(BBCMicro *m) {
#if BBCMICRO_DEBUGGER
    if (m->m_state.async_call_address.w != INVALID_ASYNC_CALL_ADDRESS) {
        if (m->m_state.cpu.read == M6502ReadType_Interrupt && M6502_IsProbablyIRQ(&m->m_state.cpu)) {
            TRACEF(m->m_trace, "Enqueuing async call: address=$%04x, A=%03u ($%02x) X=%03u ($%02x) Y=%03u ($%02X) C=%s\n",
                   m->m_state.async_call_address.w,
                   m->m_state.async_call_a, m->m_state.async_call_a,
                   m->m_state.async_call_x, m->m_state.async_call_x,
                   m->m_state.async_call_y, m->m_state.async_call_y,
                   BOOL_STR(m->m_state.async_call_c));

            // Already on the stack is the actual return that the
            // thunk will RTI to.

            // Manually push current PC and status register.
            {
                M6502P p = M6502_GetP(&m->m_state.cpu);

                const BigPage *bp = m->DebugGetBigPageForAddress(m->m_state.cpu.s, {}, 0);

                // Add the thunk call address that the IRQ routine will RTI to.
                bp->w[m->m_state.cpu.s.w & BIG_PAGE_OFFSET_MASK] = m->m_state.cpu.pc.b.h;
                --m->m_state.cpu.s.b.l;

                bp->w[m->m_state.cpu.s.w & BIG_PAGE_OFFSET_MASK] = m->m_state.cpu.pc.b.l;
                --m->m_state.cpu.s.b.l;

                bp->w[m->m_state.cpu.s.w & BIG_PAGE_OFFSET_MASK] = p.value;
                --m->m_state.cpu.s.b.l;

                // Set up CPU as if it's about to execute the thunk, so
                // the IRQ routine will return to the desired place.
                p.bits.c = m->m_state.async_call_c;
                M6502_SetP(&m->m_state.cpu, p.value);
                m->m_state.cpu.pc = ASYNC_CALL_THUNK_ADDR;
            }

            // Set up thunk.
            {
                uint8_t *p = m->m_state.async_call_thunk_buf;

                *p++ = 0x48; //pha
                *p++ = 0x8a; //txa
                *p++ = 0x48; //pha
                *p++ = 0x98; //tya
                *p++ = 0x48; //pha
                *p++ = 0xa9;
                *p++ = m->m_state.async_call_a;
                *p++ = 0xa2;
                *p++ = m->m_state.async_call_x;
                *p++ = 0xa0;
                *p++ = m->m_state.async_call_y;
                *p++ = m->m_state.async_call_c ? 0x38 : 0x18; //sec:clc
                *p++ = 0x20;                                  //jsr abs
                *p++ = m->m_state.async_call_address.b.l;
                *p++ = m->m_state.async_call_address.b.h;
                *p++ = 0x68; //pla
                *p++ = 0xa8; //tay
                *p++ = 0x68; //pla
                *p++ = 0xaa; //tax
                *p++ = 0x68; //pla
                *p++ = 0x40; //rti

                ASSERT((size_t)(p - m->m_state.async_call_thunk_buf) <= sizeof m->m_state.async_call_thunk_buf);
            }

            m->FinishAsyncCall(true);
        } else {
            --m->m_state.async_call_timeout;
            if (m->m_state.async_call_timeout < 0) {
                m->FinishAsyncCall(false);
            }
        }
    }
#endif

    if (m->m_state.cpu.read == 0) {
        if (!m->m_write_fns.empty()) {
            // Same deal as instruction fns.
            auto *fn = m->m_write_fns.data();
            auto *fns_end = fn + m->m_write_fns.size();
            bool any_removed = false;

            while (fn != fns_end) {
                if ((*fn->first)(m, &m->m_state.cpu, fn->second)) {
                    ++fn;
                } else {
                    any_removed = true;
                    *fn = *--fns_end;
                }
            }

            if (any_removed) {
                m->m_write_fns.resize((size_t)(fns_end - m->m_write_fns.data()));

                m->UpdateCPUDataBusFn();
            }
        }
    }

    (*m->m_default_handle_cpu_data_bus_fn)(m);

    if (M6502_IsAboutToExecute(&m->m_state.cpu)) {
        if (!m->m_instruction_fns.empty()) {

            // This is a bit bizarre, but I just can't stomach the
            // idea of literally like 1,000,000 std::vector calls per
            // second. But this way, it's hopefully more like only
            // 300,000.

            auto *fn = m->m_instruction_fns.data();
            auto *fns_end = fn + m->m_instruction_fns.size();
            bool removed = false;

            while (fn != fns_end) {
                if ((*fn->first)(m, &m->m_state.cpu, fn->second)) {
                    ++fn;
                } else {
                    removed = true;
                    *fn = *--fns_end;
                }
            }

            if (removed) {
                m->m_instruction_fns.resize((size_t)(fns_end - m->m_instruction_fns.data()));

                m->UpdateCPUDataBusFn();
            }
        }

        if (m->m_state.hack_flags & BBCMicroHackFlag_Paste) {
            ASSERT(m->m_state.paste_state != BBCMicroPasteState_None);

            if (m->m_state.cpu.pc.w == 0xffe1) {
                // OSRDCH

                // Put next byte in A.
                switch (m->m_state.paste_state) {
                case BBCMicroPasteState_None:
                    ASSERT(false);
                    break;

                case BBCMicroPasteState_Wait:
                    m->SetKeyState(PASTE_START_KEY, false);
                    m->m_state.paste_state = BBCMicroPasteState_Delete;
                    // fall through
                case BBCMicroPasteState_Delete:
                    m->m_state.cpu.a = 127;
                    m->m_state.paste_state = BBCMicroPasteState_Paste;
                    break;

                case BBCMicroPasteState_Paste:
                    ASSERT(m->m_state.paste_index < m->m_state.paste_text->size());
                    m->m_state.cpu.a = (uint8_t)m->m_state.paste_text->at(m->m_state.paste_index);

                    ++m->m_state.paste_index;
                    if (m->m_state.paste_index == m->m_state.paste_text->size()) {
                        m->StopPaste();
                    }
                    break;
                }

                // No Escape.
                m->m_state.cpu.p.bits.c = 0;

                // Pretend the instruction was RTS.
                m->m_state.cpu.dbus = 0x60;
            }
        }

#if BBCMICRO_TRACE
        if (m->m_trace) {
            InstructionTraceEvent *e;

            // Fill out results of last instruction.
            if ((e = m->m_trace_current_instruction) != NULL) {
                e->a = m->m_state.cpu.a;
                e->x = m->m_state.cpu.x;
                e->y = m->m_state.cpu.y;
                e->p = m->m_state.cpu.p.value;
                e->data = m->m_state.cpu.data;
                e->opcode = m->m_state.cpu.opcode;
                e->s = m->m_state.cpu.s.b.l;
                //e->pc=m_state.cpu.pc.w;//...for next instruction
                e->ad = m->m_state.cpu.ad.w;
                e->ia = m->m_state.cpu.ia.w;
            }

            // Allocate event for next instruction.
            e = m->m_trace_current_instruction = (InstructionTraceEvent *)m->m_trace->AllocEvent(INSTRUCTION_EVENT);

            if (e) {
                e->pc = m->m_state.cpu.abus.w;

                /* doesn't matter if the last instruction ends up
                * bogus... there are no invalid values.
                */
            }
        }
#endif
    }

#if BBCMICRO_DEBUGGER
    if (m->m_debug) {
        if (m->m_state.cpu.read >= M6502ReadType_Opcode) {
            switch (m->m_debug->step_type) {
            default:
                ASSERT(false);
                // fall through
            case BBCMicroStepType_None:
                break;

            case BBCMicroStepType_StepIn:
                {
                    if (m->m_state.cpu.read == M6502ReadType_Opcode) {
                        // Done.
                        m->DebugHalt("single step");
                    } else {
                        ASSERT(m->m_state.cpu.read == M6502ReadType_Interrupt);
                        // The instruction was interrupted, so set a temp
                        // breakpoint in the right place.
                        uint8_t flags = m->DebugGetAddressDebugFlags(m->m_state.cpu.pc);

                        flags |= BBCMicroByteDebugFlag_TempBreakExecute;

                        m->DebugSetAddressDebugFlags(m->m_state.cpu.pc, flags);
                    }

                    m->SetDebugStepType(BBCMicroStepType_None);
                }
                break;

            case BBCMicroStepType_StepIntoIRQHandler:
                {
                    ASSERT(m->m_state.cpu.read == M6502ReadType_Opcode || m->m_state.cpu.read == M6502ReadType_Interrupt);
                    if (m->m_state.cpu.read == M6502ReadType_Opcode) {
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

void BBCMicro::CheckMemoryBigPages(const MemoryBigPages *mem_big_pages, bool non_null) {
    (void)non_null;

    if (mem_big_pages) {
        for (size_t i = 0; i < 16; ++i) {
            ASSERT(!!mem_big_pages->r[i] == non_null);
            ASSERT(!!mem_big_pages->w[i] == non_null);
#if BBCMICRO_DEBUGGER
            ASSERT(!!mem_big_pages->bp[i] == non_null);
            if (mem_big_pages->bp[i]) {
                ASSERT(mem_big_pages->debug[i] == mem_big_pages->bp[i]->debug);
            } else {
                ASSERT(!mem_big_pages->debug[i]);
            }
#endif
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
static const uint8_t CURSOR_PATTERNS[8] = {
    0 + 0 + 0 + 0,
    0 + 0 + 4 + 8,
    0 + 2 + 0 + 0,
    0 + 2 + 4 + 8,
    1 + 0 + 0 + 0,
    1 + 0 + 4 + 8,
    1 + 2 + 0 + 0,
    1 + 2 + 4 + 8,
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if BBCMICRO_DEBUGGER
uint16_t BBCMicro::DebugGetBeebAddressFromCRTCAddress(uint8_t h, uint8_t l) const {
    M6502Word addr;
    addr.b.h = h;
    addr.b.l = l;

    if (addr.w & 0x2000) {
        return (addr.w & 0x3ff) | m_teletext_bases[addr.w >> 11 & 1];
    } else {
        if (addr.w & 0x1000) {
            addr.w -= SCREEN_WRAP_ADJUSTMENTS[m_state.addressable_latch.bits.screen_base];
            addr.w &= ~0x1000;
        }

        return addr.w << 3;
    }
}
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// Timing diagram:
//
// see https://stardot.org.uk/forums/viewtopic.php?f=3&t=17896#p248051
//
// <pre>    |                                                   |
// 1MHz     |          ___.___         ___.___         ___.___  |
// phi2     | \___.___/       \___.___/       \___.___/       \ |
// 2MHz     |      ___     ___     ___     ___     ___     ___  |
// phi2     | \___/   \___/   \___/   \___/   \___/   \___/   \ |
// BBCMicro |    |<----->|<----->|<----->|<----->|<----->|<---- |
// Update   |    |   0   |   1   |   2   |   3   |   4   |      |
//          |                                                   |
// 1MHz     |          ___.___         ___.___         ___.___  |
// phi2     | \___.___/       \___.___/       \___.___/       \ |
// stretch  |      ___     ___     ___.___.___     ___     ___  |
// case 1   | \___/   \___/   \___/           \___/   \___/   \ |
//          |                         |< +1 ->|                 |
// BBCMicro |    |<----->|<----->|<----->|<----->|<----->|<---- |
// Update   |    |   0   |   1   |   2   |   3   |   4   |      |
//          |                                                   |
// 1MHz     |          ___.___         ___.___         ___.___  |
// phi2     | \___.___/       \___.___/       \___.___/       \ |
// stretch  |      ___             ___.___.___     ___     ___  |
// case 2   | \___/   \___.___.___/           \___/   \___/   \ |
//          |             |< +2 --------->|                     |
// BBCMicro |    |<----->|<----->|<----->|<----->|<----->|<---- |
// Update   |    |   0   |   1   |   2   |   3   |   4   |      |
// </pre>
//
// So each BBCMicro::Update encompasses a 2 MHz phi2 leading edge then a 2 MHz
// phi2 trailing edge, all covered by the 6502 update.
//
// It also involves a 1 MHz phi2 leading edge or a 1 MHz phi2 trailing edge.
//
// Memory accesses occur at the 2 MHz phi2 trailing edge. To access a 1 MHz
// device, the next phi2 trailing edge has to line up with the next 1 MHz
// phi2 trailing edge.
//
// When the 2 MHz phi2 leading edge coincides with 1 MHz phi2=1, this is case 2.
// Delay the leading edge for 1 x 2 MHz cycle.
//
// When the 2 MHz phi2 leading edge coincides with 1 MHz phi2=0 (either due to
// the clocks' alignment, or because this was originally case 2), this is case
// 1. Delay the trailing edge for 1 x 2 MHz cycle. The trailing edges of both
// clocks then line up.
//
bool BBCMicro::Update(VideoDataUnit *video_unit, SoundDataUnit *sound_unit) {
    uint8_t phi2_1MHz_trailing_edge = m_state.num_2MHz_cycles & 1;
    bool sound = false;

#if VIDEO_TRACK_METADATA
    video_unit->metadata.flags = 0;

    if (phi2_1MHz_trailing_edge) {
        video_unit->metadata.flags |= VideoDataUnitMetadataFlag_OddCycle;
    }
#endif

    // Update CPU.
    if (m_state.stretch) {
        if (phi2_1MHz_trailing_edge) {
            m_state.stretch = false;
        }
    } else {
        (*m_state.cpu.tfn)(&m_state.cpu);

        uint8_t mmio_page = m_state.cpu.abus.b.h - 0xfc;
        if (mmio_page < 3) {
            if (m_state.cpu.read) {
                m_state.stretch = m_mmio_stretch[mmio_page][m_state.cpu.abus.b.l];
            } else {
                m_state.stretch = m_hw_mmio_stretch[mmio_page][m_state.cpu.abus.b.l];
            }
        }
    }

    if (!m_state.stretch) {
        (*m_handle_cpu_data_bus_fn)(this);
    }

    // Update video hardware.
    if (m_state.video_ula.control.bits.fast_6845 | phi2_1MHz_trailing_edge) {
        const CRTC::Output output = m_state.crtc.Update(m_state.system_via.b.c2);

        m_state.cursor_pattern >>= 1;

        uint16_t addr = (uint16_t)output.address;

        if (addr & 0x2000) {
            addr = (addr & 0x3ff) | m_teletext_bases[addr >> 11 & 1];
        } else {
            if (addr & 0x1000) {
                addr -= SCREEN_WRAP_ADJUSTMENTS[m_state.addressable_latch.bits.screen_base];
                addr &= ~0x1000;
            }

            addr <<= 3;

            // When output.raster>=8, this address is bogus. There's a
            // check later.
            addr |= output.raster & 7;
        }

        ASSERTF(addr < 32768, "output: hsync=%u vsync=%u display=%u address=0x%x raster=%u; addr=0x%x; latch screen_base=%u\n",
                output.hsync, output.vsync, output.display, output.address, output.raster,
                addr,
                m_state.addressable_latch.bits.screen_base);
        addr |= m_state.shadow_select_mask;

        // Teletext update.
        if (phi2_1MHz_trailing_edge) {
            if (output.vsync) {
                if (!m_state.crtc_last_output.vsync) {
                    m_state.last_frame_2MHz_cycles = m_state.num_2MHz_cycles - m_state.last_vsync_2MHz_cycles;
                    m_state.last_vsync_2MHz_cycles = m_state.num_2MHz_cycles;

                    m_state.saa5050.VSync();
                }
            }

            if (m_state.video_ula.control.bits.teletext) {
                // Teletext line boundary stuff.
                //
                // The hsync output is linked up to the SAA505's GLR
                // ("General line reset") pin, which sounds like it should
                // do line stuff. The data sheet is a bit vague, though:
                // "required for internal synchronization of remote control
                // data signals"...??
                //
                // https://github.com/mist-devel/mist-board/blob/f6cc6ff597c22bdd8b002c04c331619a9767eae0/cores/bbc/rtl/saa5050/saa5050.v
                // seems to ignore it completely, and does everything based
                // on the LOSE pin, connected to 6845 DISPEN/DISPTMSG. So
                // that's what this does...
                //
                // (Evidence in favour of this: normally, R5 doesn't affect
                // the teletext chars, even though it must vary the number
                // of hsyncs between vsync and the first visible scanline.
                // But after setting R6=255, changing R5 does have an
                // affect, suggesting that DISPTMSG transitions are being
                // counted and hsyncs aren't.)
                if (output.display) {
                    if (!m_state.crtc_last_output.display) {
                        m_state.saa5050.StartOfLine();
                    }
                } else {
                    m_state.ic15_byte |= 0x40;

                    if (m_state.crtc_last_output.display) {
                        m_state.saa5050.EndOfLine();
                    }
                }
            }

            m_state.saa5050.Byte(m_state.ic15_byte, output.display);

            if (output.address & 0x2000) {
                m_state.ic15_byte = m_ram[addr];
            } else {
                m_state.ic15_byte = 0;
            }

#if VIDEO_TRACK_METADATA
            video_unit->metadata.flags |= VideoDataUnitMetadataFlag_HasValue;
            video_unit->metadata.value = m_state.ic15_byte;
#endif
        }

        if (!m_state.video_ula.control.bits.teletext) {
            if (!m_state.crtc_last_output.display) {
                m_state.video_ula.DisplayEnabled();
            }

            uint8_t value = m_ram[addr];
            m_state.video_ula.Byte(value);

#if VIDEO_TRACK_METADATA
            video_unit->metadata.flags |= VideoDataUnitMetadataFlag_HasValue;
            video_unit->metadata.value = value;
#endif
        }

        if (output.cudisp) {
            m_state.cursor_pattern = CURSOR_PATTERNS[m_state.video_ula.control.bits.cursor];
        }

#if VIDEO_TRACK_METADATA
        // TODO - can't remember why this is stored off like this...
        m_last_video_access_address = addr;

        video_unit->metadata.flags |= VideoDataUnitMetadataFlag_HasAddress;
        video_unit->metadata.address = m_last_video_access_address;
        video_unit->metadata.crtc_address = output.address;
#endif

        m_state.crtc_last_output = output;
    }

    // Update display output.
    //if(m_state.crtc_last_output.display) {
#if VIDEO_TRACK_METADATA
    if (m_state.crtc_last_output.raster == 0) {
        video_unit->metadata.flags |= VideoDataUnitMetadataFlag_6845Raster0;
    }

    if (m_state.crtc_last_output.display) {
        video_unit->metadata.flags |= VideoDataUnitMetadataFlag_6845DISPEN;
    }

    if (m_state.crtc_last_output.cudisp) {
        video_unit->metadata.flags |= VideoDataUnitMetadataFlag_6845CUDISP;
    }
#endif

    if (m_state.video_ula.control.bits.teletext) {
        m_state.saa5050.EmitPixels(&video_unit->pixels, m_state.video_ula.output_palette);

        if (m_state.cursor_pattern & 1) {
            video_unit->pixels.pixels[0].all ^= 0x0fff;
            video_unit->pixels.pixels[1].all ^= 0x0fff;
        }
    } else {
        if (m_state.crtc_last_output.display &&
            m_state.crtc_last_output.raster < 8) {
            m_state.video_ula.EmitPixels(&video_unit->pixels);

            if (m_state.cursor_pattern & 1) {
                video_unit->pixels.values[0] ^= 0x0fff0fff0fff0fffull;
                video_unit->pixels.values[1] ^= 0x0fff0fff0fff0fffull;
            }
        } else {
            if (m_state.cursor_pattern & 1) {
                video_unit->pixels.values[1] = video_unit->pixels.values[0] = 0x0fff0fff0fff0fffull;
            } else {
                video_unit->pixels.values[1] = video_unit->pixels.values[0] = 0;
            }
        }
    }

    video_unit->pixels.pixels[1].bits.x = 0;

    if (m_state.crtc_last_output.hsync) {
        video_unit->pixels.pixels[1].bits.x |= VideoDataUnitFlag_HSync;
    }

    if (m_state.crtc_last_output.vsync) {
        video_unit->pixels.pixels[1].bits.x |= VideoDataUnitFlag_VSync;
    }

    // Update VIAs and slow data bus.
    if (phi2_1MHz_trailing_edge) {
        // Update vsync.
        if (!m_state.crtc_last_output.vsync) {
            m_state.system_via.a.c1 = 0;
        }

        // Update IRQs.
        m_state.system_via_irq_pending |= m_state.system_via.UpdatePhi2TrailingEdge();
        m_state.user_via_irq_pending |= m_state.user_via.UpdatePhi2TrailingEdge();

        if (m_state.system_via_irq_pending) {
            M6502_SetDeviceIRQ(&m_state.cpu, BBCMicroIRQDevice_SystemVIA, 1);
        } else {
            M6502_SetDeviceIRQ(&m_state.cpu, BBCMicroIRQDevice_SystemVIA, 0);
        }

        if (m_state.user_via_irq_pending) {
            M6502_SetDeviceIRQ(&m_state.cpu, BBCMicroIRQDevice_UserVIA, 1);
        } else {
            M6502_SetDeviceIRQ(&m_state.cpu, BBCMicroIRQDevice_UserVIA, 0);
        }

        // Update keyboard.
        if (m_state.addressable_latch.bits.not_kb_write) {
            if (!(m_state.key_columns[m_state.key_scan_column] & 0xfe)) {
                m_state.system_via.a.c2 = 0;
            }

            ++m_state.key_scan_column;
            m_state.key_scan_column &= 0x0f;
        } else {
            // manual scan
            BeebKey key = (BeebKey)(m_state.system_via.a.p & 0x7f);
            uint8_t kcol = key & 0x0f;
            uint8_t krow = (uint8_t)(key >> 4);

            uint8_t *column = &m_state.key_columns[kcol];

            // row 0 doesn't cause an interrupt
            if (!(*column & 0xfe)) {
                m_state.system_via.a.c2 = 0;
            }

            if (!(*column & 1 << krow)) {
                m_state.system_via.a.p &= 0x7f;
            }

            //if(key==m_state.auto_reset_key) {
            //    //*column&=~(1<<krow);
            //    m_state.auto_reset_key=BeebKey_None;
            //}
        }

        if (m_beeblink_handler) {
            // Update BeebLink.
            m_beeblink->Update(&m_state.user_via);
        } else {
            // Nothing connected to the user port.
        }

        // Update addressable latch and RTC.
        SystemVIAPB pb;
        pb.value = m_state.system_via.b.p;

        if (m_state.old_system_via_pb.value != pb.value) {
            uint8_t mask = 1 << pb.bits.latch_index;

            m_state.addressable_latch.value &= ~mask;
            if (pb.bits.latch_value) {
                m_state.addressable_latch.value |= mask;
            }

#if BBCMICRO_TRACE
            if (m_trace) {
                if (m_trace_flags & BBCMicroTraceFlag_SystemVIA) {
                    TracePortB(pb);
                }
            }
#endif

            if (m_has_rtc &&
                pb.m128_bits.rtc_chip_select &&
                m_state.old_system_via_pb.m128_bits.rtc_address_strobe &&
                !pb.m128_bits.rtc_address_strobe) {
                // Latch address on AS 1->0 transition.
                m_state.rtc.SetAddress(m_state.system_via.a.p);
            }

            m_state.old_system_via_pb = pb;
        }

        if (m_has_rtc) {
            if (pb.m128_bits.rtc_chip_select &&
                !pb.m128_bits.rtc_address_strobe) {
                // AS=0
                if (m_state.addressable_latch.m128_bits.rtc_read) {
                    // RTC read mode
                    m_state.system_via.a.p &= m_state.rtc.Read();
                } else {
                    // RTC write mode
                    if (m_state.old_addressable_latch.m128_bits.rtc_data_strobe &&
                        !m_state.addressable_latch.m128_bits.rtc_data_strobe) {
                        // DS=1 -> DS=0
                        m_state.rtc.SetData(m_state.system_via.a.p);
                    }
                }
            }

            m_state.rtc.Update();
        }

        m_state.old_addressable_latch = m_state.addressable_latch;
    } else {
        m_state.system_via_irq_pending = m_state.system_via.UpdatePhi2LeadingEdge();
        m_state.user_via_irq_pending = m_state.user_via.UpdatePhi2LeadingEdge();
    }

    // Update 1770.
    if (phi2_1MHz_trailing_edge) {
        M6502_SetDeviceNMI(&m_state.cpu, BBCMicroNMIDevice_1770, m_state.fdc.Update().value);
    }

    // Update sound.
    if ((m_state.num_2MHz_cycles & ((1 << SOUND_CLOCK_SHIFT) - 1)) == 0) {
        sound_unit->sn_output = m_state.sn76489.Update(!m_state.addressable_latch.bits.not_sound_write,
                                                       m_state.system_via.a.p);

#if BBCMICRO_ENABLE_DISC_DRIVE_SOUND
        // The disc drive sounds are pretty quiet.
        sound_unit->disc_drive_sound = this->UpdateDiscDriveSound(&m_state.drives[0]);
        sound_unit->disc_drive_sound += this->UpdateDiscDriveSound(&m_state.drives[1]);
#endif
        sound = true;
    }

    ++m_state.num_2MHz_cycles;

    return sound;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if BBCMICRO_ENABLE_DISC_DRIVE_SOUND
void BBCMicro::SetDiscDriveSound(DiscDriveType type, DiscDriveSound sound, std::vector<float> samples) {
    ASSERT(sound >= 0 && sound < DiscDriveSound_EndValue);
    ASSERT(g_disc_drive_sounds[type][sound].empty());
    ASSERT(samples.size() <= INT_MAX);
    g_disc_drive_sounds[type][sound] = std::move(samples);
}

//void BBCMicro::SetDiscDriveSound(int drive,DiscDriveSound sound,const float *samples,size_t num_samples) {
//    ASSERT(drive>=0&&drive<NUM_DRIVES);
//    DiscDrive_SetSoundData(&m_state.drives[drive],sound,samples,num_samples);
//}
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

uint32_t BBCMicro::GetLEDs() {
    uint32_t leds = 0;

    if (!(m_state.addressable_latch.bits.caps_lock_led)) {
        leds |= BBCMicroLEDFlag_CapsLock;
    }

    if (!(m_state.addressable_latch.bits.shift_lock_led)) {
        leds |= BBCMicroLEDFlag_ShiftLock;
    }

    for (int i = 0; i < NUM_DRIVES; ++i) {
        if (m_state.drives[i].motor) {
            leds |= (uint32_t)BBCMicroLEDFlag_Drive0 << i;
        }
    }

    return leds;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

std::vector<uint8_t> BBCMicro::GetNVRAM() const {
    if (m_has_rtc) {
        return m_state.rtc.GetRAMContents();
    } else {
        return std::vector<uint8_t>();
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BBCMicro::SetOSROM(std::shared_ptr<const ROMData> data) {
    m_state.os_buffer = std::move(data);

    this->InitPaging();
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BBCMicro::SetSidewaysROM(uint8_t bank, std::shared_ptr<const ROMData> data) {
    ASSERT(bank < 16);

    m_state.sideways_ram_buffers[bank].clear();

    m_state.sideways_rom_buffers[bank] = std::move(data);

    this->InitPaging();
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BBCMicro::SetSidewaysRAM(uint8_t bank, std::shared_ptr<const ROMData> data) {
    ASSERT(bank < 16);

    if (data) {
        m_state.sideways_ram_buffers[bank] = std::vector<uint8_t>(data->begin(), data->end());
    } else {
        m_state.sideways_ram_buffers[bank] = std::vector<uint8_t>(16384);
    }

    m_state.sideways_rom_buffers[bank] = nullptr;

    this->InitPaging();
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if BBCMICRO_TRACE
void BBCMicro::StartTrace(uint32_t trace_flags, size_t max_num_bytes) {
    this->StopTrace(nullptr);

    this->SetTrace(std::make_shared<Trace>(max_num_bytes,
                                           m_type,
                                           m_state.romsel,
                                           m_state.acccon),
                   trace_flags);
}
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if BBCMICRO_TRACE
void BBCMicro::StopTrace(std::shared_ptr<Trace> *old_trace_ptr) {
    if (old_trace_ptr) {
        *old_trace_ptr = m_trace_ptr;
    }

    if (m_trace) {
        if (m_trace_current_instruction) {
            m_trace->CancelEvent(INSTRUCTION_EVENT, m_trace_current_instruction);
            m_trace_current_instruction = NULL;
        }

        this->SetTrace(nullptr, 0);
    }
}
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if BBCMICRO_TRACE
int BBCMicro::GetTraceStats(struct TraceStats *stats) {
    if (!m_trace) {
        return 0;
    }

    m_trace->GetStats(stats);
    return 1;
}
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BBCMicro::AddInstructionFn(InstructionFn fn, void *context) {
    ASSERT(std::find(m_instruction_fns.begin(), m_instruction_fns.end(), std::make_pair(fn, context)) == m_instruction_fns.end());

    m_instruction_fns.emplace_back(fn, context);

    this->UpdateCPUDataBusFn();
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BBCMicro::RemoveInstructionFn(InstructionFn fn, void *context) {
    auto &&it = std::find(m_instruction_fns.begin(), m_instruction_fns.end(), std::make_pair(fn, context));

    if (it != m_instruction_fns.end()) {
        m_instruction_fns.erase(it);

        this->UpdateCPUDataBusFn();
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BBCMicro::AddWriteFn(WriteFn fn, void *context) {
    ASSERT(std::find(m_write_fns.begin(), m_write_fns.end(), std::make_pair(fn, context)) == m_write_fns.end());

    m_write_fns.emplace_back(fn, context);

    this->UpdateCPUDataBusFn();
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BBCMicro::SetMMIOFns(uint16_t addr, ReadMMIOFn read_fn, WriteMMIOFn write_fn, void *context) {
    ASSERT(addr >= 0xfc00 && addr <= 0xfeff);

    M6502Word tmp;
    tmp.w = addr;
    tmp.b.h -= 0xfc;

    m_hw_rmmio_fns[tmp.b.h][tmp.b.l] = read_fn ? read_fn : &ReadUnmappedMMIO;
    m_hw_wmmio_fns[tmp.b.h][tmp.b.l] = write_fn ? write_fn : &WriteUnmappedMMIO;
    m_hw_mmio_fn_contexts[tmp.b.h][tmp.b.l] = context;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

std::shared_ptr<DiscImage> BBCMicro::TakeDiscImage(int drive) {
    if (drive >= 0 && drive < NUM_DRIVES) {
        std::shared_ptr<DiscImage> tmp = std::move(m_disc_images[drive]);
        return tmp;
    } else {
        return nullptr;
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

std::shared_ptr<const DiscImage> BBCMicro::GetDiscImage(int drive) const {
    if (drive >= 0 && drive < NUM_DRIVES) {
        return m_disc_images[drive];
    } else {
        return nullptr;
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BBCMicro::SetDiscImage(int drive,
                            std::shared_ptr<DiscImage> disc_image) {
    if (drive < 0 || drive >= NUM_DRIVES) {
        return;
    }

    m_disc_images[drive] = std::move(disc_image);
    m_is_drive_write_protected[drive] = false;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BBCMicro::SetDriveWriteProtected(int drive,
                                      bool is_write_protected) {
    ASSERT(drive >= 0 && drive < NUM_DRIVES);

    m_is_drive_write_protected[drive] = is_write_protected;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool BBCMicro::IsDriveWriteProtected(int drive) const {
    ASSERT(drive >= 0 && drive < NUM_DRIVES);

    return m_is_drive_write_protected[drive];
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool BBCMicro::GetAndResetDiscAccessFlag() {
    bool result = m_disc_access;

    m_disc_access = false;

    return result;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool BBCMicro::IsPasting() const {
    return (m_state.hack_flags & BBCMicroHackFlag_Paste) != 0;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BBCMicro::StartPaste(std::shared_ptr<const std::string> text) {
    this->StopPaste();

    m_state.hack_flags |= BBCMicroHackFlag_Paste;
    m_state.paste_state = BBCMicroPasteState_Wait;
    m_state.paste_text = std::move(text);
    m_state.paste_index = 0;
    m_state.paste_wait_end = m_state.num_2MHz_cycles + 2000000;

    this->SetKeyState(PASTE_START_KEY, true);

    this->UpdateCPUDataBusFn();
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BBCMicro::StopPaste() {
    m_state.paste_state = BBCMicroPasteState_None;
    m_state.paste_index = 0;
    m_state.paste_text.reset();

    m_state.hack_flags &= (uint32_t)~BBCMicroHackFlag_Paste;
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
    if (m_ext_mem) {
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
BBCMicro::AddressableLatch BBCMicro::DebugGetAddressableLatch() const {
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
const SN76489 *BBCMicro::DebugGetSN76489() const {
    return &m_state.sn76489;
}
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if BBCMICRO_DEBUGGER
const MC146818 *BBCMicro::DebugGetRTC() const {
    if (m_type->flags & BBCMicroTypeFlag_HasRTC) {
        return &m_state.rtc;
    } else {
        return nullptr;
    }
}
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if BBCMICRO_DEBUGGER
void BBCMicro::DebugGetPaging(ROMSEL *romsel, ACCCON *acccon) const {
    *romsel = m_state.romsel;
    *acccon = m_state.acccon;
}
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if BBCMICRO_DEBUGGER
void BBCMicro::FinishAsyncCall(bool called) {
    if (m_async_call_fn) {
        (*m_async_call_fn)(called, m_async_call_context);
    }

    m_state.async_call_address.w = INVALID_ASYNC_CALL_ADDRESS;
    m_state.async_call_timeout = 0;
    m_async_call_fn = nullptr;
    m_async_call_context = nullptr;

    this->UpdateCPUDataBusFn();
}
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if BBCMICRO_DEBUGGER
const BBCMicro::BigPage *BBCMicro::DebugGetBigPageForAddress(M6502Word addr,
                                                             bool mos,
                                                             uint32_t dpo) const {
    ROMSEL romsel = m_state.romsel;
    ACCCON acccon = m_state.acccon;
    (*m_type->apply_dpo_fn)(&romsel, &acccon, dpo);

    MemoryBigPageTables tables;
    bool io, crt_shadow;
    (*m_type->get_mem_big_page_tables_fn)(&tables, &io, &crt_shadow, romsel, acccon);

    uint8_t big_page = tables.mem_big_pages[mos][addr.p.p];
    ASSERT(big_page < NUM_BIG_PAGES);
    const BigPage *bp = &m_big_pages[big_page];
    return bp;
}
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if BBCMICRO_DEBUGGER
void BBCMicro::GetMemBigPageIsMOSTable(uint8_t *mem_big_page_is_mos, uint32_t dpo) const {

    // Should maybe try to make this all fit together a bit better...

    ROMSEL romsel = m_state.romsel;
    ACCCON acccon = m_state.acccon;
    (*m_type->apply_dpo_fn)(&romsel, &acccon, dpo);

    MemoryBigPageTables tables;
    bool io, crt_shadow;
    (*m_type->get_mem_big_page_tables_fn)(&tables, &io, &crt_shadow, romsel, acccon);

    memcpy(mem_big_page_is_mos, tables.pc_mem_big_pages_set, 16);
}
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if BBCMICRO_DEBUGGER
uint8_t BBCMicro::DebugGetByteDebugFlags(const BigPage *big_page,
                                         uint32_t offset) const {
    ASSERT(offset < BIG_PAGE_SIZE_BYTES);

    if (big_page->debug) {
        return big_page->debug[offset & BIG_PAGE_OFFSET_MASK];
    } else {
        return 0;
    }
}
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if BBCMICRO_DEBUGGER
void BBCMicro::DebugSetByteDebugFlags(uint8_t big_page_index,
                                      uint32_t offset,
                                      uint8_t flags) {
    ASSERT(big_page_index < NUM_BIG_PAGES);
    ASSERT(offset < BIG_PAGE_SIZE_BYTES);

    BigPage *big_page = &m_big_pages[big_page_index];
    if (big_page->debug) {
        uint8_t *byte_flags = &big_page->debug[offset & BIG_PAGE_OFFSET_MASK];

        if (*byte_flags != flags) {
            *byte_flags = flags;

            ++m_debug->breakpoints_changed_counter;

            if (flags & BBCMicroByteDebugFlag_TempBreakExecute) {
                m_debug->temp_execute_breakpoints.push_back(byte_flags);
            }
        }
    }
}
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if BBCMICRO_DEBUGGER
const uint8_t *BBCMicro::DebugGetAddressDebugFlagsForMemBigPage(uint8_t mem_big_page) const {
    ASSERT(mem_big_page < 16);

    if (m_debug) {
        return &m_debug->address_debug_flags[mem_big_page << 12];
    } else {
        return nullptr;
    }
}
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if BBCMICRO_DEBUGGER
uint8_t BBCMicro::DebugGetAddressDebugFlags(M6502Word addr) const {
    if (m_debug) {
        return m_debug->address_debug_flags[addr.w];
    } else {
        return 0;
    }
}
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if BBCMICRO_DEBUGGER
void BBCMicro::DebugSetAddressDebugFlags(M6502Word addr, uint8_t flags) const {
    if (m_debug) {
        uint8_t *addr_flags = &m_debug->address_debug_flags[addr.w];

        if (*addr_flags != flags) {
            *addr_flags = flags;

            ++m_debug->breakpoints_changed_counter;

            if (flags & BBCMicroByteDebugFlag_TempBreakExecute) {
                m_debug->temp_execute_breakpoints.push_back(addr_flags);
            }
        }
    }
}
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if BBCMICRO_DEBUGGER
void BBCMicro::DebugGetBytes(uint8_t *bytes, size_t num_bytes, M6502Word addr, uint32_t dpo) {
    // Not currently very clever.
    for (size_t i = 0; i < num_bytes; ++i) {
        const BigPage *bp = this->DebugGetBigPageForAddress(addr, {}, dpo);

        if (bp->r) {
            bytes[i] = bp->r[addr.p.o];
        } else {
            bytes[i] = 0;
        }

        ++addr.w;
    }
}
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if BBCMICRO_DEBUGGER
void BBCMicro::DebugSetBytes(M6502Word addr, uint32_t dpo, const uint8_t *bytes, size_t num_bytes) {
    // Not currently very clever.
    for (size_t i = 0; i < num_bytes; ++i) {
        const BigPage *bp = this->DebugGetBigPageForAddress(addr, {}, dpo);

        if (bp->w) {
            bp->w[addr.p.o] = bytes[i];
        }

        ++addr.w;
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
void BBCMicro::DebugHalt(const char *fmt, ...) {
    if (m_debug) {
        m_debug->is_halted = true;

        if (fmt) {
            va_list v;

            va_start(v, fmt);
            vsnprintf(m_debug->halt_reason, sizeof m_debug->halt_reason, fmt, v);
            va_end(v);
        } else {
            m_debug->halt_reason[0] = 0;
        }

        if (!m_debug->temp_execute_breakpoints.empty()) {
            for (uint8_t *flags : m_debug->temp_execute_breakpoints) {
                // Not even sure this isn't UB or something.
                ASSERT(((uintptr_t)flags >= (uintptr_t)m_debug->big_pages_debug_flags &&
                        (uintptr_t)flags < (uintptr_t)((char *)m_debug->big_pages_debug_flags + sizeof m_debug->big_pages_debug_flags)) ||
                       ((uintptr_t)flags >= (uintptr_t)m_debug->address_debug_flags &&
                        (uintptr_t)flags < (uintptr_t)((char *)m_debug->address_debug_flags + sizeof m_debug->address_debug_flags)));

                // Doesn't matter.
                //ASSERT(*flags&BBCMicroByteDebugFlag_TempBreakExecute);

                *flags &= ~BBCMicroByteDebugFlag_TempBreakExecute;
            }

            m_debug->temp_execute_breakpoints.clear();

            ++m_debug->breakpoints_changed_counter;
        }

        this->SetDebugStepType(BBCMicroStepType_None);
    }
}
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if BBCMICRO_DEBUGGER
bool BBCMicro::DebugIsHalted() const {
    if (!m_debug) {
        return false;
    }

    return m_debug->is_halted;
}
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if BBCMICRO_DEBUGGER
const char *BBCMicro::DebugGetHaltReason() const {
    if (!m_debug) {
        return nullptr;
    }

    if (m_debug->halt_reason[0] == 0) {
        return nullptr;
    }

    return m_debug->halt_reason;
}
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if BBCMICRO_DEBUGGER
void BBCMicro::DebugRun() {
    if (!m_debug) {
        return;
    }

    m_debug->is_halted = false;
}
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if BBCMICRO_DEBUGGER
//BBCMicro::DebugState::ByteDebugFlags BBCMicro::DebugGetByteFlags(M6502Word addr) const {
//    if(!m_debug) {
//        return DUMMY_BYTE_DEBUG_FLAGS;
//    }
//
//    if(m_pc_big_pages) {
//        return m_pc_pages[m_state.cpu.opcode_pc.b.h]->debug[addr.b.h][addr.b.l];
//    } else {
//        return m_pages.debug[addr.b.h][addr.b.l];
//    }
//}
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if BBCMICRO_DEBUGGER
//void BBCMicro::DebugSetByteFlags(M6502Word addr,DebugState::ByteDebugFlags flags) {
//    if(!m_debug) {
//        return;
//    }
//
//    if(m_pc_pages) {
//        m_pc_pages[m_state.cpu.opcode_pc.b.h]->debug[addr.b.h][addr.b.l]=flags;
//    } else {
//        m_pages.debug[addr.b.h][addr.b.l]=flags;
//    }
//}
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if BBCMICRO_DEBUGGER
void BBCMicro::DebugStepIn() {
    if (!m_debug) {
        return;
    }

    this->SetDebugStepType(BBCMicroStepType_StepIn);
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
    std::unique_ptr<BBCMicro::DebugState> debug = std::move(m_debug_ptr);

    m_debug = nullptr;

    this->UpdateDebugState();

    return debug;
}
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if BBCMICRO_DEBUGGER
void BBCMicro::SetDebugState(std::unique_ptr<DebugState> debug) {
    m_debug_ptr = std::move(debug);
    m_debug = m_debug_ptr.get();

    this->UpdateDebugState();
}
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if BBCMICRO_DEBUGGER
BBCMicro::HardwareDebugState BBCMicro::GetHardwareDebugState() const {
    if (!m_debug) {
        return HardwareDebugState();
    }

    return m_debug->hw;
}
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if BBCMICRO_DEBUGGER
void BBCMicro::SetHardwareDebugState(const HardwareDebugState &hw) {
    if (!m_debug) {
        return;
    }

    m_debug->hw = hw;
}
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if BBCMICRO_DEBUGGER
void BBCMicro::DebugSetAsyncCall(uint16_t address, uint8_t a, uint8_t x, uint8_t y, bool c, DebugAsyncCallFn fn, void *context) {
    this->FinishAsyncCall(false);

    m_state.async_call_address.w = address;
    m_state.async_call_timeout = ASYNC_CALL_TIMEOUT;
    m_state.async_call_a = a;
    m_state.async_call_x = x;
    m_state.async_call_y = y;
    m_state.async_call_c = c;
    m_async_call_fn = fn;
    m_async_call_context = context;

    this->UpdateCPUDataBusFn();
}
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if BBCMICRO_DEBUGGER
uint32_t BBCMicro::DebugGetCurrentPageOverride() const {
    return (*m_type->get_dpo_fn)(m_state.romsel, m_state.acccon);
}
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if BBCMICRO_DEBUGGER
uint64_t BBCMicro::DebugGetBreakpointsChangeCounter() const {
    if (m_debug) {
        return m_debug->breakpoints_changed_counter;
    } else {
        return 0;
    }
}
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if BBCMICRO_DEBUGGER
void BBCMicro::DebugGetDebugFlags(uint8_t *addr_debug_flags,
                                  uint8_t *big_pages_debug_flags) const {
    if (m_debug) {
        memcpy(addr_debug_flags, m_debug->address_debug_flags, 65536);
        memcpy(big_pages_debug_flags, m_debug->big_pages_debug_flags, NUM_BIG_PAGES * BIG_PAGE_SIZE_BYTES);
    } else {
        memset(addr_debug_flags, 0, 65536);
        memset(big_pages_debug_flags, 0, NUM_BIG_PAGES * BIG_PAGE_SIZE_BYTES);
    }
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

void BBCMicro::TestSetByte(uint16_t ram_buffer_index, uint8_t value) {
    ASSERT(ram_buffer_index < m_state.ram_buffer.size());
    m_state.ram_buffer[ram_buffer_index] = value;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BBCMicro::TestRTS() {
    ASSERT(M6502_IsAboutToExecute(&m_state.cpu));
    m_state.cpu.dbus = 0x60;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if BBCMICRO_DEBUGGER
void BBCMicro::UpdateDebugBigPages(MemoryBigPages *mem_big_pages) {
    for (size_t i = 0; i < 16; ++i) {
        mem_big_pages->debug[i] = mem_big_pages->bp[i]->debug;
    }
}
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if BBCMICRO_DEBUGGER
void BBCMicro::UpdateDebugState() {
    this->UpdateCPUDataBusFn();

    // Update debug page pointers.
    for (size_t i = 0; i < NUM_BIG_PAGES; ++i) {
        BigPage *bp = &m_big_pages[i];

        if (m_debug) {
            bp->debug = m_debug->big_pages_debug_flags[bp->index];
        } else {
            bp->debug = nullptr;
        }
    }

    for (size_t i = 0; i < 2; ++i) {
        MemoryBigPages *mbp = &m_mem_big_pages[i];

        for (size_t j = 0; j < 16; ++j) {
            mbp->debug[j] = mbp->bp[j] ? mbp->bp[j]->debug : nullptr;
        }
    }

    //    // Update the debug page pointers.
    //    if(m_shadow_mem_big_pages) {
    //        this->UpdateDebugBigPages(m_shadow_mem_big_pages);
    //    }
    //
    //    this->UpdateDebugBigPages(&m_main_mem_big_pages);
}
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if BBCMICRO_DEBUGGER
void BBCMicro::SetDebugStepType(BBCMicroStepType step_type) {
    if (m_debug) {
        if (m_debug->step_type != step_type) {
            m_debug->step_type = step_type;
            this->UpdateCPUDataBusFn();
        }
    }
}
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BBCMicro::InitStuff() {
    CHECK_SIZEOF(AddressableLatch, 1);
    CHECK_SIZEOF(ROMSEL, 1);
    CHECK_SIZEOF(ACCCON, 1);
    CHECK_SIZEOF(SystemVIAPB, 1);
    static_assert(::NUM_BIG_PAGES == BBCMicro::NUM_BIG_PAGES, "oops");

    m_ram = m_state.ram_buffer.data();

    m_state.cpu.context = this;

    for (int i = 0; i < 3; ++i) {
        m_hw_rmmio_fns[i] = std::vector<ReadMMIOFn>(256);
        m_hw_wmmio_fns[i] = std::vector<WriteMMIOFn>(256);
        m_hw_mmio_fn_contexts[i] = std::vector<void *>(256);
        m_hw_mmio_stretch[i] = std::vector<uint8_t>(256);

        // Assume hardware is mapped. It will get fixed up later if
        // not.
        m_rmmio_fns[i] = m_hw_rmmio_fns[i].data();
        m_mmio_fn_contexts[i] = m_hw_mmio_fn_contexts[i].data();
        m_mmio_stretch[i] = m_hw_mmio_stretch[i].data();
        m_rom_mmio = false;
    }

    // initially no I/O
    for (uint16_t i = 0xfc00; i < 0xff00; ++i) {
        this->SetMMIOFns(i, nullptr, nullptr, nullptr);
    }

#if BBCMICRO_DEBUGGER
    for (size_t i = 0; i < sizeof m_state.async_call_thunk_buf; ++i) {
        this->SetMMIOFns((uint16_t)(ASYNC_CALL_THUNK_ADDR.w + i), &ReadAsyncCallThunk, nullptr, this);
    }
#endif

    if (m_ext_mem) {
        m_state.ext_mem.AllocateBuffer();

        this->SetMMIOFns(0xfc00, nullptr, &ExtMem::WriteAddressL, &m_state.ext_mem);
        this->SetMMIOFns(0xfc01, nullptr, &ExtMem::WriteAddressH, &m_state.ext_mem);
        this->SetMMIOFns(0xfc02, &ExtMem::ReadAddressL, nullptr, &m_state.ext_mem);
        this->SetMMIOFns(0xfc03, &ExtMem::ReadAddressH, nullptr, &m_state.ext_mem);

        for (uint16_t i = 0xfd00; i <= 0xfdff; ++i) {
            this->SetMMIOFns(i, &ExtMem::ReadData, &ExtMem::WriteData, &m_state.ext_mem);
        }
    }

    // I/O: VIAs
    for (uint16_t i = 0; i < 32; ++i) {
        this->SetMMIOFns(0xfe40 + i, g_R6522_read_fns[i & 15], g_R6522_write_fns[i & 15], &m_state.system_via);
        this->SetMMIOFns(0xfe60 + i, g_R6522_read_fns[i & 15], g_R6522_write_fns[i & 15], &m_state.user_via);
    }

    // I/O: 6845
    for (int i = 0; i < 8; i += 2) {
        this->SetMMIOFns((uint16_t)(0xfe00 + i + 0), &CRTC::ReadAddress, &CRTC::WriteAddress, &m_state.crtc);
        this->SetMMIOFns((uint16_t)(0xfe00 + i + 1), &CRTC::ReadData, &CRTC::WriteData, &m_state.crtc);
    }

    // I/O: Video ULA
    m_state.video_ula.nula = m_video_nula;
    for (int i = 0; i < 2; ++i) {
        this->SetMMIOFns((uint16_t)(0xfe20 + i * 2), nullptr, &VideoULA::WriteControlRegister, &m_state.video_ula);
        this->SetMMIOFns((uint16_t)(0xfe21 + i * 2), nullptr, &VideoULA::WritePalette, &m_state.video_ula);
    }

    if (m_video_nula) {
        this->SetMMIOFns(0xfe22, nullptr, &VideoULA::WriteNuLAControlRegister, &m_state.video_ula);
        this->SetMMIOFns(0xfe23, nullptr, &VideoULA::WriteNuLAPalette, &m_state.video_ula);
    }

    // I/O: disc interface
    if (m_disc_interface) {
        m_state.fdc.SetHandler(this);
        m_state.fdc.SetNoINTRQ(!!(m_disc_interface->flags & DiscInterfaceFlag_NoINTRQ));
        m_state.fdc.Set1772(!!(m_disc_interface->flags & DiscInterfaceFlag_1772));

        M6502Word c = {m_disc_interface->control_addr};
        c.b.h -= 0xfc;
        ASSERT(c.b.h < 3);

        M6502Word f = {m_disc_interface->fdc_addr};
        f.b.h -= 0xfc;
        ASSERT(f.b.h < 3);

        for (int i = 0; i < 4; ++i) {
            this->SetMMIOFns((uint16_t)(m_disc_interface->fdc_addr + i), g_WD1770_read_fns[i], g_WD1770_write_fns[i], &m_state.fdc);
        }

        this->SetMMIOFns(m_disc_interface->control_addr, &Read1770ControlRegister, &Write1770ControlRegister, this);

        m_disc_interface->InstallExtraHardware(this);
    } else {
        m_state.fdc.SetHandler(nullptr);
    }

    m_state.system_via.SetID(BBCMicroVIAID_SystemVIA, "SystemVIA");
    m_state.user_via.SetID(BBCMicroVIAID_UserVIA, "UserVIA");

    m_state.old_system_via_pb.value = m_state.system_via.b.p;

    if (m_beeblink_handler) {
        m_beeblink = std::make_unique<BeebLink>(m_beeblink_handler);
    }

    this->UpdateCPUDataBusFn();

    m_romsel_mask = m_type->romsel_mask;
    m_acccon_mask = m_type->acccon_mask;

    if (m_type->flags & BBCMicroTypeFlag_CanDisplayTeletext3c00) {
        m_teletext_bases[0] = 0x3c00;
        m_teletext_bases[1] = 0x7c00;
    } else {
        m_teletext_bases[0] = 0x7c00;
        m_teletext_bases[1] = 0x7c00;
    }

    if (m_acccon_mask == 0) {
        for (uint16_t i = 0; i < 16; ++i) {
            this->SetMMIOFns((uint16_t)(0xfe30 + i), &ReadROMSEL, &WriteROMSEL, this);
        }
    } else {
        for (uint16_t i = 0; i < 4; ++i) {
            this->SetMMIOFns((uint16_t)(0xfe30 + i), &ReadROMSEL, &WriteROMSEL, this);
            this->SetMMIOFns((uint16_t)(0xfe34 + i), &ReadACCCON, &WriteACCCON, this);
        }
    }

    m_has_rtc = !!(m_type->flags & BBCMicroTypeFlag_HasRTC);

    for (int i = 0; i < 3; ++i) {
        m_rom_rmmio_fns = std::vector<ReadMMIOFn>(256, &ReadROMMMIO);
        m_rom_mmio_fn_contexts = std::vector<void *>(256, this);
        m_rom_mmio_stretch = std::vector<uint8_t>(256, 0x00);
    }

    // FRED = all stretched
    m_hw_mmio_stretch[0] = std::vector<uint8_t>(256, 0xff);

    // JIM = all stretched
    m_hw_mmio_stretch[1] = std::vector<uint8_t>(256, 0xff);

    // SHEILA = part stretched
    m_hw_mmio_stretch[2] = std::vector<uint8_t>(256, 0x00);
    for (const BBCMicroType::SHEILACycleStretchRegion &region : m_type->sheila_cycle_stretch_regions) {
        ASSERT(region.first < region.last);
        for (uint8_t i = region.first; i <= region.last; ++i) {
            m_hw_mmio_stretch[2][i] = 0xff;
        }
    }

    // Page in current ROM bank and sort out ACCCON.
    this->InitPaging();

#if BBCMICRO_ENABLE_DISC_DRIVE_SOUND
    this->InitDiscDriveSounds(m_type->default_disc_drive_type);
#endif

#if BBCMICRO_TRACE
    this->SetTrace(nullptr, 0);
#endif

    for (int i = 0; i < 3; ++i) {
        ASSERT(m_rmmio_fns[i] == m_hw_rmmio_fns[i].data() || m_rmmio_fns[i] == m_rom_rmmio_fns.data());
        ASSERT(m_mmio_fn_contexts[i] == m_hw_mmio_fn_contexts[i].data() || m_mmio_fn_contexts[i] == m_rom_mmio_fn_contexts.data());
        ASSERT(m_mmio_stretch[i] == m_hw_mmio_stretch[i].data() || m_mmio_stretch[i] == m_rom_mmio_stretch.data());
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool BBCMicro::IsTrack0() {
    if (DiscDrive *dd = this->GetDiscDrive()) {
        return dd->track == 0;
    }

    return false;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BBCMicro::StepOut() {
    if (DiscDrive *dd = this->GetDiscDrive()) {
        if (dd->track > 0) {
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
    if (DiscDrive *dd = this->GetDiscDrive()) {
        if (dd->track < 255) {
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
    if (DiscDrive *dd = this->GetDiscDrive()) {
        dd->motor = true;

#if BBCMICRO_ENABLE_DISC_DRIVE_SOUND
        dd->spin_sound_index = 0;
        dd->spin_sound = DiscDriveSound_SpinStartLoaded;
#endif
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BBCMicro::SpinDown() {
    if (DiscDrive *dd = this->GetDiscDrive()) {
        dd->motor = false;

#if BBCMICRO_ENABLE_DISC_DRIVE_SOUND
        dd->spin_sound_index = 0;
        dd->spin_sound = DiscDriveSound_SpinEnd;
#endif
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool BBCMicro::IsWriteProtected() {
    if (this->GetDiscDrive()) {
        if (m_disc_images[m_state.disc_control.drive]) {
            if (m_disc_images[m_state.disc_control.drive]->IsWriteProtected()) {
                return true;
            }

            if (m_is_drive_write_protected[m_state.disc_control.drive]) {
                return true;
            }
        }
    }

    return false;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool BBCMicro::GetByte(uint8_t *value, uint8_t sector, size_t offset) {
    if (DiscDrive *dd = this->GetDiscDrive()) {
        m_disc_access = true;

        if (m_disc_images[m_state.disc_control.drive]) {
            if (m_disc_images[m_state.disc_control.drive]->Read(value,
                                                                m_state.disc_control.side,
                                                                dd->track,
                                                                sector,
                                                                offset)) {
                return true;
            }
        }
    }

    return false;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool BBCMicro::SetByte(uint8_t sector, size_t offset, uint8_t value) {
    if (DiscDrive *dd = this->GetDiscDrive()) {
        m_disc_access = true;

        if (m_disc_images[m_state.disc_control.drive]) {
            if (m_disc_images[m_state.disc_control.drive]->Write(m_state.disc_control.side,
                                                                 dd->track,
                                                                 sector,
                                                                 offset,
                                                                 value)) {
                return true;
            }
        }
    }

    return false;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool BBCMicro::GetSectorDetails(uint8_t *track, uint8_t *side, size_t *size, uint8_t sector, bool double_density) {
    if (DiscDrive *dd = this->GetDiscDrive()) {
        m_disc_access = true;

        if (m_disc_images[m_state.disc_control.drive]) {
            if (m_disc_images[m_state.disc_control.drive]->GetDiscSectorSize(size, m_state.disc_control.side, dd->track, sector, double_density)) {
                *track = dd->track;
                *side = m_state.disc_control.side;
                return true;
            }
        }
    }

    return false;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

BBCMicro::DiscDrive *BBCMicro::GetDiscDrive() {
    if (m_state.disc_control.drive >= 0 && m_state.disc_control.drive < NUM_DRIVES) {
        return &m_state.drives[m_state.disc_control.drive];
    } else {
        return nullptr;
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if BBCMICRO_ENABLE_DISC_DRIVE_SOUND
void BBCMicro::InitDiscDriveSounds(DiscDriveType type) {
    auto &&it = g_disc_drive_sounds.find(type);
    if (it == g_disc_drive_sounds.end()) {
        return;
    }

    for (size_t i = 0; i < DiscDriveSound_EndValue; ++i) {
        m_disc_drive_sounds[i] = &it->second[i];
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

#define SEEK_SOUND(N) \
    { SOUND_CLOCKS_FROM_MS(N), DiscDriveSound_Seek##N##ms, }

static const SeekSound g_seek_sounds[] = {
    {
        SOUND_CLOCKS_FROM_MS(17),
        DiscDriveSound_Seek20ms,
    },
    {
        SOUND_CLOCKS_FROM_MS(10),
        DiscDriveSound_Seek12ms,
    },
    {
        SOUND_CLOCKS_FROM_MS(4),
        DiscDriveSound_Seek6ms,
    },
    {
        1,
        DiscDriveSound_Seek2ms,
    },
    {0},
};

void BBCMicro::StepSound(DiscDrive *dd) {
    if (dd->step_sound_index < 0) {
        // step
        dd->step_sound_index = 0;
    } else if (dd->seek_sound == DiscDriveSound_EndValue) {
        // skip a bit of the step sound
        dd->step_sound_index += SOUND_CLOCK_HZ / 100;

        // seek. Start with 20ms... it's as good a guess as any.
        dd->seek_sound = DiscDriveSound_Seek20ms;
        dd->seek_sound_index = 0;
    } else {
        for (const SeekSound *seek_sound = g_seek_sounds; seek_sound->clock_ticks != 0; ++seek_sound) {
            if (dd->seek_sound_index >= seek_sound->clock_ticks) {
                if (dd->seek_sound != seek_sound->sound) {
                    dd->seek_sound = seek_sound->sound;
                    dd->seek_sound_index = 0;
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
    float acc = 0.f;

    if (dd->spin_sound != DiscDriveSound_EndValue) {
        ASSERT(dd->spin_sound >= 0 && dd->spin_sound < DiscDriveSound_EndValue);
        const std::vector<float> *spin_sound = m_disc_drive_sounds[dd->spin_sound];

        acc += (*spin_sound)[dd->spin_sound_index];

        ++dd->spin_sound_index;
        if (dd->spin_sound_index >= spin_sound->size()) {
            switch (dd->spin_sound) {
            case DiscDriveSound_SpinStartEmpty:
            case DiscDriveSound_SpinEmpty:
                dd->spin_sound = DiscDriveSound_SpinEmpty;
                break;

            case DiscDriveSound_SpinStartLoaded:
            case DiscDriveSound_SpinLoaded:
                dd->spin_sound = DiscDriveSound_SpinLoaded;
                break;

            default:
                dd->spin_sound = DiscDriveSound_EndValue;
                break;
            }

            dd->spin_sound_index = 0;
        }
    }

    if (dd->seek_sound != DiscDriveSound_EndValue) {
        const std::vector<float> *seek_sound = m_disc_drive_sounds[dd->seek_sound];

        acc += (*seek_sound)[dd->seek_sound_index];

        ++dd->seek_sound_index;
        if ((size_t)dd->seek_sound_index >= seek_sound->size()) {
            dd->seek_sound = DiscDriveSound_EndValue;
        }
    } else if (dd->step_sound_index >= 0) {
        const std::vector<float> *step_sound = m_disc_drive_sounds[DiscDriveSound_Step];

        // check for end first as the playback position is adjusted in
        // StepSound.
        if ((size_t)dd->step_sound_index >= step_sound->size()) {
            dd->step_sound_index = -1;
        } else {
            acc += (*step_sound)[(size_t)dd->step_sound_index++];
        }
    }

    return acc;
}
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BBCMicro::UpdateCPUDataBusFn() {
#if BBCMICRO_DEBUGGER
    if (m_debug) {
        m_default_handle_cpu_data_bus_fn = &HandleCPUDataBusWithShadowRAMDebug;
    } else {
        m_default_handle_cpu_data_bus_fn = &HandleCPUDataBusWithShadowRAM;
    }
#else
    m_default_handle_cpu_data_bus_fn = &HandleCPUDataBusWithShadowRAM;
#endif

    if (m_state.hack_flags != 0) {
        goto hack;
    }

#if BBCMICRO_TRACE
    if (m_trace) {
        goto hack;
    }
#endif

#if BBCMICRO_DEBUGGER
    if (m_debug) {
        if (m_debug->step_type != BBCMicroStepType_None) {
            goto hack;
        }
    }
#endif

    if (!m_instruction_fns.empty()) {
        goto hack;
    }

    if (!m_write_fns.empty()) {
        goto hack;
    }

#if BBCMICRO_DEBUGGER
    if (m_state.async_call_address.w != INVALID_ASYNC_CALL_ADDRESS) {
        goto hack;
    }
#endif

    // No hacks.
    m_handle_cpu_data_bus_fn = m_default_handle_cpu_data_bus_fn;
    return;

hack:;
    m_handle_cpu_data_bus_fn = &HandleCPUDataBusWithHacks;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
