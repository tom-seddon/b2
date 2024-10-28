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
#include <beeb/tube.h>
#include <set>
#include <unordered_set>
#include <shared/sha1.h>

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
static const std::vector<float> DUMMY_DISC_DRIVE_SOUND(1, 0.f);

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if BBCMICRO_NUM_UPDATE_GROUPS > 1
BBCMicro::UpdateMFn BBCMicro::ms_update_mfns[NUM_UPDATE_MFNS];
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// The key to press to start the paste going.
const BeebKey BBCMicro::PASTE_START_KEY = BeebKey_Space;

// The corresponding char, so it can be removed when copying the BASIC
// listing.
const char BBCMicro::PASTE_START_CHAR = ' ';

#if BBCMICRO_TRACE
const TraceEventType BBCMicro::INSTRUCTION_EVENT("Instruction", sizeof(InstructionTraceEvent), TraceEventSource_None);
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

static const uint8_t g_unmapped_reads[BIG_PAGE_SIZE_BYTES] = {
    0,
};
static uint8_t g_unmapped_writes[BIG_PAGE_SIZE_BYTES];

const uint16_t BBCMicro::SCREEN_WRAP_ADJUSTMENTS[] = {
    0x4000 >> 3,
    0x2000 >> 3,
    0x5000 >> 3,
    0x2800 >> 3,
};

const uint16_t BBCMicro::ADJI_ADDRESSES[4] = {
    0xfcc0,
    0xfcd0,
    0xfce0,
    0xfcf0,
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

BBCMicro::BBCMicro(std::shared_ptr<const BBCMicroType> type,
                   const DiscInterface *disc_interface,
                   BBCMicroParasiteType parasite_type,
                   const std::vector<uint8_t> &nvram_contents,
                   const tm *rtc_time,
                   uint32_t init_flags,
                   BeebLinkHandler *beeblink_handler,
                   CycleCount initial_cycle_count)
    : m_state(std::move(type),
              disc_interface,
              parasite_type,
              nvram_contents,
              init_flags,
              rtc_time,
              initial_cycle_count)
    , m_beeblink_handler(beeblink_handler) {
    this->InitStuff();
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

BBCMicro::BBCMicro(const BBCMicroUniqueState &state)
    : m_state(state) {
    this->InitStuff();
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

BBCMicro::~BBCMicro() {
#if BBCMICRO_TRACE
    this->StopTrace(nullptr);
#endif
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

uint32_t BBCMicro::GetCloneImpediments() const {
    uint32_t result = 0;

    for (int i = 0; i < NUM_DRIVES; ++i) {
        const BBCMicroState::DiscDrive *drive = &m_state.drives[i];
        if (!!drive->disc_image) {
            if (!drive->disc_image->CanClone()) {
                result |= (uint32_t)BBCMicroCloneImpediment_Drive0 << i;
            }
        }
    }

    if (!!m_beeblink_handler) {
        result |= BBCMicroCloneImpediment_BeebLink;
    }

    return result;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

const BBCMicroUniqueState *BBCMicro::GetUniqueState() const {
    if (this->GetCloneImpediments() != 0) {
        return nullptr;
    }

    return &m_state;
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
        m_trace->SetTime(&m_state.cycle_count);
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
    SetTubeTrace(&m_state.parasite_tube, trace_flags & BBCMicroTraceFlag_Tube ? m_trace : nullptr);
    m_state.adc.SetTrace(trace_flags & BBCMicroTraceFlag_ADC ? m_trace : nullptr);
    SetPCD8572Trace(&m_state.eeprom, trace_flags & BBCMicroTraceFlag_EEPROM ? m_trace : nullptr);

    if (!!m_beeblink) {
        m_beeblink->SetTrace(trace_flags & BBCMicroTraceFlag_BeebLink ? m_trace : nullptr);
    }

    this->UpdateCPUDataBusFn();
}
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static_assert(PagingFlags_ROMIO == 1);
static_assert(PagingFlags_IFJ == 2);
std::vector<BBCMicro::ReadMMIO> BBCMicro::*BBCMicro::ms_read_mmios_mptrs[] = {
    &BBCMicro::m_read_mmios_hw,           //0
    &BBCMicro::m_read_mmios_rom,          //ROMIO
    &BBCMicro::m_read_mmios_hw_cartridge, //IFJ
    &BBCMicro::m_read_mmios_rom,          //IFJ|ROMIO
};

std::vector<uint8_t> BBCMicro::*BBCMicro::ms_read_mmios_stretch_mptrs[] = {
    &BBCMicro::m_mmios_stretch_hw,           //0
    &BBCMicro::m_mmios_stretch_rom,          //ROMIO
    &BBCMicro::m_mmios_stretch_hw_cartridge, //IFJ
    &BBCMicro::m_mmios_stretch_rom,          //IFJ|ROMIO
};

std::vector<BBCMicro::WriteMMIO> BBCMicro::*BBCMicro::ms_write_mmios_mptrs[] = {
    &BBCMicro::m_write_mmios_hw,           //0
    &BBCMicro::m_write_mmios_hw,           //ROMIO
    &BBCMicro::m_write_mmios_hw_cartridge, //IFJ
    &BBCMicro::m_write_mmios_hw_cartridge, //IFJ|ROMIO
};

std::vector<uint8_t> BBCMicro::*BBCMicro::ms_write_mmios_stretch_mptrs[] = {
    &BBCMicro::m_mmios_stretch_hw,           //0
    &BBCMicro::m_mmios_stretch_hw,           //ROMIO
    &BBCMicro::m_mmios_stretch_hw_cartridge, //IFJ
    &BBCMicro::m_mmios_stretch_hw_cartridge, //IFJ|ROMIO
};

void BBCMicro::UpdatePaging() {
    MemoryBigPageTables tables;
    uint32_t paging_flags;
    (*m_state.type->get_mem_big_page_tables_fn)(&tables, &paging_flags, m_state.paging);

    for (size_t i = 0; i < 2; ++i) {
        MemoryBigPages *mbp = &m_mem_big_pages[i];

        for (size_t j = 0; j < 16; ++j) {
            ASSERT(tables.mem_big_pages[i][j].i < NUM_BIG_PAGES);
            const BigPage *bp = &m_big_pages[tables.mem_big_pages[i][j].i];

            mbp->w[j] = bp->w;
            mbp->r[j] = bp->r;
#if BBCMICRO_DEBUGGER
            mbp->byte_debug_flags[j] = bp->byte_debug_flags;
            mbp->bp[j] = bp;
#endif
        }
    }

    for (size_t i = 0; i < 16; ++i) {
        ASSERT(tables.pc_mem_big_pages_set[i] == 0 || tables.pc_mem_big_pages_set[i] == 1);
        m_pc_mem_big_pages[i] = &m_mem_big_pages[tables.pc_mem_big_pages_set[i]];
    }

    if (paging_flags & PagingFlags_DisplayShadow) {
        m_state.shadow_select_mask = 0x8000;
    } else {
        m_state.shadow_select_mask = 0;
    }

    uint32_t index = paging_flags & (PagingFlags_ROMIO | PagingFlags_IFJ);
    m_read_mmios = (this->*ms_read_mmios_mptrs[index]).data();
    m_read_mmios_stretch = (this->*ms_read_mmios_stretch_mptrs[index]).data();
    m_write_mmios = (this->*ms_write_mmios_mptrs[index]).data();
    m_write_mmios_stretch = (this->*ms_write_mmios_stretch_mptrs[index]).data();

    bool parasite_accessible;
    switch (m_state.parasite_type) {
    default:
        ASSERT(false);
        // fall through
    case BBCMicroParasiteType_None:
        parasite_accessible = false;
        break;

    case BBCMicroParasiteType_External3MHz6502:
        if (m_state.type->type_id == BBCMicroTypeID_Master) {
            parasite_accessible = !m_state.paging.acccon.m128_bits.itu;
        } else {
            parasite_accessible = true;
        }
        break;

    case BBCMicroParasiteType_MasterTurbo:
        if (m_state.type->type_id == BBCMicroTypeID_Master) {
            parasite_accessible = m_state.paging.acccon.m128_bits.itu;
        } else {
            parasite_accessible = true;
        }
        break;
    }

    if (parasite_accessible != m_state.parasite_accessible) {
        if (parasite_accessible) {
            static constexpr ReadMMIOFn host_rmmio_fns[7] = {
                &ReadHostTube1,
                &ReadHostTube2,
                &ReadHostTube3,
                &ReadHostTube4,
                &ReadHostTube5,
                &ReadHostTube6,
                &ReadHostTube7,
            };

            static constexpr WriteMMIOFn host_wmmio_fns[7] = {
                &WriteHostTube1,
                &WriteTubeDummy,
                &WriteHostTube3,
                &WriteTubeDummy,
                &WriteHostTube5,
                &WriteTubeDummy,
                &WriteHostTube7,
            };

            for (uint16_t a = 0xfee0; a < 0xff00; a += 8) {
                this->SetSIO(a + 0, &ReadHostTube0, &m_state.parasite_tube, &WriteHostTube0Wrapper, this);
                for (uint16_t i = 0; i < 7; ++i) {
                    this->SetSIO(a + 1 + i, host_rmmio_fns[i], &m_state.parasite_tube, host_wmmio_fns[i], &m_state.parasite_tube);
                }
            }
        } else {
            for (uint16_t a = 0xfee0; a < 0xff00; ++a) {
                this->SetSIO(a, nullptr, nullptr, nullptr, nullptr);
            }
        }

        m_state.parasite_accessible = parasite_accessible;
    }

#if BBCMICRO_DEBUGGER
    ++m_update_mfn_data->num_UpdatePaging_calls;
#endif
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BBCMicro::WriteHostTube0Wrapper(void *m_, M6502Word a, uint8_t value) {
    auto m = (BBCMicro *)m_;

    //uint8_t old_status = m->m_state.parasite_tube.status.value;

    WriteHostTube0(&m->m_state.parasite_tube, a, value);

    m->UpdateCPUDataBusFn();
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// TODO: this should probably be part of BBCMicroType, or something...?
void BBCMicro::InitReadOnlyBigPage(ReadOnlyBigPage *bp,
                                   const BBCMicroState *state,
#if BBCMICRO_DEBUGGER
                                   const DebugState *debug_state,
#endif
                                   BigPageIndex big_page_index) {
    bp->writeable = false;
    bp->r = nullptr;

    if (big_page_index.i >= 0 &&
        big_page_index.i < 16) {
        size_t offset = big_page_index.i * BIG_PAGE_SIZE_BYTES;

        if (offset < state->ram_buffer->size()) {
            bp->r = &state->ram_buffer->at(offset);
            bp->writeable = true;
        }
    } else if (big_page_index.i >= ROM0_BIG_PAGE_INDEX.i &&
               big_page_index.i < ROM0_BIG_PAGE_INDEX.i + 16 * NUM_ROM_BIG_PAGES) {
        size_t bank = ((size_t)big_page_index.i - ROM0_BIG_PAGE_INDEX.i) / NUM_ROM_BIG_PAGES;
        ASSERT(bank < 16);
        size_t region = (((size_t)big_page_index.i - ROM0_BIG_PAGE_INDEX.i) % NUM_ROM_BIG_PAGES) / 4;
        ASSERT(region < 8);
        size_t rom_big_page_index = (((size_t)big_page_index.i - ROM0_BIG_PAGE_INDEX.i) % NUM_ROM_BIG_PAGES) % 4;
        ASSERT(rom_big_page_index < 4);

        size_t offset = GetROMOffset(state->paging.rom_types[bank], (uint8_t)rom_big_page_index, (uint8_t)region);
        ASSERT(offset < GetROMTypeMetadata(state->paging.rom_types[bank])->num_bytes);
        //size_t offset = ((size_t)big_page_index.i - ROM0_BIG_PAGE_INDEX.i) % NUM_ROM_BIG_PAGES * BIG_PAGE_SIZE_BYTES;

        if (!!state->sideways_rom_buffers[bank]) {
            bp->r = &state->sideways_rom_buffers[bank]->at(offset);
        } else if (!!state->sideways_ram_buffers[bank]) {
            bp->r = &state->sideways_ram_buffers[bank]->at(offset);
            bp->writeable = true;
        }
    } else if (big_page_index.i >= MOS_BIG_PAGE_INDEX.i &&
               big_page_index.i < MOS_BIG_PAGE_INDEX.i + NUM_MOS_BIG_PAGES) {
        if (!!state->os_buffer) {
            size_t offset = (big_page_index.i - MOS_BIG_PAGE_INDEX.i) * BIG_PAGE_SIZE_BYTES;
            bp->r = &state->os_buffer->at(offset);
        }
    } else if (big_page_index.i >= PARASITE_BIG_PAGE_INDEX.i &&
               big_page_index.i < PARASITE_BIG_PAGE_INDEX.i + NUM_PARASITE_BIG_PAGES) {
        if (state->parasite_type != BBCMicroParasiteType_None) {
            size_t offset = (big_page_index.i - PARASITE_BIG_PAGE_INDEX.i) * BIG_PAGE_SIZE_BYTES;
            bp->r = &state->parasite_ram_buffer->at(offset);
            bp->writeable = true;
        }
    } else if (big_page_index.i >= PARASITE_ROM_BIG_PAGE_INDEX.i &&
               big_page_index.i < PARASITE_ROM_BIG_PAGE_INDEX.i + NUM_PARASITE_ROM_BIG_PAGES) {
        // During the initialisation, this can get called before the parasite OS
        // contents are set. Don't assert there's a rom buffer. Leave the area
        // unmapped if there isn't.
        if (!!state->parasite_rom_buffer) {
            size_t offset = (big_page_index.i - PARASITE_ROM_BIG_PAGE_INDEX.i) * BIG_PAGE_SIZE_BYTES;
            bp->r = &state->parasite_rom_buffer->at(offset);
        }
    } else {
        ASSERT(false);
    }

    bp->index = big_page_index;
    bp->metadata = &state->type->big_pages_metadata[bp->index.i];

#if BBCMICRO_DEBUGGER
    bp->byte_debug_flags = nullptr;
    bp->address_debug_flags = nullptr;

    if (debug_state) {
        if (bp->metadata->addr != 0xffff) {
            ASSERT(bp->metadata->addr % BIG_PAGE_SIZE_BYTES == 0);
            bp->byte_debug_flags = debug_state->big_pages_byte_debug_flags[bp->metadata->debug_flags_index.i];

            if (bp->metadata->is_parasite) {
                bp->address_debug_flags = &debug_state->parasite_address_debug_flags[bp->metadata->addr];
            } else {
                bp->address_debug_flags = &debug_state->host_address_debug_flags[bp->metadata->addr];
            }
        }
    }
#endif
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BBCMicro::InitPaging() {
    for (BigPage &bp : m_big_pages) {
        bp = {};
    }

    for (BigPageIndex i = {0}; i.i < NUM_BIG_PAGES; ++i.i) {
        ReadOnlyBigPage rbp;
        InitReadOnlyBigPage(&rbp,
                            &m_state,
#if BBCMICRO_DEBUGGER
                            m_debug,
#endif
                            i);

        BigPage *bp = &m_big_pages[i.i];
        bp->index = rbp.index;
        bp->metadata = rbp.metadata;
        bp->r = rbp.r;
        if (rbp.writeable) {
            bp->w = const_cast<uint8_t *>(bp->r);
        }

#if BBCMICRO_DEBUGGER
        bp->address_debug_flags = const_cast<uint8_t *>(rbp.address_debug_flags);
        bp->byte_debug_flags = const_cast<uint8_t *>(rbp.byte_debug_flags);
#endif

        if (!bp->r) {
            bp->r = g_unmapped_reads;
        }

        if (!bp->w) {
            bp->w = g_unmapped_writes;
        }
    }

#if BBCMICRO_DEBUGGER
    this->UpdateDebugState();
#endif

    this->UpdatePaging();
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// TODO: could the state pointer be m_state?
void BBCMicro::Write1770ControlRegister(void *m_, M6502Word a, uint8_t value) {
    auto m = (BBCMicro *)m_;
    (void)a;

    ASSERT(m->m_state.disc_interface);
    m->m_state.disc_control = m->m_state.disc_interface->GetControlFromByte(value);

#if BBCMICRO_TRACE
    if (m->m_trace) {
        m->m_trace->AllocStringf(TraceEventSource_Host, "1770 - Control Register: Reset=%d; DDEN=%d; drive=%d, side=%d\n",
                                 m->m_state.disc_control.reset, m->m_state.disc_control.dden, m->m_state.disc_control.drive, m->m_state.disc_control.side);
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

// TODO: could the state pointer be m_state?
uint8_t BBCMicro::Read1770ControlRegister(void *m_, M6502Word a) {
    auto m = (BBCMicro *)m_;
    (void)a;

    ASSERT(m->m_state.disc_interface);

    uint8_t value = m->m_state.disc_interface->GetByteFromControl(m->m_state.disc_control);
    return value;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if BBCMICRO_TRACE
void BBCMicro::TracePortB(BBCMicroState::SystemVIAPB pb) {
    Log log("", m_trace->GetLogPrinter(TraceEventSource_Host, 1000));

    log.f("PORTB - PB = $%02X (%%%s): ", pb.value, BINARY_BYTE_STRINGS[pb.value]);

    bool has_rtc = m_state.type->type_id == BBCMicroTypeID_Master;
    bool has_eeprom = m_state.type->type_id == BBCMicroTypeID_MasterCompact;

    if (has_rtc) {
        log.f("RTC AS=%u; RTC CS=%u; ", pb.m128_bits.rtc_address_strobe, pb.m128_bits.rtc_chip_select);
    } else if (has_eeprom) {
        log.f("EEPROM Clk=%u; EEPROM Data=%u; ", pb.mcompact_bits.clk, pb.mcompact_bits.data);
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
        if (has_rtc) {
            name = "RTC Read";
        } else if (has_eeprom) {
            name = "Bit 1";
        } else {
            name = "Speech Read";
        }
        goto print_bool;

    case 2:
        if (has_rtc) {
            name = "RTC DS";
        } else if (has_eeprom) {
            name = "Bit 2";
        } else {
            name = "Speech Write";
        }
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
    (void)a;

    auto m = (BBCMicro *)m_;
    (void)m;

    return 0; //m->m_state.cpu.dbus;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

uint8_t BBCMicro::ReadROMMMIO(void *m_, M6502Word a) {
    auto m = (BBCMicro *)m_;

    return m->m_big_pages[MOS_BIG_PAGE_INDEX.i + 3].r[a.p.o];
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

uint8_t BBCMicro::ReadROMSEL(void *m_, M6502Word a) {
    auto m = (BBCMicro *)m_;
    (void)a;

    return m->m_state.paging.romsel.value;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BBCMicro::WriteROMSEL(void *m_, M6502Word a, uint8_t value) {
    auto m = (BBCMicro *)m_;
    (void)a;

    if ((m->m_state.paging.romsel.value ^ value) & m->m_romsel_mask) {
        m->m_state.paging.romsel.value = value & m->m_romsel_mask;

        m->UpdatePaging();
        m->UpdateCPUDataBusFn();
        //(*m->m_update_romsel_pages_fn)(m);

#if BBCMICRO_TRACE
        if (m->m_trace) {
            m->m_trace->AllocWriteROMSELEvent(m->m_state.paging.romsel);
        }
#endif
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

uint8_t BBCMicro::ReadACCCON(void *m_, M6502Word a) {
    auto m = (BBCMicro *)m_;
    (void)a;

    return m->m_state.paging.acccon.value;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BBCMicro::WriteACCCON(void *m_, M6502Word a, uint8_t value) {
    auto m = (BBCMicro *)m_;
    (void)a;

    if ((m->m_state.paging.acccon.value ^ value) & m->m_acccon_mask) {
        m->m_state.paging.acccon.value = value & m->m_acccon_mask;
        m->UpdatePaging();

#if BBCMICRO_TRACE
        if (m->m_trace) {
            m->m_trace->AllocWriteACCCONEvent(m->m_state.paging.acccon);
        }
#endif
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

uint8_t BBCMicro::ReadADJI(void *m_, M6502Word a) {
    auto m = (BBCMicro *)m_;
    (void)a;

    return m->m_state.digital_joystick_state.value;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

BBCMicroTypeID BBCMicro::GetTypeID() const {
    return m_state.type->type_id;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

BBCMicroParasiteType BBCMicro::GetParasiteType() const {
    return m_state.parasite_type;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

const CycleCount *BBCMicro::GetCycleCountPtr() const {
    return &m_state.cycle_count;
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

            // If the parasite CPU is disabled,these calls are benign.
            if (new_state) {
                M6502_Halt(&m_state.cpu);
                M6502_Halt(&m_state.parasite_cpu);
            } else {
                M6502_Reset(&m_state.cpu);
                M6502_Reset(&m_state.parasite_cpu);
                ResetTube(&m_state.parasite_tube);
                m_state.parasite_boot_mode = true;
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

bool BBCMicro::GetJoystickButtonState(uint8_t index) const {
    ASSERT(index == 0 || index == 1);
    static_assert(BBCMicroState::SystemVIAPBBits::NOT_JOYSTICK1_FIRE_BIT == BBCMicroState::SystemVIAPBBits::NOT_JOYSTICK0_FIRE_BIT + 1, "");
    uint8_t mask = 1 << (BBCMicroState::SystemVIAPBBits::NOT_JOYSTICK1_FIRE_BIT + (index & 1));

    return !(m_state.not_joystick_buttons & mask);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BBCMicro::SetJoystickButtonState(uint8_t index, bool new_state) {
    uint8_t mask = 1 << (4 + (index & 1));

    if (new_state) {
        m_state.not_joystick_buttons &= ~mask;
    } else {
        m_state.not_joystick_buttons |= mask;
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool BBCMicro::HasNumericKeypad() const {
    return ::HasNumericKeypad(m_state.type->type_id);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if BBCMICRO_DEBUGGER
void BBCMicro::SetTeletextDebug(bool teletext_debug) {
    m_state.saa5050.debug = teletext_debug;
}
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BBCMicro::OptionalLowFrequencyUpdate() {
#if BBCMICRO_DEBUGGER
    this->UpdateUpdateMFnData();
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
                ASSERT(mem_big_pages->byte_debug_flags[i] == mem_big_pages->bp[i]->byte_debug_flags);
            } else {
                ASSERT(!mem_big_pages->byte_debug_flags[i]);
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
const uint8_t BBCMicro::CURSOR_PATTERNS[8] = {
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

void BBCMicro::SetDiscDriveSound(DiscDriveType type, DiscDriveSound sound, std::vector<float> samples) {
    ASSERT(sound >= 0 && sound < DiscDriveSound_EndValue);
    ASSERT(g_disc_drive_sounds[type][sound].empty());
    ASSERT(samples.size() <= INT_MAX);
    g_disc_drive_sounds[type][sound] = std::move(samples);
}

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
    switch (m_state.type->type_id) {
    case BBCMicroTypeID_Master:
        return m_state.rtc.GetRAMContents();

    case BBCMicroTypeID_MasterCompact:
        return std::vector<uint8_t>(m_state.eeprom.ram, m_state.eeprom.ram + sizeof m_state.eeprom.ram);

    default:
        return std::vector<uint8_t>();
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BBCMicro::SetOSROM(std::shared_ptr<const std::array<uint8_t, 16384>> data) {
    m_state.os_buffer = std::move(data);

    this->InitPaging();
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BBCMicro::SetSidewaysROM(uint8_t bank, std::shared_ptr<const std::vector<uint8_t>> data, ROMType type) {
    ASSERT(bank < 16);

    // No sideways RAM in this bank.
    m_state.sideways_ram_buffers[bank].reset();

    m_state.sideways_rom_buffers[bank] = std::move(data);
    m_state.paging.rom_types[bank] = type;
    m_state.paging.rom_regions[bank] = 0;

    this->InitPaging();
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BBCMicro::SetSidewaysRAM(uint8_t bank, std::shared_ptr<const std::vector<uint8_t>> data) {
    ASSERT(bank < 16);

    if (data) {
        m_state.sideways_ram_buffers[bank] = std::make_shared<std::array<uint8_t, 16384>>();
        for (size_t i = 0; i < std::min(data->size(), (size_t)16384); ++i) {
        }
    } else {
        m_state.sideways_ram_buffers[bank] = std::make_shared<std::array<uint8_t, 16384>>();
    }

    // No sideways ROM in this bank.
    m_state.sideways_rom_buffers[bank] = {};

    this->InitPaging();
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BBCMicro::SetParasiteOS(std::shared_ptr<const std::array<uint8_t, 2048>> data) {
    m_state.parasite_rom_buffer = std::move(data);

    this->InitPaging();
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if BBCMICRO_TRACE
void BBCMicro::StartTrace(uint32_t trace_flags, size_t max_num_bytes) {
    this->StopTrace(nullptr);

    bool parasite_boot_mode = false;
    const M6502Config *parasite_m6502_config = nullptr;
    if (m_state.parasite_type != BBCMicroParasiteType_None) {
        parasite_boot_mode = m_state.parasite_boot_mode;
        parasite_m6502_config = m_state.parasite_cpu.config;
    }

    this->SetTrace(std::make_shared<Trace>(max_num_bytes,
                                           m_state.type,
                                           m_state.paging,
                                           m_state.parasite_type,
                                           parasite_m6502_config,
                                           parasite_boot_mode),
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
            m_trace_current_instruction = nullptr;
        }

        if (m_trace_parasite_current_instruction) {
            m_trace->CancelEvent(INSTRUCTION_EVENT, m_trace_parasite_current_instruction);
            m_trace_parasite_current_instruction = nullptr;
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

void BBCMicro::AddHostInstructionFn(InstructionFn fn, void *context) {
    ASSERT(std::find(m_host_instruction_fns.begin(), m_host_instruction_fns.end(), std::make_pair(fn, context)) == m_host_instruction_fns.end());

    m_host_instruction_fns.emplace_back(fn, context);

    this->UpdateCPUDataBusFn();
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BBCMicro::RemoveHostInstructionFn(InstructionFn fn, void *context) {
    auto &&it = std::find(m_host_instruction_fns.begin(), m_host_instruction_fns.end(), std::make_pair(fn, context));

    if (it != m_host_instruction_fns.end()) {
        m_host_instruction_fns.erase(it);

        this->UpdateCPUDataBusFn();
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BBCMicro::AddHostWriteFn(WriteFn fn, void *context) {
    ASSERT(std::find(m_host_write_fns.begin(), m_host_write_fns.end(), std::make_pair(fn, context)) == m_host_write_fns.end());

    m_host_write_fns.emplace_back(fn, context);

    this->UpdateCPUDataBusFn();
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BBCMicro::SetSIO(uint16_t addr, ReadMMIOFn read_fn, void *read_context, WriteMMIOFn write_fn, void *write_context) {
    ASSERT(addr >= 0xfe00 && addr <= 0xfeff);
    this->SetMMIOFnsInternal(addr, read_fn, read_context, write_fn, write_context, true, true);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BBCMicro::SetXFJIO(uint16_t addr, ReadMMIOFn read_fn, void *read_context, WriteMMIOFn write_fn, void *write_context) {
    ASSERT(addr >= 0xfc00 && addr <= 0xfdff);
    this->SetMMIOFnsInternal(addr, read_fn, read_context, write_fn, write_context, true, false);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BBCMicro::SetIFJIO(uint16_t addr, ReadMMIOFn read_fn, void *read_context, WriteMMIOFn write_fn, void *write_context) {
    ASSERT(addr >= 0xfc00 && addr <= 0xfdff);
    this->SetMMIOFnsInternal(addr, read_fn, read_context, write_fn, write_context, false, true);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

std::shared_ptr<DiscImage> BBCMicro::TakeDiscImage(int drive) {
    if (drive >= 0 && drive < NUM_DRIVES) {
        std::shared_ptr<DiscImage> tmp = std::move(m_state.drives[drive].disc_image);
        return tmp;
    } else {
        return nullptr;
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

std::shared_ptr<const DiscImage> BBCMicro::GetDiscImage(int drive) const {
    if (drive >= 0 && drive < NUM_DRIVES) {
        return m_state.drives[drive].disc_image;
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

    BBCMicroState::DiscDrive *dd = &m_state.drives[drive];

    dd->disc_image = std::move(disc_image);
    dd->is_write_protected = false;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BBCMicro::SetDriveWriteProtected(int drive,
                                      bool is_write_protected) {
    ASSERT(drive >= 0 && drive < NUM_DRIVES);

    m_state.drives[drive].is_write_protected = is_write_protected;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool BBCMicro::IsDriveWriteProtected(int drive) const {
    ASSERT(drive >= 0 && drive < NUM_DRIVES);

    return m_state.drives[drive].is_write_protected;
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

    if (!text->empty()) {
        m_state.hack_flags |= BBCMicroHackFlag_Paste;
        m_state.paste_state = BBCMicroPasteState_Wait;
        m_state.paste_text = std::move(text);
        m_state.paste_index = 0;
        m_state.paste_wait_end = m_state.cycle_count.n + CYCLES_PER_SECOND;

        this->SetKeyState(PASTE_START_KEY, true);

        this->UpdateCPUDataBusFn();
    }
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

std::shared_ptr<const BBCMicroReadOnlyState> BBCMicro::DebugGetState() const {
    auto result = std::make_shared<BBCMicroReadOnlyState>(m_state);
    return result;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

const M6502 *BBCMicro::GetM6502() const {
    return &m_state.cpu;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if BBCMICRO_DEBUGGER
const BBCMicro::BigPage *BBCMicro::DebugGetBigPageForAddress(M6502Word addr,
                                                             bool mos,
                                                             uint32_t dso) const {
    BigPageIndex big_page;
    if (dso & BBCMicroDebugStateOverride_Parasite) {
        bool parasite_boot_mode = m_state.parasite_boot_mode;
        if (dso & BBCMicroDebugStateOverride_OverrideParasiteROM) {
            parasite_boot_mode = !!(dso & BBCMicroDebugStateOverride_ParasiteROM);
        }

        if (addr.w >= 0xf000 && parasite_boot_mode) {
            big_page = PARASITE_ROM_BIG_PAGE_INDEX;
        } else {
            big_page = {(BigPageIndex::Type)(PARASITE_BIG_PAGE_INDEX.i + addr.p.p)};
        }
    } else {
        PagingState paging = m_state.paging;
        (*m_state.type->apply_dso_fn)(&paging, dso);

        MemoryBigPageTables tables;
        uint32_t paging_flags;
        (*m_state.type->get_mem_big_page_tables_fn)(&tables, &paging_flags, paging);

        big_page = tables.mem_big_pages[mos][addr.p.p];
    }

    ASSERT(big_page.i < NUM_BIG_PAGES);
    const BigPage *bp = &m_big_pages[big_page.i];
    return bp;
}
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if BBCMICRO_DEBUGGER
void BBCMicro::DebugGetBigPageForAddress(ReadOnlyBigPage *bp,
                                         const BBCMicroState *state,
                                         const DebugState *debug_state,
                                         M6502Word addr,
                                         bool mos,
                                         uint32_t dso) {
    BigPageIndex index;
    if (dso & BBCMicroDebugStateOverride_Parasite) {
        bool boot_mode = state->parasite_boot_mode;
        if (dso & BBCMicroDebugStateOverride_OverrideParasiteROM) {
            boot_mode = !!(dso & BBCMicroDebugStateOverride_ParasiteROM);
        }

        if (addr.w >= 0xf000 && boot_mode) {
            index = PARASITE_ROM_BIG_PAGE_INDEX;
        } else {
            index = {(BigPageIndex::Type)(PARASITE_BIG_PAGE_INDEX.i + addr.p.p)};
        }
    } else {
        PagingState paging = state->paging;
        (*state->type->apply_dso_fn)(&paging, dso);

        MemoryBigPageTables tables;
        uint32_t paging_flags;
        (*state->type->get_mem_big_page_tables_fn)(&tables, &paging_flags, paging);

        index = tables.mem_big_pages[mos][addr.p.p];
    }

    InitReadOnlyBigPage(bp, state, debug_state, index);
}
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if BBCMICRO_DEBUGGER
void BBCMicro::DebugGetMemBigPageIsMOSTable(uint8_t *mem_big_page_is_mos, const BBCMicroState *state, uint32_t dso) {
    // Should maybe try to make this all fit together a bit better...
    if (dso & BBCMicroDebugStateOverride_Parasite) {
        memset(mem_big_page_is_mos, 0, 16);
    } else {
        PagingState paging = state->paging;
        (*state->type->apply_dso_fn)(&paging, dso);

        MemoryBigPageTables tables;
        uint32_t paging_flags;
        (*state->type->get_mem_big_page_tables_fn)(&tables, &paging_flags, paging);

        memcpy(mem_big_page_is_mos, tables.pc_mem_big_pages_set, 16);
    }
}
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if BBCMICRO_DEBUGGER
uint8_t BBCMicro::DebugGetByteDebugFlags(const BigPage *big_page,
                                         uint32_t offset) const {
    ASSERT(offset < BIG_PAGE_SIZE_BYTES);

    if (big_page->byte_debug_flags) {
        return big_page->byte_debug_flags[offset & BIG_PAGE_OFFSET_MASK];
    } else {
        return 0;
    }
}
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if BBCMICRO_DEBUGGER
void BBCMicro::DebugSetByteDebugFlags(BigPageIndex big_page_index,
                                      uint32_t offset,
                                      uint8_t flags) {
    ASSERT(big_page_index.i < NUM_BIG_PAGES);
    ASSERT(offset < BIG_PAGE_SIZE_BYTES);

    BigPage *big_page = &m_big_pages[big_page_index.i];
    if (big_page->byte_debug_flags) {
        uint8_t *byte_flags = &big_page->byte_debug_flags[offset & BIG_PAGE_OFFSET_MASK];

        if (*byte_flags != flags) {
            if (*byte_flags == 0) {
                ++m_debug->num_breakpoint_bytes;
            } else if (flags == 0) {
                ASSERT(m_debug->num_breakpoint_bytes > 0);
                --m_debug->num_breakpoint_bytes;
            }

            *byte_flags = flags;

            ++m_debug->breakpoints_changed_counter;

            if (flags & BBCMicroByteDebugFlag_TempBreakExecute) {
                m_debug->temp_execute_breakpoints.push_back(byte_flags);
            }
        }

        this->UpdateCPUDataBusFn();
    }
}
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if BBCMICRO_DEBUGGER
uint8_t BBCMicro::DebugGetAddressDebugFlags(M6502Word addr, uint32_t dso) const {
    if (m_debug) {
        if (dso & BBCMicroDebugStateOverride_Parasite) {
            return m_debug->parasite_address_debug_flags[addr.w];
        } else {
            return m_debug->host_address_debug_flags[addr.w];
        }
    } else {
        return 0;
    }
}
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if BBCMICRO_DEBUGGER
void BBCMicro::DebugSetAddressDebugFlags(M6502Word addr, uint32_t dso, uint8_t flags) {
    if (m_debug) {
        uint8_t *addr_flags;
        if (dso & BBCMicroDebugStateOverride_Parasite) {
            addr_flags = &m_debug->parasite_address_debug_flags[addr.w];
        } else {
            addr_flags = &m_debug->host_address_debug_flags[addr.w];
        }

        if (*addr_flags != flags) {
            if (*addr_flags == 0) {
                ++m_debug->num_breakpoint_bytes;
            } else if (flags == 0) {
                ASSERT(m_debug->num_breakpoint_bytes > 0);
                --m_debug->num_breakpoint_bytes;
            }

            *addr_flags = flags;

            ++m_debug->breakpoints_changed_counter;

            if (flags & BBCMicroByteDebugFlag_TempBreakExecute) {
                m_debug->temp_execute_breakpoints.push_back(addr_flags);
            }
        }

        this->UpdateCPUDataBusFn();
    }
}
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if BBCMICRO_DEBUGGER
void BBCMicro::DebugGetBytes(uint8_t *bytes, size_t num_bytes, M6502Word addr, uint32_t dso, bool mos) {
    // Not currently very clever.
    for (size_t i = 0; i < num_bytes; ++i) {
        const BigPage *bp = this->DebugGetBigPageForAddress(addr, mos, dso);

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
void BBCMicro::DebugSetBytes(M6502Word addr, uint32_t dso, bool mos, const uint8_t *bytes, size_t num_bytes) {
    // Not currently very clever.
    for (size_t i = 0; i < num_bytes; ++i) {
        const BigPage *bp = this->DebugGetBigPageForAddress(addr, mos, dso);

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
        m_debug->is_halted = m_debug_is_halted = true;

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
                ASSERT(((uintptr_t)flags >= (uintptr_t)m_debug->big_pages_byte_debug_flags &&
                        (uintptr_t)flags < (uintptr_t)((char *)m_debug->big_pages_byte_debug_flags + sizeof m_debug->big_pages_byte_debug_flags)) ||
                       ((uintptr_t)flags >= (uintptr_t)m_debug->host_address_debug_flags &&
                        (uintptr_t)flags < (uintptr_t)((char *)m_debug->host_address_debug_flags + sizeof m_debug->host_address_debug_flags)) ||
                       ((uintptr_t)flags >= (uintptr_t)m_debug->parasite_address_debug_flags &&
                        (uintptr_t)flags < (uintptr_t)((char *)m_debug->parasite_address_debug_flags + sizeof m_debug->parasite_address_debug_flags)));

                uint8_t old = *flags;
                *flags &= (uint8_t)~BBCMicroByteDebugFlag_TempBreakExecute;
                if (old != 0 && *flags == 0) {
                    ASSERT(m_debug->num_breakpoint_bytes > 0);
                    --m_debug->num_breakpoint_bytes;
                }
            }

            m_debug->temp_execute_breakpoints.clear();

            ++m_debug->breakpoints_changed_counter;

            this->UpdateCPUDataBusFn();
        }

        this->SetDebugStepType(BBCMicroStepType_None, nullptr);
    }
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
    m_debug->is_halted = m_debug_is_halted = false;
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
void BBCMicro::DebugStepOver(uint32_t dso) {
    if (!m_debug) {
        return;
    }

    const M6502 *s = m_state.DebugGetM6502(dso);
    if (!s) {
        return;
    }

    uint8_t opcode = M6502_GetOpcode(s);
    const M6502DisassemblyInfo *di = &s->config->disassembly_info[opcode];

    if (di->always_step_in) {
        this->DebugStepIn(dso);
    } else {
        // More work than required here - but it's not a massive problem, just
        // a bit ugly :(
        uint8_t pc_is_mos[16];
        this->DebugGetMemBigPageIsMOSTable(pc_is_mos, &m_state, dso);

        // Try to put a breakpoint on the actual next instruction, rather than
        // its address.
        M6502Word next_pc = {(uint16_t)(s->opcode_pc.w + di->num_bytes)};
        const BBCMicro::BigPage *big_page = this->DebugGetBigPageForAddress(next_pc,
                                                                            !!pc_is_mos[s->pc.p.p],
                                                                            dso | DebugGetCurrentStateOverride(&m_state));

        uint8_t flags = this->DebugGetByteDebugFlags(big_page, next_pc.p.o);
        flags |= BBCMicroByteDebugFlag_TempBreakExecute;
        this->DebugSetByteDebugFlags(big_page->index, next_pc.p.o, flags);
    }
}
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if BBCMICRO_DEBUGGER
void BBCMicro::DebugStepIn(uint32_t dso) {
    if (!m_debug) {
        return;
    }

    const M6502 *cpu = m_state.DebugGetM6502(dso);
    this->SetDebugStepType(BBCMicroStepType_StepIn, cpu);
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
std::shared_ptr<BBCMicro::DebugState> BBCMicro::TakeDebugState() {
    std::shared_ptr<BBCMicro::DebugState> debug = std::move(m_debug_ptr);

    m_debug = nullptr;

    this->UpdateDebugState();

    return debug;
}
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if BBCMICRO_DEBUGGER
std::shared_ptr<const BBCMicro::DebugState> BBCMicro::GetDebugState() const {
    return m_debug_ptr;
}
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if BBCMICRO_DEBUGGER
void BBCMicro::SetDebugState(std::shared_ptr<DebugState> debug) {
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
uint32_t BBCMicro::DebugGetCurrentStateOverride(const BBCMicroState *state) {
    uint32_t dso = (state->type->get_dso_fn)(state->paging);

    if (state->parasite_type != BBCMicroParasiteType_None) {
        if (state->parasite_boot_mode) {
            dso |= BBCMicroDebugStateOverride_ParasiteROM;
        }
    }

    return dso;
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
void BBCMicro::DebugResetRelativeCycleBase(uint32_t dso) {
    if (m_debug) {
        DebugState::RelativeCycleCountBase DebugState::*base_mptr = DebugGetRelativeCycleCountBaseMPtr(m_state, dso);
        if (base_mptr) {
            DebugState::RelativeCycleCountBase *base = &(m_debug->*base_mptr);

            base->prev = m_state.cycle_count;
            base->recent = m_state.cycle_count;
        }
    }
}
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if BBCMICRO_DEBUGGER
void BBCMicro::DebugToggleResetRelativeCycleBaseOnBreakpoint(uint32_t dso) {
    if (m_debug) {
        DebugState::RelativeCycleCountBase DebugState::*base_mptr = DebugGetRelativeCycleCountBaseMPtr(m_state, dso);
        if (base_mptr) {
            DebugState::RelativeCycleCountBase *base = &(m_debug->*base_mptr);

            base->reset_on_breakpoint = !base->reset_on_breakpoint;
        }
    }
}
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if BBCMICRO_DEBUGGER
BBCMicro::DebugState::RelativeCycleCountBase BBCMicro::DebugState::*BBCMicro::DebugGetRelativeCycleCountBaseMPtr(const BBCMicroState &state, uint32_t dso) {
    if (dso & BBCMicroDebugStateOverride_Parasite) {
        if (state.parasite_type != BBCMicroParasiteType_None) {
            return &DebugState::parasite_relative_base;
        }
    } else {
        return &DebugState::host_relative_base;
    }

    return nullptr;
}
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BBCMicro::SendBeebLinkResponse(std::vector<uint8_t> data) {
    if (!m_beeblink) {
        // Just discard the response. The request is now outdated.
    } else {
        m_beeblink->SendResponse(std::move(data));
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

std::string BBCMicro::GetUpdateFlagExpr(const uint32_t flags_) {
    std::string expr;

    // ROMType is dealt with separately.
    uint32_t flags = flags_ & ~(BBCMicroUpdateFlag_ROMTypeMask << BBCMicroUpdateFlag_ROMTypeShift);
    uint32_t mask = 1;
    while (flags != 0) {
        if (flags & mask) {
            if (!expr.empty()) {
                expr += "|";
            }

            const char *name = GetBBCMicroUpdateFlagEnumName(mask);
            if (name[0] == '?') {
                char tmp[100];
                snprintf(tmp, sizeof tmp, "0x%" PRIx32, mask);
                expr += tmp;
            } else {
                expr += name;
            }
        }
        flags &= ~mask;
        mask <<= 1;
    }

    ROMType type = (ROMType)(flags_ >> BBCMicroUpdateFlag_ROMTypeShift & BBCMicroUpdateFlag_ROMTypeMask);
    if (type != ROMType_16KB) {
        const char *type_name = GetROMTypeEnumName(type);
        if (type_name[0] == '?') {
            expr += "(ROMType)" + std::to_string((int)type);
        } else {
            expr += type_name;
        }
        expr += "<<ROMTypeShift";
    }

    if (expr.empty()) {
        expr = "0";
    } else {
        char tmp[100];
        snprintf(tmp, sizeof tmp, " (0x%" PRIx32 ")", flags_);
        expr += tmp;
    }

    return expr;
}

template <>
struct std::hash<BBCMicro::UpdateMFn> {
    uint64_t operator()(const BBCMicro::UpdateMFn &mfn) const {
        uint8_t mfn_data[sizeof mfn];
        memcpy(mfn_data, &mfn, sizeof mfn);

        unsigned char digest[SHA1::DIGEST_SIZE];
        SHA1::HashBuffer(digest, nullptr, mfn_data, sizeof mfn_data);

        uint64_t result;
        memcpy(&result, digest, sizeof(uint64_t));

        return result;
    }
};

static size_t LogNumUniqueInstantiations(Log *log, const char *prefix, const BBCMicro::UpdateMFn *mfns, size_t num_mfns, const size_t *num_unique_overall) {
    std::unordered_set<BBCMicro::UpdateMFn> update_mfns;
    for (size_t i = 0; i < num_mfns; ++i) {
        update_mfns.insert(mfns[i]);
    }

    log->f("%s: %zu/%zu unique BBCMicro::UpdateTemplate instantiations", prefix, update_mfns.size(), num_mfns);
    if (num_unique_overall) {
        log->f(" (%.2fx ideal)", (double)update_mfns.size() * BBCMICRO_NUM_UPDATE_GROUPS / *num_unique_overall);
    }
    log->f("\n");

    return update_mfns.size();
}

void BBCMicro::PrintInfo(Log *log) {
    EnsureUpdateMFnsTableIsReady();

    size_t num_update_mfns = sizeof ms_update_mfns / sizeof ms_update_mfns[0];

    std::set<uint32_t> normalized_flags;
    for (uint32_t i = 0; i < num_update_mfns; ++i) {
        normalized_flags.insert(GetNormalizedBBCMicroUpdateFlags(i));
    }

    log->f("%zu/%zu normalized BBCMicroUpdateFlag combinations\n", normalized_flags.size(), num_update_mfns);

    size_t num_unique_overall = LogNumUniqueInstantiations(log, "ms_update_mfns", ms_update_mfns, sizeof ms_update_mfns / sizeof ms_update_mfns[0], nullptr);
#if BBCMICRO_NUM_UPDATE_GROUPS > 1
    LogNumUniqueInstantiations(log, "ms_update_mfns0", ms_update_mfns0, sizeof ms_update_mfns0 / sizeof ms_update_mfns0[0], &num_unique_overall);
    LogNumUniqueInstantiations(log, "ms_update_mfns1", ms_update_mfns1, sizeof ms_update_mfns1 / sizeof ms_update_mfns1[0], &num_unique_overall);
#endif
#if BBCMICRO_NUM_UPDATE_GROUPS > 2
    LogNumUniqueInstantiations(log, "ms_update_mfns2", ms_update_mfns2, sizeof ms_update_mfns2 / sizeof ms_update_mfns2[0], &num_unique_overall);
    LogNumUniqueInstantiations(log, "ms_update_mfns3", ms_update_mfns3, sizeof ms_update_mfns3 / sizeof ms_update_mfns3[0], &num_unique_overall);
#endif

    uint32_t unused_bits = ~(uint32_t)0;
    for (uint32_t bit = 0; bit < 32; ++bit) {
        uint32_t mask = 1 << bit;
        if (mask >= num_update_mfns) {
            unused_bits &= mask - 1;
            break;
        }
        for (uint32_t i = 0; i < num_update_mfns; ++i) {
            if (ms_update_mfns[i] != ms_update_mfns[i | mask]) {
                unused_bits &= ~mask;
            }
        }
    }

    if (unused_bits != 0) {
        log->f("unused BBCMicroUpdateFlag values: %s\n", GetUpdateFlagExpr(unused_bits).c_str());
    }

    log->f("sizeof(BBCMicro): %zu\n", sizeof(BBCMicro));
    log->f("sizeof(BBCMicroState): %zu\n", sizeof(BBCMicroState));
    log->f("sizeof BBCMicro::ms_update_mfns: %zu\n", sizeof ms_update_mfns);
    log->f("sizeof BBCMicro::ms_update_mfns[0]: %zu\n", sizeof ms_update_mfns[0]);
    log->f("sizeof(BBCMicro::UpdateMFn): %zu\n", sizeof(UpdateMFn));
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BBCMicro::SetPrinterEnabled(bool printer_enabled) {
    if (printer_enabled != m_state.printer_enabled) {
        m_state.printer_enabled = printer_enabled;
        if (m_state.printer_enabled) {
            // Ensure there's a CA1 blip so the OS knows a printer is attached.
            m_state.printer_busy_counter = 2;
        } else {
            m_state.printer_busy_counter = 0;
        }
        this->UpdateCPUDataBusFn();
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BBCMicro::SetPrinterBuffer(std::vector<uint8_t> *buffer) {
    m_printer_buffer = buffer;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool BBCMicro::HasADC() const {
    return m_state.type->adc_addr != 0;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

uint32_t BBCMicro::GetUpdateFlags() const {
    return m_update_flags;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if BBCMICRO_DEBUGGER
std::shared_ptr<const BBCMicro::UpdateMFnData> BBCMicro::GetUpdateMFnData() const {
    return m_update_mfn_data_ptr;
}
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BBCMicro::AddMouseMotion(int dx, int dy) {
    if (!(m_update_flags & BBCMicroUpdateFlag_Mouse)) {
        return;
    }

    m_state.mouse_dx += dx;
    m_state.mouse_dy += dy;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BBCMicro::SetMouseButtons(uint8_t mask, uint8_t value) {
    if (!(m_update_flags & BBCMicroUpdateFlag_Mouse)) {
        return;
    }

    if (m_update_flags & BBCMicroUpdateFlag_IsMasterCompact) {
        if (mask & BBCMicroMouseButton_Left) {
            m_state.mouse_data.compact_bits.l = !(value & BBCMicroMouseButton_Left);
        }

        if (mask & BBCMicroMouseButton_Middle) {
            m_state.mouse_data.compact_bits.m = !(value & BBCMicroMouseButton_Middle);
        }

        if (mask & BBCMicroMouseButton_Right) {
            m_state.mouse_data.compact_bits.r = !(value & BBCMicroMouseButton_Right);
        }
    } else {
        if (mask & BBCMicroMouseButton_Left) {
            m_state.mouse_data.amx_bits.l = !(value & BBCMicroMouseButton_Left);
        }

        if (mask & BBCMicroMouseButton_Middle) {
            m_state.mouse_data.amx_bits.m = !(value & BBCMicroMouseButton_Middle);
        }

        if (mask & BBCMicroMouseButton_Right) {
            m_state.mouse_data.amx_bits.r = !(value & BBCMicroMouseButton_Right);
        }
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BBCMicro::TestSetByte(uint16_t ram_buffer_index, uint8_t value) {
    ASSERT(ram_buffer_index < m_state.ram_buffer->size());
    m_state.ram_buffer->at(ram_buffer_index) = value;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BBCMicro::TestSetParasiteByte(uint16_t addr, uint8_t value) {
    ASSERT(!!m_state.parasite_ram_buffer);
    ASSERT(addr < m_state.parasite_ram_buffer->size());
    m_state.parasite_ram_buffer->at(addr) = value;
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
        mem_big_pages->byte_debug_flags[i] = mem_big_pages->bp[i]->byte_debug_flags;
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

        bp->byte_debug_flags = nullptr;
        bp->address_debug_flags = nullptr;

        if (m_debug) {
            const BigPageMetadata *metadata = &m_state.type->big_pages_metadata[i];
            if (metadata->addr != 0xffff) {
                bp->byte_debug_flags = m_debug->big_pages_byte_debug_flags[bp->index.i];

                if (metadata->is_parasite) {
                    bp->address_debug_flags = &m_debug->parasite_address_debug_flags[metadata->addr];
                } else {
                    bp->address_debug_flags = &m_debug->host_address_debug_flags[metadata->addr];
                }
            }
        }
    }

    for (size_t i = 0; i < 2; ++i) {
        MemoryBigPages *mbp = &m_mem_big_pages[i];

        for (size_t j = 0; j < 16; ++j) {
            mbp->byte_debug_flags[j] = mbp->bp[j] ? mbp->bp[j]->byte_debug_flags : nullptr;
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
void BBCMicro::SetDebugStepType(BBCMicroStepType step_type, const M6502 *step_cpu) {
    ASSERT(step_type >= 0 && step_type < BBCMicroStepType_Count);
    ASSERT((step_type == BBCMicroStepType_None && !step_cpu) || (step_type != BBCMicroStepType_None && step_cpu));

    if (m_debug) {
        // changes in CPU pointer won't affect the update function.
        if (m_debug->step_type != step_type) {
            m_debug->step_type = step_type;
            m_debug->step_cpu = step_cpu;
            this->UpdateCPUDataBusFn();
        }
    }
}
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if BBCMICRO_DEBUGGER
void BBCMicro::DebugHitBreakpoint(const M6502 *cpu, BBCMicro::DebugState::RelativeCycleCountBase *base, uint8_t flags) {
    auto metadata = (const M6502Metadata *)cpu->context;
    bool maybe_update_base = false;

    if (cpu->read == 0) {
        if (flags & BBCMicroByteDebugFlag_BreakWrite) {
            maybe_update_base = true;
            DebugHalt("%s data write: $%04x", metadata->name, m_state.cpu.abus.w);
        }
    } else {
        if (flags & BBCMicroByteDebugFlag_TempBreakExecute) {
            if (cpu->read == M6502ReadType_Opcode) {
                this->DebugHalt("%s single step", metadata->name);
            }
        } else if (flags & BBCMicroByteDebugFlag_BreakExecute) {
            if (cpu->read == M6502ReadType_Opcode) {
                // Only update the hit cycle count when not stepping.
                if (m_debug->step_type == BBCMicroStepType_None) {
                    maybe_update_base = true;
                }

                this->DebugHalt("%s execute: $%04x", metadata->name, m_state.cpu.abus.w);
            }
        }

        if (flags & BBCMicroByteDebugFlag_BreakRead) {
            if (cpu->read <= M6502ReadType_LastInterestingDataRead) {
                maybe_update_base = true;
                this->DebugHalt("%s data read: $%04x", metadata->name, m_state.cpu.abus.w);
            }
        }
    }

    if (maybe_update_base) {
        if (base->reset_on_breakpoint) {
            base->prev = base->recent;
            base->recent = m_state.cycle_count;
        }
    }
}
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if BBCMICRO_DEBUGGER
void BBCMicro::DebugHandleStep() {

    switch (m_debug->step_type) {
    default:
        ASSERT(false);
        // fall through
    case BBCMicroStepType_None:
        // It's valid to end up here with no step type: the flags and
        // m_update_mfn might change, but the current update function continues
        // to run. Just do nothing in this case.
        break;

    case BBCMicroStepType_StepIn:
        {
            ASSERT(m_debug->step_cpu);
            auto metadata = (const M6502Metadata *)m_debug->step_cpu->context;

            if (m_debug->step_cpu->read == M6502ReadType_Opcode) {
                // Done.
                this->DebugHalt("%s single step", metadata->name);
            } else if (m_debug->step_cpu->read == M6502ReadType_Interrupt) {
                // The instruction was interrupted, so set a temp
                // breakpoint in the right place.
                uint8_t flags = this->DebugGetAddressDebugFlags(m_debug->step_cpu->pc, metadata->dso);

                flags |= BBCMicroByteDebugFlag_TempBreakExecute;

                this->DebugSetAddressDebugFlags(m_debug->step_cpu->pc, metadata->dso, flags);
                this->SetDebugStepType(BBCMicroStepType_None, nullptr);
            }
        }
        break;

    case BBCMicroStepType_StepIntoIRQHandler:
        {
            ASSERT(m_debug->step_cpu);
            auto metadata = (const M6502Metadata *)m_debug->step_cpu->context;

            ASSERT(m_debug->step_cpu->read == M6502ReadType_Opcode || m_debug->step_cpu->read == M6502ReadType_Interrupt);
            if (m_debug->step_cpu->read == M6502ReadType_Opcode) {
                this->DebugHalt("%s IRQ/NMI", metadata->name);
            }
        }
        break;
    }
}
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BBCMicro::InitStuff() {
    CHECK_SIZEOF(BBCMicroState::AddressableLatch, 1);
    CHECK_SIZEOF(ROMSEL, 1);
    CHECK_SIZEOF(ACCCON, 1);
    CHECK_SIZEOF(BBCMicroState::SystemVIAPB, 1);

    EnsureUpdateMFnsTableIsReady();

#if BBCMICRO_DEBUGGER
    ASSERT(!m_update_mfn_data_ptr);
    m_update_mfn_data_ptr = std::make_shared<UpdateMFnData>();
    m_update_mfn_data = m_update_mfn_data_ptr.get();
    m_last_mfn_change_cycle_count = m_state.cycle_count;
#endif

    m_ram = m_state.ram_buffer->data();

#if PCD8572_MOS510_DEBUG
    m_state.eeprom.cpu = &m_state.cpu;
#endif

    m_read_mmios_hw = std::vector<ReadMMIO>(768);
    m_write_mmios_hw = std::vector<WriteMMIO>(768);
    m_mmios_stretch_hw = std::vector<uint8_t>(768);

    m_read_mmios_hw_cartridge = std::vector<ReadMMIO>(768);
    m_write_mmios_hw_cartridge = std::vector<WriteMMIO>(768);
    m_mmios_stretch_hw_cartridge = std::vector<uint8_t>(768);

    // Assume hardware is mapped. It will get fixed up later if
    // not.
    //m_read_mmios = m_read_mmios_hw.data();
    //m_mmios_stretch = m_mmios_stretch_hw.data();
    //m_rom_mmio = false;

    // initially no I/O
    for (uint16_t i = 0xfc00; i < 0xff00; ++i) {
        this->SetMMIOFnsInternal(i, nullptr, nullptr, nullptr, nullptr, true, true);
    }

    if (m_state.init_flags & BBCMicroInitFlag_ExtMem) {
        m_state.ext_mem.AllocateBuffer();

        this->SetXFJIO(0xfc00, nullptr, nullptr, &ExtMem::WriteAddressL, &m_state.ext_mem);
        this->SetXFJIO(0xfc01, nullptr, nullptr, &ExtMem::WriteAddressH, &m_state.ext_mem);
        this->SetXFJIO(0xfc02, &ExtMem::ReadAddressL, &m_state.ext_mem, nullptr, nullptr);
        this->SetXFJIO(0xfc03, &ExtMem::ReadAddressH, &m_state.ext_mem, nullptr, nullptr);

        for (uint16_t i = 0xfd00; i <= 0xfdff; ++i) {
            this->SetXFJIO(i, &ExtMem::ReadData, &m_state.ext_mem, &ExtMem::WriteData, &m_state.ext_mem);
        }
    }

    // I/O: VIAs
    for (uint16_t i = 0; i < 32; ++i) {
        this->SetSIO(0xfe40 + i, g_R6522_read_fns[i & 15], &m_state.system_via, g_R6522_write_fns[i & 15], &m_state.system_via);
        this->SetSIO(0xfe60 + i, g_R6522_read_fns[i & 15], &m_state.user_via, g_R6522_write_fns[i & 15], &m_state.user_via);
    }

    // I/O: 6845
    for (int i = 0; i < 8; i += 2) {
        this->SetSIO((uint16_t)(0xfe00 + i + 0), &CRTC::ReadAddress, &m_state.crtc, &CRTC::WriteAddress, &m_state.crtc);
        this->SetSIO((uint16_t)(0xfe00 + i + 1), &CRTC::ReadData, &m_state.crtc, &CRTC::WriteData, &m_state.crtc);
    }

    // I/O: Video ULA
    m_state.video_ula.nula = !!(m_state.init_flags & BBCMicroInitFlag_VideoNuLA);
    for (int i = 0; i < 2; ++i) {
        this->SetSIO((uint16_t)(0xfe20 + i * 2), nullptr, nullptr, &VideoULA::WriteControlRegister, &m_state.video_ula);
        this->SetSIO((uint16_t)(0xfe21 + i * 2), nullptr, nullptr, &VideoULA::WritePalette, &m_state.video_ula);
    }

    if (m_state.init_flags & BBCMicroInitFlag_VideoNuLA) {
        this->SetSIO(0xfe22, nullptr, nullptr, &VideoULA::WriteNuLAControlRegister, &m_state.video_ula);
        this->SetSIO(0xfe23, nullptr, nullptr, &VideoULA::WriteNuLAPalette, &m_state.video_ula);
    }

    // I/O: disc interface
    if (m_state.disc_interface) {
        m_state.fdc.SetHandler(this);
        m_state.fdc.SetNoINTRQ(!!(m_state.disc_interface->flags & DiscInterfaceFlag_NoINTRQ));
        m_state.fdc.Set1772(!!(m_state.disc_interface->flags & DiscInterfaceFlag_1772));

        M6502Word c = {m_state.disc_interface->control_addr};
        c.b.h -= 0xfc;
        ASSERT(c.b.h < 3);

        M6502Word f = {m_state.disc_interface->fdc_addr};
        f.b.h -= 0xfc;
        ASSERT(f.b.h < 3);

        // Slightly ugly code gonig straight to the internal function, to work
        // around Challenger FDC being in the external 1 MHz bus area.
        for (int i = 0; i < 4; ++i) {
            uint16_t addr = (uint16_t)(m_state.disc_interface->fdc_addr + i);

            this->SetMMIOFnsInternal(addr, g_WD1770_read_fns[i], &m_state.fdc, g_WD1770_write_fns[i], &m_state.fdc, true, false);
        }

        this->SetMMIOFnsInternal(m_state.disc_interface->control_addr, &Read1770ControlRegister, this, &Write1770ControlRegister, this, true, false);

        m_state.disc_interface->InstallExtraHardware(this, m_state.disc_interface_extra_hardware);
    } else {
        m_state.fdc.SetHandler(nullptr);
    }

    m_state.system_via.SetID(BBCMicroVIAID_SystemVIA, "SystemVIA");
    m_state.user_via.SetID(BBCMicroVIAID_UserVIA, "UserVIA");

    m_state.old_system_via_pb.value = m_state.system_via.b.p;

    if (m_beeblink_handler) {
        m_beeblink = std::make_unique<BeebLink>(m_beeblink_handler);

        this->SetSIO(0xfe9e, &BeebLink::ReadControl, m_beeblink.get(), &BeebLink::WriteControl, m_beeblink.get());
        this->SetSIO(0xfe9f, &BeebLink::ReadData, m_beeblink.get(), &BeebLink::WriteData, m_beeblink.get());
    }

    this->UpdateCPUDataBusFn();

    m_romsel_mask = m_state.type->romsel_mask;
    m_acccon_mask = m_state.type->acccon_mask;

    if (CanDisplayTeletextAt3C00(m_state.type->type_id)) {
        m_teletext_bases[0] = 0x3c00;
        m_teletext_bases[1] = 0x7c00;
    } else {
        m_teletext_bases[0] = 0x7c00;
        m_teletext_bases[1] = 0x7c00;
    }

    if (m_acccon_mask == 0) {
        for (uint16_t i = 0; i < 16; ++i) {
            this->SetSIO((uint16_t)(0xfe30 + i), &ReadROMSEL, this, &WriteROMSEL, this);
        }
    } else {
        for (uint16_t i = 0; i < 4; ++i) {
            this->SetSIO((uint16_t)(0xfe30 + i), &ReadROMSEL, this, &WriteROMSEL, this);
            this->SetSIO((uint16_t)(0xfe34 + i), &ReadACCCON, this, &WriteACCCON, this);
        }
    }

    //
    if (m_state.type->adc_addr != 0) {
        ASSERT(m_state.type->adc_count % 4 == 0);
        for (unsigned i = 0; i < m_state.type->adc_count; i += 4) {
            this->SetSIO((uint16_t)(m_state.type->adc_addr + i + 0u), &ADC::Read0, &m_state.adc, &ADC::Write0, &m_state.adc);
            this->SetSIO((uint16_t)(m_state.type->adc_addr + i + 1u), &ADC::Read1, &m_state.adc, &ADC::Write1, &m_state.adc);
            this->SetSIO((uint16_t)(m_state.type->adc_addr + i + 2u), &ADC::Read2, &m_state.adc, &ADC::Write2, &m_state.adc);
            this->SetSIO((uint16_t)(m_state.type->adc_addr + i + 3u), &ADC::Read3, &m_state.adc, &ADC::Write3, &m_state.adc);
        }
    }

    if (m_state.init_flags & BBCMicroInitFlag_ADJI) {
        uint8_t adji_addr = m_state.init_flags >> BBCMicroInitFlag_ADJIDIPSwitchesShift & 3;
        this->SetIFJIO(ADJI_ADDRESSES[adji_addr], &ReadADJI, this, nullptr, nullptr);
    }

    // Set up TST=1 tables.
    m_read_mmios_rom = std::vector<ReadMMIO>(768, {&ReadROMMMIO, this});
    m_mmios_stretch_rom = std::vector<uint8_t>(768, 0x00);

    // FRED = external stretched, internal not
    for (size_t i = 0; i < 0x100; ++i) {
        m_mmios_stretch_hw[i] = 0xff;
        m_mmios_stretch_hw_cartridge[i] = 0x00;
    }

    // JIM = external stretched, internal not
    for (size_t i = 0x100; i < 0x200; ++i) {
        m_mmios_stretch_hw[i] = 0xff;
        m_mmios_stretch_hw_cartridge[i] = 0x00;
    }

    // SHEILA = part stretched
    for (size_t i = 0x200; i < 0x300; ++i) {
        m_mmios_stretch_hw[i] = 0x00;
    }

    for (const BBCMicroType::SHEILACycleStretchRegion &region : m_state.type->sheila_cycle_stretch_regions) {
        ASSERT(region.first < region.last);
        for (uint8_t i = region.first; i <= region.last; ++i) {
            m_mmios_stretch_hw[0x200u + i] = 0xff;
        }
    }

    // Copy SHEILA stretching flags for the cartridge copy.
    for (size_t i = 0x200; i < 0x300; ++i) {
        m_mmios_stretch_hw_cartridge[i] = m_mmios_stretch_hw[i];
    }

    if (m_state.parasite_type != BBCMicroParasiteType_None) {
        m_state.parasite_cpu.context = this;

        ASSERT(!!m_state.parasite_ram_buffer);
        ASSERT(m_state.parasite_ram_buffer->size() == 65536);
        m_parasite_ram = m_state.parasite_ram_buffer->data();

        m_parasite_read_mmio_fns[0] = &ReadParasiteTube0;
        m_parasite_read_mmio_fns[1] = &ReadParasiteTube1;
        m_parasite_read_mmio_fns[2] = &ReadParasiteTube2;
        m_parasite_read_mmio_fns[3] = &ReadParasiteTube3;
        m_parasite_read_mmio_fns[4] = &ReadParasiteTube4;
        m_parasite_read_mmio_fns[5] = &ReadParasiteTube5;
        m_parasite_read_mmio_fns[6] = &ReadParasiteTube6;
        m_parasite_read_mmio_fns[7] = &ReadParasiteTube7;

        m_parasite_write_mmio_fns[0] = &WriteTubeDummy;
        m_parasite_write_mmio_fns[1] = &WriteParasiteTube1;
        m_parasite_write_mmio_fns[2] = &WriteTubeDummy;
        m_parasite_write_mmio_fns[3] = &WriteParasiteTube3;
        m_parasite_write_mmio_fns[4] = &WriteTubeDummy;
        m_parasite_write_mmio_fns[5] = &WriteParasiteTube5;
        m_parasite_write_mmio_fns[6] = &WriteTubeDummy;
        m_parasite_write_mmio_fns[7] = &WriteParasiteTube7;
    } else {
        ASSERT(!m_state.parasite_ram_buffer);
    }

    m_host_cpu_metadata.name = "host";
#if BBCMICRO_DEBUGGER
    m_host_cpu_metadata.dso = 0;
    m_host_cpu_metadata.debug_step_update_flag = BBCMicroUpdateFlag_DebugStepHost;
#endif
    m_state.cpu.context = &m_host_cpu_metadata;

    m_parasite_cpu_metadata.name = "parasite";
#if BBCMICRO_DEBUGGER
    m_parasite_cpu_metadata.dso = 0;
    m_parasite_cpu_metadata.debug_step_update_flag = BBCMicroUpdateFlag_DebugStepParasite;
#endif
    m_state.parasite_cpu.context = &m_parasite_cpu_metadata;

    m_state.adc.SetHandler(&ReadAnalogueChannel, this);

    // Page in current ROM bank and sort out ACCCON.
    this->InitPaging();

    this->InitDiscDriveSounds(m_state.type->default_disc_drive_type);

#if BBCMICRO_TRACE
    this->SetTrace(nullptr, 0);
#endif
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool BBCMicro::IsTrack0() {
    if (BBCMicroState::DiscDrive *dd = this->GetDiscDrive()) {
        return dd->track == 0;
    }

    return false;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BBCMicro::StepOut() {
    if (BBCMicroState::DiscDrive *dd = this->GetDiscDrive()) {
        if (dd->track > 0) {
            --dd->track;

            this->StepSound(dd);
        }
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BBCMicro::StepIn() {
    if (BBCMicroState::DiscDrive *dd = this->GetDiscDrive()) {
        if (dd->track < 255) {
            ++dd->track;

            this->StepSound(dd);
        }
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BBCMicro::SpinUp() {
    if (BBCMicroState::DiscDrive *dd = this->GetDiscDrive()) {
        dd->motor = true;

        dd->spin_sound_index = 0;
        dd->spin_sound = DiscDriveSound_SpinStartLoaded;
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BBCMicro::SpinDown() {
    if (BBCMicroState::DiscDrive *dd = this->GetDiscDrive()) {
        dd->motor = false;

        dd->spin_sound_index = 0;
        dd->spin_sound = DiscDriveSound_SpinEnd;

        if (dd->disc_image) {
            dd->disc_image->Flush();
        }
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool BBCMicro::IsWriteProtected() {
    if (BBCMicroState::DiscDrive *dd = this->GetDiscDrive()) {
        if (dd->disc_image) {
            if (dd->disc_image->IsWriteProtected()) {
                return true;
            }

            if (dd->is_write_protected) {
                return true;
            }
        }
    }

    return false;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool BBCMicro::GetByte(uint8_t *value, uint8_t sector, size_t offset) {
    if (BBCMicroState::DiscDrive *dd = this->GetDiscDrive()) {
        m_disc_access = true;

        if (dd->disc_image) {
            if (dd->disc_image->Read(value, m_state.disc_control.side, dd->track, sector, offset)) {
                return true;
            }
        }
    }

    return false;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool BBCMicro::SetByte(uint8_t sector, size_t offset, uint8_t value) {
    if (BBCMicroState::DiscDrive *dd = this->GetDiscDrive()) {
        m_disc_access = true;

        if (dd->disc_image) {
            if (dd->disc_image->Write(m_state.disc_control.side, dd->track, sector, offset, value)) {
                return true;
            }
        }
    }

    return false;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool BBCMicro::GetSectorDetails(uint8_t *track, uint8_t *side, size_t *size, uint8_t sector, bool double_density) {
    if (BBCMicroState::DiscDrive *dd = this->GetDiscDrive()) {
        m_disc_access = true;

        if (dd->disc_image) {
            if (dd->disc_image->GetDiscSectorSize(size, m_state.disc_control.side, dd->track, sector, double_density)) {
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

BBCMicroState::DiscDrive *BBCMicro::GetDiscDrive() {
    if (m_state.disc_control.drive >= 0 && m_state.disc_control.drive < NUM_DRIVES) {
        return &m_state.drives[m_state.disc_control.drive];
    } else {
        return nullptr;
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BBCMicro::InitDiscDriveSounds(DiscDriveType type) {
    for (size_t i = 0; i < DiscDriveSound_EndValue; ++i) {
        m_disc_drive_sounds[i] = &DUMMY_DISC_DRIVE_SOUND;
    }

    auto &&it = g_disc_drive_sounds.find(type);
    if (it == g_disc_drive_sounds.end()) {
        return;
    }

    for (size_t i = 0; i < DiscDriveSound_EndValue; ++i) {
        if (!it->second[i].empty()) {
            m_disc_drive_sounds[i] = &it->second[i];
        }
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

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

void BBCMicro::StepSound(BBCMicroState::DiscDrive *dd) {
    if (dd->step_sound_index < 0) {
        // step
        dd->step_sound_index = 0;
    } else if (dd->seek_sound == DiscDriveSound_EndValue) {
        // skip a bit of the step sound
        dd->step_sound_index += (int)SOUND_CLOCK_HZ / 100;

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

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

float BBCMicro::UpdateDiscDriveSound(BBCMicroState::DiscDrive *dd) {
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

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BBCMicro::UpdateCPUDataBusFn() {
    uint32_t update_flags = 0;

    if (m_state.hack_flags != 0) {
        update_flags |= BBCMicroUpdateFlag_Hacks;
    }

#if BBCMICRO_TRACE
    if (m_trace) {
        update_flags |= BBCMicroUpdateFlag_Trace;
    }
#endif

#if BBCMICRO_DEBUGGER
    if (m_debug) {
        if (m_debug->step_type == BBCMicroStepType_None) {
            if (m_debug->num_breakpoint_bytes > 0) {
                update_flags |= BBCMicroUpdateFlag_Debug;
            }
        } else {
            ASSERT(m_debug->step_cpu);
            auto metadata = (const M6502Metadata *)m_debug->step_cpu->context;
            update_flags |= BBCMicroUpdateFlag_Debug | metadata->debug_step_update_flag;
        }
    }
#endif

    if (!m_host_instruction_fns.empty()) {
        update_flags |= BBCMicroUpdateFlag_Hacks;
    }

    if (!m_host_write_fns.empty()) {
        update_flags |= BBCMicroUpdateFlag_Hacks;
    }

    if (m_state.type->type_id == BBCMicroTypeID_Master) {
        update_flags |= BBCMicroUpdateFlag_IsMaster128;
    } else if (m_state.type->type_id == BBCMicroTypeID_MasterCompact) {
        update_flags |= BBCMicroUpdateFlag_IsMasterCompact;
    }

    if (m_state.parasite_type != BBCMicroParasiteType_None) {
        update_flags |= BBCMicroUpdateFlag_Parasite;

        if (m_state.parasite_type == BBCMicroParasiteType_External3MHz6502) {
            update_flags |= BBCMicroUpdateFlag_Parasite3MHzExternal;
        }

        if (m_state.parasite_boot_mode ||
            m_state.parasite_tube.status.bits.p ||
            m_state.parasite_tube.status.bits.t) {
            update_flags |= BBCMicroUpdateFlag_ParasiteSpecial;
        }
    }

    if (update_flags & BBCMicroUpdateFlag_ParasiteSpecial) {
        ASSERT(update_flags & BBCMicroUpdateFlag_Parasite);
    }

    if (m_state.printer_enabled) {
        update_flags |= BBCMicroUpdateFlag_ParallelPrinter;
    }

    if (m_state.init_flags & BBCMicroInitFlag_Mouse) {
        update_flags |= BBCMicroUpdateFlag_Mouse;
    }

#if BBCMICRO_DEBUGGER
    this->UpdateUpdateMFnData();
    if (update_flags != m_update_flags) {
        ++m_update_mfn_data->num_update_mfn_changes;
    }
#endif

    update_flags |= (uint32_t)m_state.paging.rom_types[m_state.paging.romsel.b_bits.pr] << BBCMicroUpdateFlag_ROMTypeShift;

    ASSERT(update_flags < sizeof ms_update_mfns / sizeof ms_update_mfns[0]);
    m_update_flags = update_flags;
    m_update_mfn = ms_update_mfns[update_flags];
    ASSERT(m_update_mfn);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BBCMicro::SetMMIOFnsInternal(uint16_t addr, ReadMMIOFn read_fn, void *read_context, WriteMMIOFn write_fn, void *write_context, bool set_xfj, bool set_ifj) {
    ASSERT(set_xfj || set_ifj);
    ASSERT(addr >= 0xfc00 && addr <= 0xfeff);

    uint16_t index = addr - 0xfc00;

    ReadMMIO read_mmio;
    if (read_fn) {
        read_mmio = {read_fn, read_context};
    } else {
        read_mmio = {&ReadUnmappedMMIO, this};
    }

    WriteMMIO write_mmio;
    if (write_fn) {
        write_mmio = {write_fn, write_context};
    } else {
        write_mmio = {&WriteUnmappedMMIO, this};
    }

    if (set_xfj) {
        m_write_mmios_hw[index] = write_mmio;
        m_read_mmios_hw[index] = read_mmio;
    }

    if (set_ifj) {
        m_write_mmios_hw_cartridge[index] = write_mmio;
        m_read_mmios_hw_cartridge[index] = read_mmio;
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

uint16_t BBCMicro::GetAnalogueChannel(uint8_t channel) const {
    ASSERT(channel < 4);
    return m_state.analogue_channel_values[channel];
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BBCMicro::SetAnalogueChannel(uint8_t channel, uint16_t value) {
    ASSERT(channel < 4);
    m_state.analogue_channel_values[channel] = value;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

uint16_t BBCMicro::ReadAnalogueChannel(uint8_t channel, void *context) {
    auto m = (BBCMicro *)context;

    uint16_t value = m->GetAnalogueChannel(channel);
    return value;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

BBCMicroState::DigitalJoystickInput BBCMicro::GetDigitalJoystickState(uint8_t index) const {
    (void)index;

    return m_state.digital_joystick_state;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BBCMicro::SetDigitalJoystickState(uint8_t index, BBCMicroState::DigitalJoystickInput state) {
    (void)index;

    m_state.digital_joystick_state = state;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if BBCMICRO_DEBUGGER
void BBCMicro::UpdateUpdateMFnData() {
    if (m_update_mfn) {
        ASSERT(m_update_flags < NUM_UPDATE_MFNS);
        m_update_mfn_data->update_mfn_cycle_count[m_update_flags].n += m_state.cycle_count.n - m_last_mfn_change_cycle_count.n;
        m_last_mfn_change_cycle_count = m_state.cycle_count;
    }
}
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BBCMicro::UpdateMapperRegion(uint8_t region) {
    m_state.paging.rom_regions[m_state.paging.romsel.b_bits.pr] = region;
    this->UpdatePaging();
    // The update_mfn won't change.

#if BBCMICRO_TRACE
    if (m_trace) {
        m_trace->AllocSetMapperRegionEvent(region);
    }
#endif
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BBCMicro::EnsureUpdateMFnsTableIsReady() {
#if BBCMICRO_NUM_UPDATE_GROUPS > 1
    if (!ms_update_mfns[0]) {
        for (size_t i = 0; i < NUM_UPDATE_MFNS; ++i) {
            const UpdateMFn *mfns = nullptr; //i % 2 == 0 ? ms_update_mfns0 : ms_update_mfns1;
            switch (i % BBCMICRO_NUM_UPDATE_GROUPS) {
            default:
                ASSERT(false);
            case 0:
                mfns = ms_update_mfns0;
                break;

            case 1:
                mfns = ms_update_mfns1;
                break;

#if BBCMICRO_NUM_UPDATE_GROUPS > 2
            case 2:
                mfns = ms_update_mfns2;
                break;

            case 3:
                mfns = ms_update_mfns3;
                break;
#endif
            }

            ms_update_mfns[i] = mfns[i / BBCMICRO_NUM_UPDATE_GROUPS];
        }
    }
#endif

    for (size_t i = 0; i < NUM_UPDATE_MFNS; ++i) {
        ASSERT(ms_update_mfns[i]);
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
