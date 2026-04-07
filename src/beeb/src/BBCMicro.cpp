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
#include <unordered_map>

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

BBCMicro::UpdateMFn BBCMicro::ms_update_mfns[NUM_BBCMICRO_UPDATE_MFNS];

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

#if BBCMICRO_DEBUGGER
class BBCMicroReadOnlyStateWithDebugMMIO : public BBCMicroReadOnlyState {
  public:
    explicit BBCMicroReadOnlyStateWithDebugMMIO(const BBCMicroUniqueState &src, std::shared_ptr<BBCMicro::DebugReadMMIOData> debug_read_mmio_data)
        : BBCMicroReadOnlyState(src)
        , m_debug_read_mmio_data_ptr(std::move(debug_read_mmio_data))
        , m_type_id(this->type->type_id)
        , m_debug_read_mmio_data(m_debug_read_mmio_data_ptr.get()) {
    }

    DebugReadMMIOResult DebugReadMMIO(uint8_t *value, M6502Word addr, uint8_t host_io_flags) const override {
        ASSERT(addr.w >= IO_BEGIN_ADDRESS.w && addr.w < IO_END_ADDRESS.w);

        const std::vector<BBCMicro::DebugReadMMIO> *table = &m_debug_read_mmio_data->debug_read_mmios[host_io_flags & 3];

        uint16_t index = addr.w - IO_BEGIN_ADDRESS.w;
        const BBCMicro::DebugReadMMIO *debug_read_mmio = &(*table)[index];

        if (!debug_read_mmio->set) {
            return DebugReadMMIOResult_Unset;
        }

        if (!debug_read_mmio->fn) {
            return DebugReadMMIOResult_Unmapped;
        }

        const void *context = (*debug_read_mmio->context_fn)(this);
        *value = (*debug_read_mmio->fn)(context, addr);

        return DebugReadMMIOResult_GotValue;
    }

    uint8_t DebugGetStaleDataBusByte() const override {
        BBCMicroTypeID type_id = this->type->type_id;
        switch (type_id) {
        default:
            ASSERT(false);
            [[fallthrough]];
        case BBCMicroTypeID_B:
        case BBCMicroTypeID_BPlus:
            return this->cpu.dbus;

        case BBCMicroTypeID_Master:
        case BBCMicroTypeID_MasterCompact:
            return this->last_fetched_video_byte;

        case BBCMicroTypeID_Electron:
            return 0xff;
        }
    }

  protected:
  private:
    const std::shared_ptr<BBCMicro::DebugReadMMIOData> m_debug_read_mmio_data_ptr;

    // Save on some shared_ptr lookups...
    const BBCMicroTypeID m_type_id;
    BBCMicro::DebugReadMMIOData *const m_debug_read_mmio_data = nullptr;
};
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

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

const BBCMicro::UpdateMFn *const BBCMicro::ms_update_mfn_groups[] = {
#include "../generated/BBCMicro.groups.generated.inl"
    nullptr,
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

PrinterBuffer::PrinterBuffer() {
    MUTEX_SET_NAME(m_mutex, "PrinterBuffer");
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void PrinterBuffer::Clear() {
    LockGuard<Mutex> lock(m_mutex);

    m_data.clear();
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void PrinterBuffer::AddByte(uint8_t value) {
    LockGuard<Mutex> lock(m_mutex);

    m_data.push_back(value);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

size_t PrinterBuffer::GetDataSizeBytes() const {
    LockGuard<Mutex> lock(m_mutex);

    return m_data.size();
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

std::vector<uint8_t> PrinterBuffer::GetData() const {
    LockGuard<Mutex> lock(m_mutex);

    return m_data;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if BBCMICRO_DEBUGGER
BBCMicroDebugState::~BBCMicroDebugState() {
}
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if BBCMICRO_DEBUGGER
bool BBCMicroDebugState::IsAddressDebugFlagIndex(uint32_t debug_flag_index) {
    if (debug_flag_index >= HOST_ADDRESS_DEBUG_FLAGS_INDEX && debug_flag_index < HOST_ADDRESS_DEBUG_FLAGS_INDEX + NUM_HOST_ADDRESS_DEBUG_FLAGS) {
        return true;
    } else if (debug_flag_index >= PARASITE_ADDRESS_DEBUG_FLAGS_INDEX && debug_flag_index < PARASITE_ADDRESS_DEBUG_FLAGS_INDEX + NUM_PARASITE_ADDRESS_DEBUG_FLAGS) {
        return true;
    } else {
        return false;
    }
}
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if BBCMICRO_DEBUGGER
void BBCMicroDebugState::GetDetailsFromAddressDebugFlagIndex(M6502Word *addr, uint32_t *dso, uint32_t debug_flag_index) {
    if (debug_flag_index >= BBCMicroDebugState::HOST_ADDRESS_DEBUG_FLAGS_INDEX &&
        debug_flag_index < BBCMicroDebugState::HOST_ADDRESS_DEBUG_FLAGS_INDEX + BBCMicroDebugState::NUM_HOST_ADDRESS_DEBUG_FLAGS) {
        static_assert(BBCMicroDebugState::NUM_HOST_ADDRESS_DEBUG_FLAGS <= 65536);
        addr->w = (uint16_t)(debug_flag_index - BBCMicroDebugState::HOST_ADDRESS_DEBUG_FLAGS_INDEX);
        *dso = 0;
    } else if (debug_flag_index >= BBCMicroDebugState::PARASITE_ADDRESS_DEBUG_FLAGS_INDEX &&
               debug_flag_index < BBCMicroDebugState::PARASITE_ADDRESS_DEBUG_FLAGS_INDEX + BBCMicroDebugState::NUM_PARASITE_ADDRESS_DEBUG_FLAGS) {
        static_assert(BBCMicroDebugState::NUM_PARASITE_ADDRESS_DEBUG_FLAGS <= 65536);
        addr->w = (uint16_t)(debug_flag_index - BBCMicroDebugState::PARASITE_ADDRESS_DEBUG_FLAGS_INDEX);
        *dso = BBCMicroDebugStateOverride_Parasite;
    } else {
        ASSERT(false);
        *addr = {};
        *dso = 0;
    }
}
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if BBCMICRO_DEBUGGER
void BBCMicroDebugState::GetDetailsFromByteDebugFlagIndex(BigPageIndex *big_page_index, uint16_t *big_page_offset, uint32_t debug_flag_index) {
    ASSERT(!IsAddressDebugFlagIndex(debug_flag_index));

    if (debug_flag_index >= BBCMicroDebugState::BIG_PAGES_BYTE_DEBUG_FLAGS_INDEX &&
        debug_flag_index < BBCMicroDebugState::BIG_PAGES_BYTE_DEBUG_FLAGS_INDEX + BBCMicroDebugState::NUM_BIG_PAGES_BYTE_DEBUG_FLAGS) {
        *big_page_index = {(BigPageIndex::Type)((debug_flag_index - BBCMicroDebugState::BIG_PAGES_BYTE_DEBUG_FLAGS_INDEX) / BIG_PAGE_SIZE_BYTES)};
        *big_page_offset = debug_flag_index % BIG_PAGE_SIZE_BYTES;
    } else if (debug_flag_index >= BBCMicroDebugState::IO_BYTE_DEBUG_FLAGS_INDEX &&
               debug_flag_index < BBCMicroDebugState::IO_BYTE_DEBUG_FLAGS_INDEX + BBCMicroDebugState::NUM_IO_BYTE_DEBUG_FLAGS) {
        auto region = (BBCMicroIOByteDebugFlagRegion)((debug_flag_index - BBCMicroDebugState::IO_BYTE_DEBUG_FLAGS_INDEX) / BBCMicroDebugState::IO_BYTE_DEBUG_FLAG_REGION_SIZE_BYTES);

        if (region >= BBCMicroIOByteDebugFlagRegion_XFJ && region < BBCMicroIOByteDebugFlagRegion_XFJ + 16) {
            *big_page_index = FIRST_IO_BIG_PAGE_INDEX;
            *big_page_offset = (FJ_IO_BEGIN_ADDRESS.p.o +
                                (region - BBCMicroIOByteDebugFlagRegion_XFJ) * BBCMicroDebugState::IO_BYTE_DEBUG_FLAG_REGION_SIZE_BYTES +
                                debug_flag_index % BBCMicroDebugState::IO_BYTE_DEBUG_FLAG_REGION_SIZE_BYTES);
        } else if (region >= BBCMicroIOByteDebugFlagRegion_IFJ && region < BBCMicroIOByteDebugFlagRegion_IFJ + 16) {
            *big_page_index = {FIRST_IO_BIG_PAGE_INDEX.i + HostIOFlag_IFJ};
            *big_page_offset = (FJ_IO_BEGIN_ADDRESS.p.o +
                                (region - BBCMicroIOByteDebugFlagRegion_IFJ) * BBCMicroDebugState::IO_BYTE_DEBUG_FLAG_REGION_SIZE_BYTES +
                                debug_flag_index % BBCMicroDebugState::IO_BYTE_DEBUG_FLAG_REGION_SIZE_BYTES);
        } else if (region >= BBCMicroIOByteDebugFlagRegion_S_XTU && region < BBCMicroIOByteDebugFlagRegion_S_XTU + 8) {
            *big_page_index = FIRST_IO_BIG_PAGE_INDEX;
            *big_page_offset = (S_IO_BEGIN_ADDRESS.p.o +
                                (region - BBCMicroIOByteDebugFlagRegion_S_XTU) * BBCMicroDebugState::IO_BYTE_DEBUG_FLAG_REGION_SIZE_BYTES +
                                debug_flag_index % BBCMicroDebugState::IO_BYTE_DEBUG_FLAG_REGION_SIZE_BYTES);
        } else if (region == BBCMicroIOByteDebugFlagRegion_S_ITU) {
            *big_page_index = {FIRST_IO_BIG_PAGE_INDEX.i + HostIOFlag_ITU};
            *big_page_offset = (S_IO_BEGIN_ADDRESS.p.o + 0xe0 +
                                debug_flag_index % BBCMicroDebugState::IO_BYTE_DEBUG_FLAG_REGION_SIZE_BYTES);
        } else {
            goto bad;
        }
    } else {
    bad:
        ASSERT(false);
        *big_page_index = INVALID_BIG_PAGE_INDEX;
        *big_page_offset = 0;
    }
}
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if BBCMICRO_DEBUGGER
bool BBCMicroDebugState::CopyDebugFlags(uint8_t *dest, uint64_t *local_breakpoints_changed_counter) const {
    uint64_t breakpoints_changed_counter = this->GetBreakpointsChangedCounter();
    if (breakpoints_changed_counter != *local_breakpoints_changed_counter) {
        memcpy(dest, m_debug_flags, sizeof m_debug_flags);
        *local_breakpoints_changed_counter = breakpoints_changed_counter;
        return true;
    } else {
        return false;
    }
}
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if BBCMICRO_DEBUGGER
uint64_t BBCMicroDebugState::GetBreakpointsChangedCounter() const {
    return m_breakpoints_changed_counter.load(std::memory_order_acquire);
}
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if BBCMICRO_DEBUGGER
void BBCMicroDebugState::BreakpointsDidChange() {
    std::atomic_thread_fence(std::memory_order_release);
    m_breakpoints_changed_counter.fetch_add(1, std::memory_order_acq_rel);
}
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

BBCMicro::BBCMicro(std::shared_ptr<const BBCMicroType> type,
                   const DiscInterface *disc_interface,
                   BBCMicroParasiteType parasite_type,
                   const std::vector<uint8_t> &nvram_contents,
                   const tm *rtc_time,
                   uint32_t init_flags,
                   BeebLinkHandler *beeblink_handler,
                   const HardDiskImageSet &hard_disk_images,
                   std::string mmfs_image_path,
                   CycleCount initial_cycle_count)
    : m_state(std::move(type),
              disc_interface,
              parasite_type,
              nvram_contents,
              init_flags,
              rtc_time,
              hard_disk_images,
              std::move(mmfs_image_path),
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

    for (int i = 0; i < NUM_HARD_DISKS; ++i) {
        if (!!m_state.scsi->hds.images[i]) {
            result |= (uint32_t)BBCMicroCloneImpediment_HardDisk0 << i;
        }
    }

    if (!!m_beeblink_handler) {
        result |= BBCMicroCloneImpediment_BeebLink;
    }

    if (m_state.serproc.HasSource() || !!m_state.serproc.HasSink()) {
        result |= BBCMicroCloneImpediment_Serial;
    }

    if (!!m_state.mmfs) {
        result |= BBCMicroCloneImpediment_MMFS;
    }

    return result;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

const BBCMicroUniqueState *BBCMicro::GetCloneableUniqueState() const {
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
                          !!(trace_flags & BBCMicroTraceFlag_6845Columns),
                          !!(trace_flags & BBCMicroTraceFlag_6845Scanlines),
                          !!(trace_flags & BBCMicroTraceFlag_6845Rows),
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

    m_disk_drive_trace = trace_flags & BBCMicroTraceFlag_DiskDrive ? m_trace : nullptr;

    if (!!m_state.scsi) {
        m_state.scsi->SetTrace(m_trace_flags & BBCMicroTraceFlag_SCSI ? m_trace : nullptr);
    }

    m_state.serproc.SetTrace(m_trace_flags & BBCMicroTraceFlag_Serial ? m_trace : nullptr);
    m_state.acia.SetTrace(m_trace_flags & BBCMicroTraceFlag_Serial ? m_trace : nullptr, !!(m_trace_flags & BBCMicroTraceFlag_SerialExtra));

    m_state.plus1.SetTrace(m_trace_flags & BBCMicroTraceFlag_Plus1 ? m_trace : nullptr);

    this->UpdateCPUDataBusFn();
}
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BBCMicro::UpdatePaging() {
    MemoryBigPageTables tables;
    uint32_t paging_flags;
    (*m_state.type->get_mem_big_page_tables_fn)(&tables, &paging_flags, m_state.paging);

    for (size_t i = 0; i < 2; ++i) {
        MemoryBigPages *mbp = &m_mem_big_pages[i];

        const BigPage *bp;
        for (size_t j = 0; j < 16; ++j) {
            ASSERT(tables.mem_big_pages[i][j].i < NUM_BIG_PAGES);
            bp = &m_big_pages[tables.mem_big_pages[i][j].i];

            mbp->w[j] = bp->w;
            mbp->r[j] = bp->r;
#if BBCMICRO_DEBUGGER
            mbp->byte_debug_flags[j] = bp->byte_debug_flags;
            mbp->bp[j] = bp;
#endif
        }

        // I/O is always in the last page examined.
#if BBCMICRO_DEBUGGER
        mbp->read_io_byte_debug_flags = bp->read_io_byte_debug_flags;
        mbp->write_io_byte_debug_flags = bp->write_io_byte_debug_flags;
#endif
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

    ASSERT(tables.mem_big_pages[0][15].i >= FIRST_IO_BIG_PAGE_INDEX.i && tables.mem_big_pages[0][15].i < FIRST_IO_BIG_PAGE_INDEX.i + NUM_IO_BIG_PAGES);
    ASSERT(!(m_big_pages[tables.mem_big_pages[0][15].i].metadata->host_io_flags & HostIOFlag_NoIO));

    uint32_t host_io_flags = tables.mem_big_pages[0][15].i - FIRST_IO_BIG_PAGE_INDEX.i;

    if (host_io_flags & HostIOFlag_TST) {
        m_read_mmios = m_read_mmios_rom.data();
        m_read_mmios_new_run_state = m_mmios_new_run_state_rom.data();
    } else {
        m_read_mmios = m_read_mmios_hw[host_io_flags].data();
        m_read_mmios_new_run_state = m_mmios_new_run_state_hw[host_io_flags].data();
    }

    uint32_t write_table_index = host_io_flags & (HostIOFlag_ITU | HostIOFlag_IFJ);
    m_write_mmios = m_write_mmios_hw[write_table_index].data();
    m_write_mmios_new_run_state = m_mmios_new_run_state_hw[write_table_index].data();

    // The per-type ACCCON mask ensures that ITU will remain clear on B/B+.
    m_state.parasite_accessible = m_state.paging.acccon.m128_bits.itu == m_state.parasite_itu;

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

void BBCMicro::GetBigPageProperties(const uint8_t **read_ptr,
                                    bool *writeable_ptr,
                                    const BigPageMetadata **metadata_ptr,
                                    BigPageIndex big_page_index,
                                    const BBCMicroState *state) {
    *writeable_ptr = false;
    *read_ptr = nullptr;
    *metadata_ptr = &state->type->big_pages_metadata[big_page_index.i];

    if (big_page_index.i >= 0 &&
        big_page_index.i < 16) {
        size_t offset = big_page_index.i * BIG_PAGE_SIZE_BYTES;

        if (offset < state->ram_buffer->size()) {
            *read_ptr = &state->ram_buffer->at(offset);
            *writeable_ptr = true;
        }
    } else if (big_page_index.i >= ROM0_BIG_PAGE_INDEX.i &&
               big_page_index.i < ROM0_BIG_PAGE_INDEX.i + 16 * NUM_ROM_BIG_PAGES) {
        size_t bank = ((size_t)big_page_index.i - ROM0_BIG_PAGE_INDEX.i) / NUM_ROM_BIG_PAGES;
        ASSERT(bank < 16);
        size_t region = (((size_t)big_page_index.i - ROM0_BIG_PAGE_INDEX.i) % NUM_ROM_BIG_PAGES) / 4;
        ASSERT(region < NUM_MAPPER_REGIONS);
        size_t rom_big_page_index = (((size_t)big_page_index.i - ROM0_BIG_PAGE_INDEX.i) % NUM_ROM_BIG_PAGES) % 4;
        ASSERT(rom_big_page_index < 4);

        size_t offset = GetROMOffset(state->sideways_roms[bank].type, (uint8_t)rom_big_page_index, (uint8_t)region);
        ASSERT(offset < GetROMTypeMetadata(state->sideways_roms[bank].type)->num_bytes);
        //size_t offset = ((size_t)big_page_index.i - ROM0_BIG_PAGE_INDEX.i) % NUM_ROM_BIG_PAGES * BIG_PAGE_SIZE_BYTES;

        if (!!state->sideways_roms[bank].data) {
            *read_ptr = &state->sideways_roms[bank].data->at(offset);
        } else if (!!state->sideways_ram_buffers[bank]) {
            *read_ptr = &state->sideways_ram_buffers[bank]->at(offset);
            *writeable_ptr = true;
        }
    } else if ((big_page_index.i >= MOS_BIG_PAGE_INDEX.i &&
                big_page_index.i < MOS_BIG_PAGE_INDEX.i + NUM_MOS_BIG_PAGES)) {
        if (!!state->os_buffer) {
            size_t offset = (big_page_index.i - MOS_BIG_PAGE_INDEX.i) * BIG_PAGE_SIZE_BYTES;
            *read_ptr = &state->os_buffer->at(offset);
        }
    } else if (big_page_index.i >= FIRST_IO_BIG_PAGE_INDEX.i &&
               big_page_index.i < FIRST_IO_BIG_PAGE_INDEX.i + NUM_IO_BIG_PAGES) {
        if (!!state->os_buffer) {
            size_t offset = 3 * BIG_PAGE_SIZE_BYTES; //I/O big page is always at $f000...$ffff
            *read_ptr = &state->os_buffer->at(offset);
        }
    } else if (big_page_index.i >= PARASITE_BIG_PAGE_INDEX.i &&
               big_page_index.i < PARASITE_BIG_PAGE_INDEX.i + NUM_PARASITE_BIG_PAGES) {
        if (state->parasite_type != BBCMicroParasiteType_None) {
            size_t offset = (big_page_index.i - PARASITE_BIG_PAGE_INDEX.i) * BIG_PAGE_SIZE_BYTES;
            *read_ptr = &state->parasite_ram_buffer->at(offset);
            *writeable_ptr = true;
        }
    } else if (big_page_index.i >= PARASITE_ROM_BIG_PAGE_INDEX.i &&
               big_page_index.i < PARASITE_ROM_BIG_PAGE_INDEX.i + NUM_PARASITE_ROM_BIG_PAGES) {
        // During the initialisation, this can get called before the parasite OS
        // contents are set. Don't assert there's a rom buffer. Leave the area
        // unmapped if there isn't.
        if (!!state->parasite_rom_buffer) {
            size_t offset = (big_page_index.i - PARASITE_ROM_BIG_PAGE_INDEX.i) * BIG_PAGE_SIZE_BYTES;
            *read_ptr = &state->parasite_rom_buffer->at(offset);
        }
    } else if (big_page_index.i >= ELECTRON_KEYBOARD_BIG_PAGE_INDEX.i &&
               big_page_index.i < ELECTRON_KEYBOARD_BIG_PAGE_INDEX.i + NUM_ELECTRON_KEYBOARD_BIG_PAGES) {
        // An annoying special case, that has some special handling elsewhere.
        *read_ptr = nullptr;
        *writeable_ptr = false;
    } else {
        ASSERT(false);
    }
}

// TODO: this should probably be part of BBCMicroType, or something...?
void BBCMicro::InitReadOnlyBigPage(ReadOnlyBigPage *bp,
                                   const BBCMicroState *state,
#if BBCMICRO_DEBUGGER
                                   const BBCMicroDebugState *debug_state,
#endif
                                   BigPageIndex big_page_index) {
    bp->index = big_page_index;
    GetBigPageProperties(&bp->r, &bp->writeable, &bp->metadata, bp->index, state);

#if BBCMICRO_DEBUGGER
    auto mutable_debug_state = const_cast<BBCMicroDebugState *>(debug_state); //ugh
    bp->byte_debug_flags = GetByteDebugFlagsForBigPage(bp->metadata, mutable_debug_state);
    bp->address_debug_flags = GetAddressDebugFlagsForBigPage(bp->metadata, mutable_debug_state);
    GetIOByteDebugFlagsForBigPage(const_cast<uint8_t **>(bp->read_io_byte_debug_flags),
                                  const_cast<uint8_t **>(bp->write_io_byte_debug_flags),
                                  bp->metadata, mutable_debug_state);
#endif
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BBCMicro::InitPaging() {
    for (BigPage &bp : m_big_pages) {
        bp = {};
    }

    for (BigPageIndex big_page_index = {0}; big_page_index.i < NUM_BIG_PAGES; ++big_page_index.i) {
        BigPage *bp = &m_big_pages[big_page_index.i];
        bp->index = big_page_index;

        bool writeable;
        GetBigPageProperties(&bp->r, &writeable, &bp->metadata, bp->index, &m_state);

        if (writeable) {
            bp->w = const_cast<uint8_t *>(bp->r);
        }

        if (!bp->r) {
            bp->r = g_unmapped_reads;
        }

        if (!bp->w) {
            // Always set unwriteable regions to go somewhere.
            bp->w = g_unmapped_writes;
        }
    }

    // Fix up the ROM types.
    for (uint8_t bank = 0; bank < 16; ++bank) {
        BBCMicroUpdateROMType update_rom_type;

        switch (m_state.sideways_roms[bank].type) {
        default:
            ASSERT(false);
            [[fallthrough]];
        case ROMType_16KB:
            {
                bool all_unmapped = true;
                size_t index = (size_t)(ROM0_BIG_PAGE_INDEX.i + bank * NUM_ROM_BIG_PAGES);
                for (size_t i = 0; i < NUM_ROM_BIG_PAGES; ++i) {
                    if (m_big_pages[index + i].r != g_unmapped_reads) {
                        all_unmapped = false;
                    }
                }

                if (all_unmapped) {
                    update_rom_type = BBCMicroUpdateROMType_EmptySocket;

                    // And fix up the r fields, since a null pointer is marginally more efficient to test for.
                    for (size_t i = 0; i < NUM_ROM_BIG_PAGES; ++i) {
                        ASSERT(m_big_pages[index + i].r == g_unmapped_reads);
                        m_big_pages[index + i].r = nullptr;
                    }
                } else {
                    update_rom_type = BBCMicroUpdateROMType_16KB;
                }
            }
            break;

        case ROMType_CCIWORD:
            update_rom_type = BBCMicroUpdateROMType_CCIWORD;
            break;

        case ROMType_CCIBASE:
            update_rom_type = BBCMicroUpdateROMType_CCIBASE;
            break;

        case ROMType_CCISPELL:
            update_rom_type = BBCMicroUpdateROMType_CCISPELL;
            break;

        case ROMType_PALQST:
            update_rom_type = BBCMicroUpdateROMType_PALQST;
            break;

        case ROMType_PALWAP:
            update_rom_type = BBCMicroUpdateROMType_PALWAP;
            break;

        case ROMType_PALTED:
            update_rom_type = BBCMicroUpdateROMType_PALTED;
            break;

        case ROMType_ABEP:
        case ROMType_ABE:
            update_rom_type = BBCMicroUpdateROMType_ABEP_OR_ABE;
            break;

        case ROMType_Trilogy:
            update_rom_type = BBCMicroUpdateROMType_Trilogy;
            break;

        case ROMType_MO2:
            update_rom_type = BBCMicroUpdateROMType_MO2;
            break;
        }

        if (IsElectron(m_state.type->type_id)) {
            if (bank == ElectronULA::KEYBOARD_ROM_BANK_BASE + 0 || bank == ElectronULA::KEYBOARD_ROM_BANK_BASE + 1) {
                update_rom_type = BBCMicroUpdateROMType_ElectronKeyboard;
            }
        }

        m_state.update_rom_types[bank] = update_rom_type;
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

    ASSERT(m);
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

    ASSERT(m);
    ASSERT(m->m_state.disc_interface);

    uint8_t value;
    if (m->m_state.disc_interface->flags & DiscInterfaceFlag_ControlIsReadOnly) {
        value = m->GetStaleDatabusByte();
    } else {
        value = m->m_state.disc_interface->GetByteFromControl(m->m_state.disc_control);
    }
    return value;
}

#if BBCMICRO_DEBUGGER
uint8_t BBCMicro::DebugRead1770ControlRegister(const void *m_, M6502Word a) {
    (void)a;
    auto m = (const BBCMicroReadOnlyState *)m_;

    ASSERT(m->disc_interface);

    uint8_t value = m->disc_interface->GetByteFromControl(m->disc_control);
    return value;
}
#endif

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

    uint8_t value = m->GetStaleDatabusByte();
    return value;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

uint8_t BBCMicro::ReadROMMMIO(void *m_, M6502Word a) {
    auto m = (BBCMicro *)m_;

    // the IFJ and ITU flags are irrelevant in this situation.
    return m->m_big_pages[FIRST_IO_BIG_PAGE_INDEX.i + HostIOFlag_TST].r[a.p.o];
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

uint8_t BBCMicro::ReadROMSEL(void *m_, M6502Word a) {
    auto m = (BBCMicro *)m_;
    (void)a;

    return m->m_state.paging.romsel.value;
}

#if BBCMICRO_DEBUGGER
uint8_t BBCMicro::DebugReadROMSEL(const void *state_, M6502Word a) {
    auto state = (const BBCMicroReadOnlyState *)state_;
    (void)a;

    return state->paging.romsel.value;
}
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

template <uint8_t MASK>
void BBCMicro::WriteROMSEL(void *m_, M6502Word a, uint8_t value) {
    auto m = (BBCMicro *)m_;
    (void)a;

    if ((m->m_state.paging.romsel.value ^ value) & MASK) {
        m->m_state.paging.romsel.value = value & MASK;

        m->UpdatePaging();
        m->UpdateCPUDataBusFn();

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

#if BBCMICRO_DEBUGGER
uint8_t BBCMicro::DebugReadACCCON(const void *state_, M6502Word a) {
    auto state = (const BBCMicroReadOnlyState *)state_;
    (void)a;

    return state->paging.acccon.value;
}
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

template <uint8_t AND_VALUE>
void BBCMicro::WriteACCCON(void *m_, M6502Word a, uint8_t value) {
    auto m = (BBCMicro *)m_;
    (void)a;

    if ((m->m_state.paging.acccon.value ^ value) & AND_VALUE) {
        m->m_state.paging.acccon.value = value & AND_VALUE;
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

#if BBCMICRO_DEBUGGER
uint8_t BBCMicro::DebugReadADJI(const void *dji_, M6502Word a) {
    auto dji = (const BBCMicroState::DigitalJoystickInput *)dji_;
    (void)a;

    return dji->value;
}
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

uint8_t BBCMicro::ReadSERPROC(void *m_, M6502Word a) {
    (void)a;
    auto m = (BBCMicro *)m_;

    uint8_t value = m->GetStaleDatabusByte();

    // There's no read/write signal for the serproc. Every access is a write.
    // Good luck!
    SERPROC::Write(&m->m_state.serproc, a, value);

    if (GetBBCMicroUpdateFlagsUpdateSystemType(m->m_update_flags) == BBCMicroUpdateSystemType_BBCMicro) {
        // I don't get this. Maybe the BBC part can dissipate the signals?
        return 0;
    } else {
        return value;
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

uint8_t BBCMicro::ReadElectronULA0(void *m_, M6502Word a) {
    (void)a;
    auto m = (BBCMicro *)m_;

    ElectronIRQ irq = m->m_state.electron_ula.irq;

    m->m_state.electron_ula.irq.bits.power_on = 0;

    irq.bits.nc = 1;
    irq.bits.master = !!(m->m_state.electron_ula.irq.flag_bits.flags & m->m_state.electron_ula.irq_mask.flag_bits.flags);

    return irq.value;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

uint8_t BBCMicro::ReadElectronULA1(void *m_, M6502Word a) {
    (void)m_, (void)a;
    return 0xff;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

uint8_t BBCMicro::ReadElectronULA2(void *m_, M6502Word a) {
    (void)m_, (void)a;
    return 0xff;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

uint8_t BBCMicro::ReadElectronULA3(void *m_, M6502Word a) {
    (void)m_, (void)a;
    return 0xff;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

uint8_t BBCMicro::ReadElectronULA4(void *m_, M6502Word a) {
    (void)a;
    auto m = (BBCMicro *)m_;

    m->m_state.electron_ula.irq.bits.rx_data_full = 0;
    //ASSERT(false);

    return 0xff;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

uint8_t BBCMicro::ReadElectronULA5(void *m_, M6502Word a) {
    (void)m_, (void)a;
    return 0xff;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

uint8_t BBCMicro::ReadElectronULA6(void *m_, M6502Word a) {
    (void)m_, (void)a;
    return 0xff;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

uint8_t BBCMicro::ReadElectronULA7(void *m_, M6502Word a) {
    (void)m_, (void)a;
    return 0xff;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

uint8_t BBCMicro::ReadElectronULA8(void *m_, M6502Word a) {
    (void)m_, (void)a;
    return 0xff;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

uint8_t BBCMicro::ReadElectronULA9(void *m_, M6502Word a) {
    (void)m_, (void)a;
    return 0xff;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

uint8_t BBCMicro::ReadElectronULAA(void *m_, M6502Word a) {
    (void)m_, (void)a;
    return 0xff;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

uint8_t BBCMicro::ReadElectronULAB(void *m_, M6502Word a) {
    (void)m_, (void)a;
    return 0xff;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

uint8_t BBCMicro::ReadElectronULAC(void *m_, M6502Word a) {
    (void)m_, (void)a;
    return 0xff;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

uint8_t BBCMicro::ReadElectronULAD(void *m_, M6502Word a) {
    (void)m_, (void)a;
    return 0xff;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

uint8_t BBCMicro::ReadElectronULAE(void *m_, M6502Word a) {
    (void)m_, (void)a;
    return 0xff;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

uint8_t BBCMicro::ReadElectronULAF(void *m_, M6502Word a) {
    (void)m_, (void)a;
    return 0xff;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BBCMicro::WriteElectronULA0(void *m_, M6502Word a, uint8_t value) {
    (void)a;
    auto m = (BBCMicro *)m_;

    m->m_state.electron_ula.irq_mask.value = value;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BBCMicro::WriteElectronULA1(void *m_, M6502Word a, uint8_t value) {
    (void)m_, (void)a, (void)value;

    // This location doesn't do anything.
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BBCMicro::WriteElectronULA2(void *m_, M6502Word a, uint8_t value) {
    (void)a;
    auto m = (BBCMicro *)m_;

    m->m_state.electron_ula.display_start_address &= ~0x1ffu;
    m->m_state.electron_ula.display_start_address |= (value & 0b11100000) << 1;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BBCMicro::WriteElectronULA3(void *m_, M6502Word a, uint8_t value) {
    (void)a;
    auto m = (BBCMicro *)m_;

    m->m_state.electron_ula.display_start_address &= ~(0x3fu << 9u);
    m->m_state.electron_ula.display_start_address |= (value & 0x3fu) << 9u;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BBCMicro::WriteElectronULA4(void *m_, M6502Word a, uint8_t value) {
    (void)a, (void)value;
    auto m = (BBCMicro *)m_;

    //ASSERT(false);

    m->m_state.electron_ula.irq.bits.tx_data_empty = 0;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// Relevant discussion: https://stardot.org.uk/forums/viewtopic.php?t=27791
void BBCMicro::WriteElectronULA5(void *m_, M6502Word a, uint8_t value) {
    (void)a, (void)value;
    auto m = (BBCMicro *)m_;

    ElectronPaging paging;
    paging.value = value;

    TRACEF(m->m_trace, "Write Electron ULA R5 - $%02x", value);

    // TODO: could do with something better here, but this register is just a
    // bit weird.
    if ((m->m_state.electron_ula.romsel & 0b1100) == 0b1000) {
        if (paging.bits.rom >= 8) {
            m->m_state.electron_ula.romsel = paging.bits.rom;
        }
    } else {
        m->m_state.electron_ula.romsel = paging.bits.rom;
    }

    if (paging.bits.clear_display_end) {
        TRACEF(m->m_trace, "Write Electron ULA R5 - clear Display End IRQ");
        m->m_state.electron_ula.irq.bits.display_end = 0;
    }

    if (paging.bits.clear_rtc) {
        TRACEF(m->m_trace, "Write Electron ULA R5 - clear RTC IRQ");
        m->m_state.electron_ula.irq.bits.rtc = 0;
    }

    if (paging.bits.clear_high_tone) {
        TRACEF(m->m_trace, "Write Electron ULA R5 - clear High Tone IRQ");
        m->m_state.electron_ula.irq.bits.high_tone = 0;
    }

    if (paging.bits.clear_nmi) {
        TRACEF(m->m_trace, "Write Electron ULA R5 - clear NMI state");
        m->m_state.electron_ula.nmi = false;
    }

    if (m->m_state.electron_ula.romsel != m->m_state.paging.romsel.b_bits.pr) {
        m->m_state.paging.romsel.b_bits.pr = value & 0xf;

        m->UpdatePaging();
        m->UpdateCPUDataBusFn();

#if BBCMICRO_TRACE
        if (m->m_trace) {
            m->m_trace->AllocWriteROMSELEvent(m->m_state.paging.romsel);
        }
#endif
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BBCMicro::WriteElectronULA6(void *m_, M6502Word a, uint8_t value) {
    (void)a;
    auto m = (BBCMicro *)m_;

    switch (m->m_state.electron_ula.misc.bits.mode) {
    case ElectronULAMiscMode_SoundGeneration:
        m->m_state.electron_ula.sound_frequency = value;
        break;

    default:
        //ASSERT(false);
        break;
    }

    m->UpdateCPUDataBusFn();
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BBCMicro::WriteElectronULA7(void *m_, M6502Word a, uint8_t value) {
    (void)a;
    auto m = (BBCMicro *)m_;

    m->m_state.electron_ula.misc.value = value;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BBCMicro::WriteElectronULA8(void *m_, M6502Word a, uint8_t value) {
    (void)a;
    auto m = (BBCMicro *)m_;

    m->m_state.electron_ula.palette[0x8].bits.g = !(value & 0b00000100);
    m->m_state.electron_ula.palette[0xa].bits.g = !(value & 0b00001000);
    m->m_state.electron_ula.palette[0x0].bits.b = !(value & 0b00010000);
    m->m_state.electron_ula.palette[0x2].bits.b = !(value & 0b00100000);
    m->m_state.electron_ula.palette[0x8].bits.b = !(value & 0b01000000);
    m->m_state.electron_ula.palette[0xa].bits.b = !(value & 0b10000000);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BBCMicro::WriteElectronULA9(void *m_, M6502Word a, uint8_t value) {
    (void)a;
    auto m = (BBCMicro *)m_;

    m->m_state.electron_ula.palette[0x0].bits.r = !(value & 0b00000001);
    m->m_state.electron_ula.palette[0x2].bits.r = !(value & 0b00000010);
    m->m_state.electron_ula.palette[0x8].bits.r = !(value & 0b00000100);
    m->m_state.electron_ula.palette[0xa].bits.r = !(value & 0b00001000);
    m->m_state.electron_ula.palette[0x0].bits.g = !(value & 0b00010000);
    m->m_state.electron_ula.palette[0x2].bits.g = !(value & 0b00100000);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BBCMicro::WriteElectronULAA(void *m_, M6502Word a, uint8_t value) {
    (void)a;
    auto m = (BBCMicro *)m_;

    m->m_state.electron_ula.palette[0xc].bits.g = !(value & 0b00000100);
    m->m_state.electron_ula.palette[0xe].bits.g = !(value & 0b00001000);
    m->m_state.electron_ula.palette[0x4].bits.b = !(value & 0b00010000);
    m->m_state.electron_ula.palette[0x6].bits.b = !(value & 0b00100000);
    m->m_state.electron_ula.palette[0xc].bits.b = !(value & 0b01000000);
    m->m_state.electron_ula.palette[0xe].bits.b = !(value & 0b10000000);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BBCMicro::WriteElectronULAB(void *m_, M6502Word a, uint8_t value) {
    (void)a;
    auto m = (BBCMicro *)m_;

    m->m_state.electron_ula.palette[0x4].bits.r = !(value & 0b00000001);
    m->m_state.electron_ula.palette[0x6].bits.r = !(value & 0b00000010);
    m->m_state.electron_ula.palette[0xc].bits.r = !(value & 0b00000100);
    m->m_state.electron_ula.palette[0xe].bits.r = !(value & 0b00001000);
    m->m_state.electron_ula.palette[0x4].bits.g = !(value & 0b00010000);
    m->m_state.electron_ula.palette[0x6].bits.g = !(value & 0b00100000);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BBCMicro::WriteElectronULAC(void *m_, M6502Word a, uint8_t value) {
    (void)a;
    auto m = (BBCMicro *)m_;

    m->m_state.electron_ula.palette[0xd].bits.g = !(value & 0b00000100);
    m->m_state.electron_ula.palette[0xf].bits.g = !(value & 0b00001000);
    m->m_state.electron_ula.palette[0x5].bits.b = !(value & 0b00010000);
    m->m_state.electron_ula.palette[0x7].bits.b = !(value & 0b00100000);
    m->m_state.electron_ula.palette[0xd].bits.b = !(value & 0b01000000);
    m->m_state.electron_ula.palette[0xf].bits.b = !(value & 0b10000000);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BBCMicro::WriteElectronULAD(void *m_, M6502Word a, uint8_t value) {
    (void)a;
    auto m = (BBCMicro *)m_;

    m->m_state.electron_ula.palette[0x5].bits.r = !(value & 0b00000001);
    m->m_state.electron_ula.palette[0x7].bits.r = !(value & 0b00000010);
    m->m_state.electron_ula.palette[0xd].bits.r = !(value & 0b00000100);
    m->m_state.electron_ula.palette[0xf].bits.r = !(value & 0b00001000);
    m->m_state.electron_ula.palette[0x5].bits.g = !(value & 0b00010000);
    m->m_state.electron_ula.palette[0x7].bits.g = !(value & 0b00100000);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BBCMicro::WriteElectronULAE(void *m_, M6502Word a, uint8_t value) {
    (void)a;
    auto m = (BBCMicro *)m_;

    m->m_state.electron_ula.palette[0x9].bits.g = !(value & 0b00000100);
    m->m_state.electron_ula.palette[0xb].bits.g = !(value & 0b00001000);
    m->m_state.electron_ula.palette[0x1].bits.b = !(value & 0b00010000);
    m->m_state.electron_ula.palette[0x3].bits.b = !(value & 0b00100000);
    m->m_state.electron_ula.palette[0x9].bits.b = !(value & 0b01000000);
    m->m_state.electron_ula.palette[0xb].bits.b = !(value & 0b10000000);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BBCMicro::WriteElectronULAF(void *m_, M6502Word a, uint8_t value) {
    (void)a;
    auto m = (BBCMicro *)m_;

    m->m_state.electron_ula.palette[0x1].bits.r = !(value & 0b00000001);
    m->m_state.electron_ula.palette[0x3].bits.r = !(value & 0b00000010);
    m->m_state.electron_ula.palette[0x9].bits.r = !(value & 0b00000100);
    m->m_state.electron_ula.palette[0xb].bits.r = !(value & 0b00001000);
    m->m_state.electron_ula.palette[0x1].bits.g = !(value & 0b00010000);
    m->m_state.electron_ula.palette[0x3].bits.g = !(value & 0b00100000);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

uint8_t BBCMicro::GetStaleDatabusByte() const {
    switch (GetBBCMicroUpdateFlagsUpdateSystemType(m_update_flags)) {
    default:
        ASSERT(false);
        [[fallthrough]];
    case BBCMicroUpdateSystemType_BBCMicro:
        return m_state.cpu.dbus;

    case BBCMicroUpdateSystemType_Master128:
    case BBCMicroUpdateSystemType_MasterCompact:
        return m_state.last_fetched_video_byte;

    case BBCMicroUpdateSystemType_ElectronWithPlus1:
        return 0xff;
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

std::shared_ptr<const BBCMicroType> BBCMicro::GetBBCMicroType() const {
    return m_state.type;
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
    uint8_t *column, mask;
    this->GetKeyColumnAndMask(key, &column, &mask);
    if (column) {
        return !!(*column & mask);
    } else {
        return false;
    }
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

    //bool old_state;

    //uint8_t *column = &m_state.key_columns[key & 0x0f];
    //uint8_t mask = 1 << (key >> 4);
    //bool old_state = (*column & mask) != 0;

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
        uint8_t *column, mask;
        this->GetKeyColumnAndMask(key, &column, &mask);

        bool old_state;
        if (column) {
            old_state = !!(*column & mask);
        } else {
            old_state = false;
        }

        if (!old_state && new_state) {
            ASSERT(m_state.num_keys_down < 256);
            ++m_state.num_keys_down;

            if (column) {
                *column |= mask;
            }

            return true;
        } else if (old_state && !new_state) {
            ASSERT(m_state.num_keys_down > 0);
            --m_state.num_keys_down;

            if (column) {
                *column &= ~mask;
            }

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

void BBCMicro::SetTeletextDimFlash(bool dim_flash) {
    m_state.saa5050.dim_flash = dim_flash;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BBCMicro::SetTeletextFlashVisibleOverride(const bool *overridden_state) {
    if (overridden_state) {
        m_state.saa5050.override_flash_visible = true;
        m_state.saa5050.overridden_flash_visible = *overridden_state;
    } else {
        m_state.saa5050.override_flash_visible = false;
    }
}

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

    if (IsElectron(m_state.type->type_id)) {
        if (m_state.electron_ula.misc.bits.caps_lock) {
            leds |= BBCMicroLEDFlag_CapsLock;
        }
    } else {
        if (!(m_state.addressable_latch.bits.caps_lock_led)) {
            leds |= BBCMicroLEDFlag_CapsLock;
        }

        if (!(m_state.addressable_latch.bits.shift_lock_led)) {
            leds |= BBCMicroLEDFlag_ShiftLock;
        }

        if (m_state.serproc.IsMotorOn()) {
            leds |= BBCMicroLEDFlag_TapeMotor;
        }
    }

    for (int i = 0; i < NUM_DRIVES; ++i) {
        if (m_state.drives[i].motor) {
            leds |= 1u << (BBCMicroLEDFlag_FloppyDisk0Shift + i);
        }
    }

    if (!!m_state.scsi) {
        leds |= (uint32_t)(m_state.scsi->leds_ever_on | m_state.scsi->leds) << BBCMicroLEDFlag_HardDisk0Shift;
        m_state.scsi->leds_ever_on = 0;
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

    m_state.sideways_roms[bank].data = std::move(data);
    m_state.sideways_roms[bank].type = type;
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
    m_state.sideways_roms[bank] = {};

    this->InitPaging();
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BBCMicro::SetParasiteOS(std::shared_ptr<const std::array<uint8_t, 4096>> data) {
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

void BBCMicro::AddHostInstructionCallback(InstructionFn fn, void *context) {
    if (m_host_instruction_callbacks.AddCallback(fn, context)) {
        this->CallbacksDidChange();
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BBCMicro::RemoveHostInstructionCallback(InstructionFn fn, void *context) {
    if (m_host_instruction_callbacks.RemoveCallback(fn, context)) {
        this->CallbacksDidChange();
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BBCMicro::AddHostWriteCallback(WriteFn fn, void *context) {
    if (m_host_write_callbacks.AddCallback(fn, context)) {
        this->CallbacksDidChange();
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BBCMicro::RemoveHostWriteCallback(WriteFn fn, void *context) {
    if (m_host_write_callbacks.RemoveCallback(fn, context)) {
        this->CallbacksDidChange();
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static uint8_t GetFJScopeFlags(bool xfj, bool ifj) {
    ASSERT(xfj || ifj);

    // XTU vs ITU doesn't affect FRED or JIM.
    uint8_t scope = BBCMicroMMIOScopeFlag_XTU | BBCMicroMMIOScopeFlag_ITU;

    if (xfj) {
        scope |= BBCMicroMMIOScopeFlag_XFJ;
    }

    if (ifj) {
        scope |= BBCMicroMMIOScopeFlag_IFJ;
    }

    return scope;
}

static uint8_t GetSScopeFlags(bool xtu, bool itu) {
    ASSERT(xtu || itu);

    // XFJ vs IFJ doesn't affect SHEILA.
    uint8_t scope = BBCMicroMMIOScopeFlag_XFJ | BBCMicroMMIOScopeFlag_IFJ;

    if (xtu) {
        scope |= BBCMicroMMIOScopeFlag_XTU;
    }

    if (itu) {
        scope |= BBCMicroMMIOScopeFlag_ITU;
    }

    return scope;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BBCMicro::SetSIO(uint16_t addr, ReadMMIOFn read_fn, void *read_context, WriteMMIOFn write_fn, void *write_context, bool xtu, bool itu) {
    ASSERT(addr >= S_IO_BEGIN_ADDRESS.w && addr < S_IO_END_ADDRESS.w);
    this->SetMMIOFnsInternal(addr, read_fn, read_context, write_fn, write_context, GetSScopeFlags(xtu, itu));
}

#if BBCMICRO_DEBUGGER
void BBCMicro::SetDebugSIO(uint16_t addr, DebugReadMMIOFn debug_read_fn, DebugGetReadMMIOContextFn debug_get_context_fn, bool xtu, bool itu) {
    ASSERT(addr >= S_IO_BEGIN_ADDRESS.w && addr < S_IO_END_ADDRESS.w);
    this->SetDebugMMIOFnsInternal(addr, debug_read_fn, debug_get_context_fn, GetSScopeFlags(xtu, itu));
}
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BBCMicro::SetXFJIO(uint16_t addr, ReadMMIOFn read_fn, void *read_context, WriteMMIOFn write_fn, void *write_context) {
    ASSERT(addr >= FJ_IO_BEGIN_ADDRESS.w && addr <= FJ_IO_END_ADDRESS.w);
    this->SetMMIOFnsInternal(addr, read_fn, read_context, write_fn, write_context, GetFJScopeFlags(true, false));
}

#if BBCMICRO_DEBUGGER
void BBCMicro::SetDebugXFJIO(uint16_t addr, DebugReadMMIOFn debug_read_fn, DebugGetReadMMIOContextFn debug_get_context_fn) {
    ASSERT(addr >= FJ_IO_BEGIN_ADDRESS.w && addr <= FJ_IO_END_ADDRESS.w);
    this->SetDebugMMIOFnsInternal(addr, debug_read_fn, debug_get_context_fn, GetFJScopeFlags(true, false));
}
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BBCMicro::SetIFJIO(uint16_t addr, ReadMMIOFn read_fn, void *read_context, WriteMMIOFn write_fn, void *write_context) {
    ASSERT(addr >= FJ_IO_BEGIN_ADDRESS.w && addr <= FJ_IO_END_ADDRESS.w);
    this->SetMMIOFnsInternal(addr, read_fn, read_context, write_fn, write_context, GetFJScopeFlags(false, true));
}

#if BBCMICRO_DEBUGGER
void BBCMicro::SetDebugIFJIO(uint16_t addr, DebugReadMMIOFn debug_read_fn, DebugGetReadMMIOContextFn debug_get_context_fn) {
    ASSERT(addr >= FJ_IO_BEGIN_ADDRESS.w && addr <= FJ_IO_END_ADDRESS.w);
    this->SetDebugMMIOFnsInternal(addr, debug_read_fn, debug_get_context_fn, GetFJScopeFlags(false, true));
}
#endif

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

#if ENABLE_TAPE
std::shared_ptr<const UEFReader> BBCMicro::GetTape() const {
    return m_state.tape;
}
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if ENABLE_TAPE
void BBCMicro::SetTape(std::shared_ptr<const UEFReader> tape) {
    m_state.tape = std::move(tape);
}
#endif

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

void BBCMicro::StartPaste(std::string text) {
    this->StopPaste();

    if (!text.empty()) {
        m_state.hack_flags |= BBCMicroHackFlag_Paste;
        m_state.paste_state = BBCMicroPasteState_DelayBeforeStartKey;
        m_state.paste_text = std::make_shared<std::string>(std::move(text));
        m_state.paste_index = 0;
        m_state.paste_delay_cycles = 10000; //whatever

        //this->SetKeyState(PASTE_START_KEY, true);

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

#if BBCMICRO_DEBUGGER
std::shared_ptr<const BBCMicroReadOnlyState> BBCMicro::DebugGetState() const {
    auto result = std::make_shared<BBCMicroReadOnlyStateWithDebugMMIO>(m_state, m_debug_read_mmio_data);
    return result;
}
#endif

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
                                         const BBCMicroDebugState *debug_state,
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
const uint8_t *BBCMicro::DebugGetReadByteDebugFlags(const BigPage *big_page,
                                                    uint16_t offset) {
    return DebugGetByteDebugFlags(big_page->metadata,
                                  big_page->byte_debug_flags,
                                  big_page->read_io_byte_debug_flags,
                                  big_page->write_io_byte_debug_flags,
                                  {offset},
                                  false);
}
#endif

#if BBCMICRO_DEBUGGER
const uint8_t *BBCMicro::DebugGetWriteByteDebugFlags(const BigPage *big_page,
                                                     uint16_t offset) {
    return DebugGetByteDebugFlags(big_page->metadata,
                                  big_page->byte_debug_flags,
                                  big_page->read_io_byte_debug_flags,
                                  big_page->write_io_byte_debug_flags,
                                  {offset},
                                  true);
}
#endif

#if BBCMICRO_DEBUGGER
const uint8_t *BBCMicro::DebugGetReadByteDebugFlags(const ReadOnlyBigPage *big_page, uint16_t offset) {
    return DebugGetByteDebugFlags(big_page->metadata,
                                  const_cast<uint8_t *>(big_page->byte_debug_flags),
                                  const_cast<uint8_t *const *>(big_page->read_io_byte_debug_flags),
                                  const_cast<uint8_t *const *>(big_page->write_io_byte_debug_flags),
                                  {offset},
                                  false);
}
#endif

#if BBCMICRO_DEBUGGER
const uint8_t *BBCMicro::DebugGetWriteByteDebugFlags(const ReadOnlyBigPage *big_page, uint16_t offset) {
    return DebugGetByteDebugFlags(big_page->metadata,
                                  const_cast<uint8_t *>(big_page->byte_debug_flags),
                                  const_cast<uint8_t *const *>(big_page->read_io_byte_debug_flags),
                                  const_cast<uint8_t *const *>(big_page->write_io_byte_debug_flags),
                                  {offset},
                                  true);
}
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if BBCMICRO_DEBUGGER
void BBCMicro::DebugModifyReadByteDebugFlags(BigPageIndex big_page_index,
                                             uint16_t offset,
                                             uint8_t clear_flags,
                                             uint8_t set_flags) {
    this->DebugModifyByteDebugFlags(big_page_index, {offset}, false, clear_flags, set_flags);
}
#endif

#if BBCMICRO_DEBUGGER
void BBCMicro::DebugModifyWriteByteDebugFlags(BigPageIndex big_page_index,
                                              uint16_t offset,
                                              uint8_t clear_flags,
                                              uint8_t set_flags) {
    this->DebugModifyByteDebugFlags(big_page_index, {offset}, true, clear_flags, set_flags);
}
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if BBCMICRO_DEBUGGER
uint8_t BBCMicro::DebugGetAddressDebugFlags(M6502Word addr, uint32_t dso) const {
    if (m_debug) {
        if (dso & BBCMicroDebugStateOverride_Parasite) {
            return m_debug->m_debug_flags[BBCMicroDebugState::PARASITE_ADDRESS_DEBUG_FLAGS_INDEX + addr.w];
        } else {
            return m_debug->m_debug_flags[BBCMicroDebugState::HOST_ADDRESS_DEBUG_FLAGS_INDEX + addr.w];
        }
    } else {
        return 0;
    }
}
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if BBCMICRO_DEBUGGER
void BBCMicro::DebugModifyAddressDebugFlags(M6502Word addr, uint32_t dso, uint8_t clear_flags, uint8_t set_flags) {
    if (m_debug) {
        uint32_t debug_flags_index;
        if (dso & BBCMicroDebugStateOverride_Parasite) {
            debug_flags_index = BBCMicroDebugState::PARASITE_ADDRESS_DEBUG_FLAGS_INDEX + addr.w;
        } else {
            debug_flags_index = BBCMicroDebugState::HOST_ADDRESS_DEBUG_FLAGS_INDEX + addr.w;
        }

        uint8_t *addr_flags = &m_debug->m_debug_flags[debug_flags_index];
        uint8_t new_flags = (*addr_flags & ~clear_flags) | set_flags;

        if (*addr_flags != new_flags) {
            if (*addr_flags == 0) {
                ++m_debug->num_breakpoint_bytes;
            } else if (new_flags == 0) {
                ASSERT(m_debug->num_breakpoint_bytes > 0);
                --m_debug->num_breakpoint_bytes;
            }

            *addr_flags = new_flags;

            m_debug->BreakpointsDidChange();

            if (new_flags & BBCMicroByteDebugFlag_TempBreakExecute) {
                m_debug->m_temp_execute_breakpoints.push_back(debug_flags_index);
            }
        }

        this->UpdateCPUDataBusFn();
    }
}
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if BBCMICRO_DEBUGGER
void BBCMicro::DebugResetAllAddressAndByteDebugFlags() {
    if (m_debug) {
        memset(m_debug->m_debug_flags, 0, sizeof m_debug->m_debug_flags);

        m_debug->m_temp_execute_breakpoints.clear();
        m_debug->num_breakpoint_bytes = 0;

        m_debug->BreakpointsDidChange();

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
void BBCMicro::DebugHalt(BBCMicroHaltReason reason, const BBCMicroM6502Metadata *cpu_metadata, int32_t addr) {
    ASSERT(reason != BBCMicroHaltReason_None);
    if (m_debug) {
        m_debug->halt_reason = m_debug_halt_reason = reason;
        m_debug->halt_cpu_metadata = cpu_metadata;
        m_debug->halt_addr = addr;

        if (!m_debug->m_temp_execute_breakpoints.empty()) {
            for (uint32_t index : m_debug->m_temp_execute_breakpoints) {
                ASSERT(index < BBCMicroDebugState::NUM_DEBUG_FLAGS);
                uint8_t *flags = &m_debug->m_debug_flags[index];

                uint8_t old = *flags;
                *flags &= (uint8_t)~BBCMicroByteDebugFlag_TempBreakExecute;
                if (old != 0 && *flags == 0) {
                    ASSERT(m_debug->num_breakpoint_bytes > 0);
                    --m_debug->num_breakpoint_bytes;
                }
            }

            m_debug->m_temp_execute_breakpoints.clear();

            m_debug->BreakpointsDidChange();

            this->UpdateCPUDataBusFn();
        }

        this->SetDebugStepType(BBCMicroStepType_None, nullptr);
    }
}
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if BBCMICRO_DEBUGGER
void BBCMicro::DebugRun() {
    m_debug->halt_reason = m_debug_halt_reason = BBCMicroHaltReason_None;
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
        this->DebugModifyReadByteDebugFlags(big_page->index, next_pc.p.o, 0, BBCMicroByteDebugFlag_TempBreakExecute);

        //if (const uint8_t *flags_ = this->DebugGetReadByteDebugFlags(big_page, next_pc.p.o)) {
        //    uint8_t flags = *flags_;
        //    flags |= BBCMicroByteDebugFlag_TempBreakExecute;
        //    this->DebugSetReadByteDebugFlags(big_page->index, next_pc.p.o, flags);
        //}
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
std::shared_ptr<BBCMicroDebugState> BBCMicro::TakeDebugState() {
    std::shared_ptr<BBCMicroDebugState> debug = std::move(m_debug_ptr);

    m_debug = nullptr;

    this->UpdateDebugState();

    return debug;
}
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if BBCMICRO_DEBUGGER
std::shared_ptr<const BBCMicroDebugState> BBCMicro::GetDebugState() const {
    return m_debug_ptr;
}
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if BBCMICRO_DEBUGGER
void BBCMicro::SetDebugState(std::shared_ptr<BBCMicroDebugState> debug) {
    m_debug_ptr = std::move(debug);
    m_debug = m_debug_ptr.get();

    this->UpdateDebugState();
}
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if BBCMICRO_DEBUGGER
void BBCMicro::SetHardwareDebugState(const BBCMicroHardwareDebugState &hw) {
    if (!m_debug) {
        return;
    }

    m_debug->hw = hw;

    this->UpdateCPUDataBusFn();
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
void BBCMicro::DebugResetRelativeCycleBase(uint32_t dso) {
    if (m_debug) {
        BBCMicroDebugState::RelativeCycleCountBase BBCMicroDebugState::*base_mptr = DebugGetRelativeCycleCountBaseMPtr(m_state, dso);
        if (base_mptr) {
            BBCMicroDebugState::RelativeCycleCountBase *base = &(m_debug->*base_mptr);

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
        BBCMicroDebugState::RelativeCycleCountBase BBCMicroDebugState::*base_mptr = DebugGetRelativeCycleCountBaseMPtr(m_state, dso);
        if (base_mptr) {
            BBCMicroDebugState::RelativeCycleCountBase *base = &(m_debug->*base_mptr);

            base->reset_on_breakpoint = !base->reset_on_breakpoint;
        }
    }
}
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if BBCMICRO_DEBUGGER
BBCMicroDebugState::RelativeCycleCountBase BBCMicroDebugState::*BBCMicro::DebugGetRelativeCycleCountBaseMPtr(const BBCMicroState &state, uint32_t dso) {
    if (dso & BBCMicroDebugStateOverride_Parasite) {
        if (state.parasite_type != BBCMicroParasiteType_None) {
            return &BBCMicroDebugState::m_parasite_relative_base;
        }
    } else {
        return &BBCMicroDebugState::m_host_relative_base;
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

    // ROMType and SystemType are dealt with separately.
    uint32_t flags = flags_ & ~((BBCMicroUpdateFlag_UpdateROMTypeMask << BBCMicroUpdateFlag_UpdateROMTypeShift) |
                                (BBCMicroUpdateFlag_UpdateSystemTypeMask << BBCMicroUpdateFlag_UpdateSystemTypeShift));
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

    auto rom_type = (BBCMicroUpdateROMType)(flags_ >> BBCMicroUpdateFlag_UpdateROMTypeShift & BBCMicroUpdateFlag_UpdateROMTypeMask);
    if (!expr.empty()) {
        expr += "|";
    }

    const char *rom_type_name = GetBBCMicroUpdateROMTypeEnumName(rom_type);
    if (rom_type_name[0] == '?') {
        expr += "(BBCMicroUpdateROMType)" + std::to_string((int)rom_type);
    } else {
        expr += rom_type_name;
    }
    expr += "<<UpdateROMTypeShift";

    auto system_type = (BBCMicroUpdateSystemType)(flags_ >> BBCMicroUpdateFlag_UpdateSystemTypeShift & BBCMicroUpdateFlag_UpdateSystemTypeMask);
    if (!expr.empty()) {
        expr += "|";
    }

    const char *system_type_name = GetBBCMicroUpdateSystemTypeEnumName(system_type);
    if (system_type_name[0] == '?') {
        expr += "(BBCMicroUpdateSystemType)" + std::to_string((int)system_type);
    } else {
        expr += system_type_name;
    }
    expr += "<<UpdateSystemTypeShift";

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

static size_t LogNumUniqueInstantiations(Log *log, const char *prefix, const BBCMicro::UpdateMFn *mfns, size_t num_mfns, const size_t *num_unique_overall, size_t num_update_groups) {
    std::unordered_set<BBCMicro::UpdateMFn> update_mfns;
    for (size_t i = 0; i < num_mfns; ++i) {
        update_mfns.insert(mfns[i]);
    }

    log->f("%s: %zu/%zu unique BBCMicro::UpdateTemplated instantiations", prefix, update_mfns.size(), num_mfns);
    if (num_unique_overall) {
        log->f(" (%.2fx ideal)", (double)update_mfns.size() * num_update_groups / *num_unique_overall);
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

    size_t num_update_groups = 0;
    while (ms_update_mfn_groups[num_update_groups]) {
        ++num_update_groups;
    }
    ASSERT(NUM_BBCMICRO_UPDATE_MFNS % num_update_groups == 0);

    size_t num_unique_overall = LogNumUniqueInstantiations(log, "ms_update_mfns", ms_update_mfns, sizeof ms_update_mfns / sizeof ms_update_mfns[0], nullptr, num_update_groups);

    for (size_t i = 0; i < num_update_groups; ++i) {
        char prefix[1000];
        snprintf(prefix, sizeof prefix, "ms_update_mfns%zu", i);

        LogNumUniqueInstantiations(log, prefix, ms_update_mfn_groups[i], NUM_BBCMICRO_UPDATE_MFNS / num_update_groups, &num_unique_overall, num_update_groups);
    }

    // Every normalized flags combination should map to a unique instantation.
    {
        size_t num_surprises = 0;
        std::unordered_map<BBCMicro::UpdateMFn, std::vector<uint32_t>> update_mfns;
        for (uint32_t i : normalized_flags) {
            std::vector<uint32_t> *flags = &update_mfns[ms_update_mfns[i]];
            if (!flags->empty()) {
                if ((*flags)[0] != i) {
                    ++num_surprises;
                }
            }
            flags->push_back(i);
        }

        log->f("%zu surprise duplicates\n", num_surprises);

        for (const auto &it : update_mfns) {
            if (it.second.size() > 1) {
                log->f("0x%x (%s):\n", it.second[0], GetUpdateFlagExpr(it.second[0]).c_str());
                for (size_t i = 1; i < it.second.size(); ++i) {
                    log->f("    0x%x (%s)\n", it.second[i], GetUpdateFlagExpr(it.second[i]).c_str());
                }
            }
        }
    }

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
#if BBCMICRO_DEBUGGER
    log->f("sizeof(BBCMicroDebugState): %zu\n", sizeof(BBCMicroDebugState));
    log->f("NUM_DEBUG_FLAGS=%" PRIu32 "\n", BBCMicroDebugState::NUM_DEBUG_FLAGS);
    log->f("sizeof(std::shared_ptr<int>)=%zu\n", sizeof(std::shared_ptr<int>));
    log->f("sizeof(std::unique_ptr<int>)=%zu\n", sizeof(std::unique_ptr<int>));
#endif
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

void BBCMicro::SetPrinterBuffer(PrinterBuffer *buffer) {
    m_printer_buffer = buffer;
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

    if (GetBBCMicroUpdateFlagsUpdateSystemType(m_update_flags) == BBCMicroUpdateSystemType_MasterCompact) {
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

void BBCMicro::SetShowCursor(bool show_cursor) {
    m_cursor_mask = !!show_cursor;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BBCMicro::SetNVRAMChangedCallback(NVRAMChangedCallbackFn fn, void *context) {
    m_nvram_changed_callback_fn = fn;
    m_nvram_changed_callback_context = context;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if BBCMICRO_DEBUGGER
void BBCMicro::SetMemoryAccessErrorMasks(uint8_t ram_and, uint8_t ram_or) {
    if (ram_and != m_state.ram_and || ram_or != m_state.ram_or) {
        m_state.ram_and = ram_and;
        m_state.ram_or = ram_or;

        this->UpdateCPUDataBusFn();
    }
}
#endif

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

        bp->byte_debug_flags = GetByteDebugFlagsForBigPage(bp->metadata, m_debug);
        bp->address_debug_flags = GetAddressDebugFlagsForBigPage(bp->metadata, m_debug);
        GetIOByteDebugFlagsForBigPage(bp->read_io_byte_debug_flags, bp->write_io_byte_debug_flags, bp->metadata, m_debug);

        //if (m_debug) {
        //    const BigPageMetadata *metadata = &m_state.type->big_pages_metadata[i];
        //    if (metadata->addr != BigPageMetadata::INVALID_ADDR) {
        //        bp->byte_debug_flags = m_debug->big_pages_byte_debug_flags[bp->index.i];

        //        if (metadata->is_parasite) {
        //            bp->address_debug_flags = &m_debug->parasite_address_debug_flags[metadata->addr];
        //        } else {
        //            bp->address_debug_flags = &m_debug->host_address_debug_flags[metadata->addr];
        //        }
        //    }
        //}
    }

    for (size_t i = 0; i < 2; ++i) {
        MemoryBigPages *mbp = &m_mem_big_pages[i];

        for (size_t j = 0; j < 16; ++j) {
            mbp->byte_debug_flags[j] = mbp->bp[j] ? mbp->bp[j]->byte_debug_flags : nullptr;
        }
    }

    if (m_debug) {
        m_debug_halt_reason = m_debug->halt_reason;

        m_debug->num_host_instruction_callbacks = m_host_instruction_callbacks.GetNumCallbacks();
        m_debug->num_host_write_callbacks = m_host_write_callbacks.GetNumCallbacks();
    } else {
        m_debug_halt_reason = BBCMicroHaltReason_None;
    }
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
        if (m_debug->m_step_type != step_type) {
            m_debug->m_step_type = step_type;
            m_debug->m_step_cpu = step_cpu;
            this->UpdateCPUDataBusFn();
        }
    }
}
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if BBCMICRO_DEBUGGER
void BBCMicro::DebugHitBreakpoint(const M6502 *cpu, BBCMicroDebugState::RelativeCycleCountBase *base, uint8_t flags) {
    auto metadata = (const BBCMicroM6502Metadata *)cpu->context;
    bool maybe_update_base = false;

    if (cpu->read == 0) {
        if (flags & BBCMicroByteDebugFlag_BreakWrite) {
            maybe_update_base = true;
            this->DebugHalt(BBCMicroHaltReason_Write, metadata, cpu->abus.w);
        }
    } else {
        if (flags & BBCMicroByteDebugFlag_TempBreakExecute) {
            if (cpu->read == M6502ReadType_Opcode) {
                this->DebugHalt(BBCMicroHaltReason_SingleStep, metadata, -1);
            }
        } else if (flags & BBCMicroByteDebugFlag_BreakExecute) {
            if (cpu->read == M6502ReadType_Opcode) {
                // Only update the hit cycle count when not stepping.
                if (m_debug->m_step_type == BBCMicroStepType_None) {
                    maybe_update_base = true;
                }

                this->DebugHalt(BBCMicroHaltReason_Execute, metadata, cpu->abus.w);
            }
        }

        if (flags & BBCMicroByteDebugFlag_BreakRead) {
            if (cpu->read <= M6502ReadType_LastInterestingDataRead) {
                maybe_update_base = true;
                this->DebugHalt(BBCMicroHaltReason_Read, metadata, cpu->abus.w);
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

    switch (m_debug->m_step_type) {
    default:
        ASSERT(false);
        [[fallthrough]];
    case BBCMicroStepType_None:
        // It's valid to end up here with no step type: the flags and
        // m_update_mfn might change, but the current update function continues
        // to run. Just do nothing in this case.
        break;

    case BBCMicroStepType_StepIn:
        {
            ASSERT(m_debug->m_step_cpu);
            auto metadata = (const BBCMicroM6502Metadata *)m_debug->m_step_cpu->context;

            if (m_debug->m_step_cpu->read == M6502ReadType_Opcode) {
                // Done.
                this->DebugHalt(BBCMicroHaltReason_SingleStep, metadata, -1);
            } else if (m_debug->m_step_cpu->read == M6502ReadType_Interrupt) {
                this->DebugModifyAddressDebugFlags(m_debug->m_step_cpu->pc, metadata->dso, 0, BBCMicroByteDebugFlag_TempBreakExecute);
                //// The instruction was interrupted, so set a temp
                //// breakpoint in the right place.
                //uint8_t flags = this->DebugGetAddressDebugFlags(m_debug->step_cpu->pc, metadata->dso);

                //flags |= BBCMicroByteDebugFlag_TempBreakExecute;

                //this->DebugSetAddressDebugFlags(m_debug->step_cpu->pc, metadata->dso, flags);
                this->SetDebugStepType(BBCMicroStepType_None, nullptr);
            }
        }
        break;

    case BBCMicroStepType_StepIntoIRQHandler:
        {
            ASSERT(m_debug->m_step_cpu);
            auto metadata = (const BBCMicroM6502Metadata *)m_debug->m_step_cpu->context;

            // TODO: not sure this assert is correct? Just keep going until the opcode fetch occurs, and that'll inevitably be the start of the IRQ handler.
            //ASSERT(m_debug->m_step_cpu->read == M6502ReadType_Opcode || m_debug->m_step_cpu->read == M6502ReadType_Interrupt);

            if (m_debug->m_step_cpu->read == M6502ReadType_Opcode) {
                this->DebugHalt(BBCMicroHaltReason_Interrupt, metadata, -1);
            }
        }
        break;
    }
}
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

#if BBCMICRO_DEBUGGER
static const BBCMicro::DebugReadMMIOFn g_R6522_debug_read_fns[16] = {
    &R6522::DebugRead0,
    &R6522::DebugRead1,
    &R6522::DebugRead2,
    &R6522::DebugRead3,
    &R6522::DebugRead4,
    &R6522::DebugRead5,
    &R6522::DebugRead6,
    &R6522::DebugRead7,
    &R6522::DebugRead8,
    &R6522::DebugRead9,
    &R6522::DebugReadA,
    &R6522::DebugReadB,
    &R6522::DebugReadC,
    &R6522::DebugReadD,
    &R6522::DebugReadE,
    &R6522::DebugReadF,
};

#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

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

#if BBCMICRO_DEBUGGER
static const BBCMicro::DebugReadMMIOFn g_WD1770_debug_read_fns[] = {
    &WD1770::DebugRead0,
    &WD1770::DebugRead1,
    &WD1770::DebugRead2,
    &WD1770::DebugRead3,
};

#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static const BBCMicro::ReadMMIOFn g_tube_host_read_fns[7] = {
    &ReadHostTube1,
    &ReadHostTube2,
    &ReadHostTube3,
    &ReadHostTube4,
    &ReadHostTube5,
    &ReadHostTube6,
    &ReadHostTube7,
};

static const BBCMicro::WriteMMIOFn g_tube_host_write_fns[7] = {
    &WriteHostTube1,
    &WriteTubeDummy,
    &WriteHostTube3,
    &WriteTubeDummy,
    &WriteHostTube5,
    &WriteTubeDummy,
    &WriteHostTube7,
};

#if BBCMICRO_DEBUGGER
static const BBCMicro::DebugReadMMIOFn g_tube_host_debug_read_fns[8] = {
    &DebugReadHostTube0,
    &DebugReadHostTube1,
    &DebugReadHostTube2,
    &DebugReadHostTube3,
    &DebugReadHostTube4,
    &DebugReadHostTube5,
    &DebugReadHostTube6,
    &DebugReadHostTube7,
};
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BBCMicro::CallbacksDidChange() {
#if BBCMICRO_DEBUGGER
    this->UpdateDebugState();
#else
    this->UpdateCPUDataBusFn();
#endif
}

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

#if BBCMICRO_DEBUGGER
    m_debug_read_mmio_data = std::make_unique<DebugReadMMIOData>();
#endif
    for (int i = 0; i < 4; ++i) {
        m_read_mmios_hw[i] = std::vector<ReadMMIO>(768);
        m_write_mmios_hw[i] = std::vector<WriteMMIO>(768);
        m_mmios_new_run_state_hw[i] = std::vector<BBCMicroCPURunState>(768, BBCMicroCPURunState_Running);

#if BBCMICRO_DEBUGGER
        m_debug_read_mmio_data->debug_read_mmios[i] = std::vector<DebugReadMMIO>(768);
#endif
    }

    // Assume hardware is mapped. It will get fixed up later if
    // not.
    //m_read_mmios = m_read_mmios_hw.data();
    //m_mmios_stretch = m_mmios_stretch_hw.data();
    //m_rom_mmio = false;

    // initially no I/O
    for (uint16_t i = IO_BEGIN_ADDRESS.w; i < IO_END_ADDRESS.w; ++i) {
        this->SetMMIOFnsInternal(i, nullptr, nullptr, nullptr, nullptr, BBCMicroMMIOScopeFlag_All);
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

    if (IsBBCMicro(m_state.type->type_id)) {
        // I/O: VIAs
        for (uint16_t i = 0; i < 32; ++i) {
            this->SetSIO(0xfe40 + i, g_R6522_read_fns[i & 15], &m_state.system_via, g_R6522_write_fns[i & 15], &m_state.system_via);
            this->SetSIO(0xfe60 + i, g_R6522_read_fns[i & 15], &m_state.user_via, g_R6522_write_fns[i & 15], &m_state.user_via);
#if BBCMICRO_DEBUGGER
            this->SetDebugSIO(0xfe40 + i, g_R6522_debug_read_fns[i & 15], &GetDebugMMIOReadSystemVIAContext);
            this->SetDebugSIO(0xfe60 + i, g_R6522_debug_read_fns[i & 15], &GetDebugMMIOReadUserVIAContext);
#endif
        }

        // I/O: 6845
        for (int i = 0; i < 8; i += 2) {
            this->SetSIO((uint16_t)(0xfe00 + i + 0), &CRTC::ReadAddress, &m_state.crtc, &CRTC::WriteAddress, &m_state.crtc);
            this->SetSIO((uint16_t)(0xfe00 + i + 1), &CRTC::ReadData, &m_state.crtc, &CRTC::WriteData, &m_state.crtc);
        }

        // I/O: Video ULA
        m_state.video_ula.nula = !!(m_state.init_flags & BBCMicroInitFlag_VideoNuLA);
        {
            uint8_t video_ula_region_size;
            switch (m_state.type->type_id) {
            default:
                ASSERT(false);
                [[fallthrough]];
            case BBCMicroTypeID_B:
            case BBCMicroTypeID_BPlus:
                video_ula_region_size = 16;
                break;

            case BBCMicroTypeID_Master:
            case BBCMicroTypeID_MasterCompact:
                video_ula_region_size = 4;
                break;
            }

            for (uint8_t i = 0; i < video_ula_region_size; ++i) {
                uint16_t addr = 0xfe20 + i;

                if ((i & 2) != 0 && (m_state.init_flags & BBCMicroInitFlag_VideoNuLA)) {
                    this->SetSIO(addr, nullptr, nullptr, i & 1 ? &VideoULA::WriteNuLAPalette : &VideoULA::WriteNuLAControlRegister, &m_state.video_ula);
                } else {
                    this->SetSIO(addr, nullptr, nullptr, i & 1 ? &VideoULA::WritePalette : &VideoULA::WriteControlRegister, &m_state.video_ula);
                }
            }
        }

        // I/O: Serial
        if (m_state.HasSerial()) {
            // I/O: ULA/SERPROC
            for (int i = 0; i < 8; ++i) {
                uint16_t addr = (uint16_t)(0xfe10 + i);
                this->SetSIO(addr, &ReadSERPROC, this, &SERPROC::Write, &m_state.serproc);
#if BBCMICRO_DEBUGGER
                this->SetDebugSIO(addr, nullptr, nullptr);
#endif
            }

            // I/O: ACIA
            for (int i = 0; i < 8; i += 2) {
                uint16_t addr = (uint16_t)(0xfe08 + i);
                this->SetSIO(addr + 0, &MC6850::ReadStatusRegister, &m_state.acia, &MC6850::WriteControlRegister, &m_state.acia);
                this->SetSIO(addr + 1, &MC6850::ReadDataRegister, &m_state.acia, &MC6850::WriteDataRegister, &m_state.acia);
#if BBCMICRO_DEBUGGER
                this->SetDebugSIO(addr + 0, &MC6850::DebugReadStatusRegister, &GetDebugMMIOReadACIAContext);
                this->SetDebugSIO(addr + 0, &MC6850::DebugReadDataRegister, &GetDebugMMIOReadACIAContext);
#endif
            }

            m_state.serproc.Link(&m_state.acia);
        }

        m_state.video_ula.InitStuff();

        m_state.system_via.SetID(BBCMicroVIAID_SystemVIA, "SystemVIA");
        m_state.user_via.SetID(BBCMicroVIAID_UserVIA, "UserVIA");

        m_state.old_system_via_pb.value = m_state.system_via.b.p;

        if (CanDisplayTeletextAt3C00(m_state.type->type_id)) {
            m_teletext_bases[0] = 0x3c00;
            m_teletext_bases[1] = 0x7c00;
        } else {
            m_teletext_bases[0] = 0x7c00;
            m_teletext_bases[1] = 0x7c00;
        }
    }

    if (IsElectron(m_state.type->type_id)) {
        for (int i = 0; i < 256; i += 16) {
            this->SetSIO((uint16_t)(0xfe00 + i + 0x0), &ReadElectronULA0, this, &WriteElectronULA0, this);
            this->SetSIO((uint16_t)(0xfe00 + i + 0x1), &ReadElectronULA1, this, &WriteElectronULA1, this);
            this->SetSIO((uint16_t)(0xfe00 + i + 0x2), &ReadElectronULA2, this, &WriteElectronULA2, this);
            this->SetSIO((uint16_t)(0xfe00 + i + 0x3), &ReadElectronULA3, this, &WriteElectronULA3, this);
            this->SetSIO((uint16_t)(0xfe00 + i + 0x4), &ReadElectronULA4, this, &WriteElectronULA4, this);
            this->SetSIO((uint16_t)(0xfe00 + i + 0x5), &ReadElectronULA5, this, &WriteElectronULA5, this);
            this->SetSIO((uint16_t)(0xfe00 + i + 0x6), &ReadElectronULA6, this, &WriteElectronULA6, this);
            this->SetSIO((uint16_t)(0xfe00 + i + 0x7), &ReadElectronULA7, this, &WriteElectronULA7, this);
            this->SetSIO((uint16_t)(0xfe00 + i + 0x8), &ReadElectronULA8, this, &WriteElectronULA8, this);
            this->SetSIO((uint16_t)(0xfe00 + i + 0x9), &ReadElectronULA9, this, &WriteElectronULA9, this);
            this->SetSIO((uint16_t)(0xfe00 + i + 0xa), &ReadElectronULAA, this, &WriteElectronULAA, this);
            this->SetSIO((uint16_t)(0xfe00 + i + 0xb), &ReadElectronULAB, this, &WriteElectronULAB, this);
            this->SetSIO((uint16_t)(0xfe00 + i + 0xc), &ReadElectronULAC, this, &WriteElectronULAC, this);
            this->SetSIO((uint16_t)(0xfe00 + i + 0xd), &ReadElectronULAD, this, &WriteElectronULAD, this);
            this->SetSIO((uint16_t)(0xfe00 + i + 0xe), &ReadElectronULAE, this, &WriteElectronULAE, this);
            this->SetSIO((uint16_t)(0xfe00 + i + 0xf), &ReadElectronULAF, this, &WriteElectronULAF, this);
        }
    }

    // Initialise ADJI first, so if there's a conflict, the disk interface gets
    // priority.
    if (m_state.init_flags & BBCMicroInitFlag_ADJI) {
        uint8_t adji_addr = m_state.init_flags >> BBCMicroInitFlag_ADJIDIPSwitchesShift & 3;

        // Sigh... looks like the cartridge port is XFJ on Electron. Can I do
        // anything useful about this??

        if (IsMasterSeries(m_state.type->type_id)) {
            this->SetIFJIO(ADJI_ADDRESSES[adji_addr], &ReadADJI, this, nullptr, nullptr);
#if BBCMICRO_DEBUGGER
            this->SetDebugIFJIO(ADJI_ADDRESSES[adji_addr], &DebugReadADJI, &GetDebugMMIOReadADJIContext);
#endif
        } else if (IsElectron(m_state.type->type_id)) {
            this->SetXFJIO(ADJI_ADDRESSES[adji_addr], &ReadADJI, this, nullptr, nullptr);
#if BBCMICRO_DEBUGGER
            this->SetDebugXFJIO(ADJI_ADDRESSES[adji_addr], &DebugReadADJI, &GetDebugMMIOReadADJIContext);
#endif
        }
    }

    // I/O: disc interface
    if (m_state.disc_interface) {
        m_state.fdc.SetHandler(this);
        m_state.fdc.SetNoINTRQ(!!(m_state.disc_interface->flags & DiscInterfaceFlag_NoINTRQ));
        m_state.fdc.Set1772(!!(m_state.disc_interface->flags & DiscInterfaceFlag_1772));

        // Slightly ugly code that goes straight to the internal function. The
        // Challenger FDC and Plus 3 are in the XFJ area, so that has to be
        // catered for.
        //
        // The scope is always XTU+ITU+XFJ. If there are any IFJ-based disk
        // interfaces - which I don't think there are? - b2 doesn't support them
        // anyway.
        uint8_t fdc_scope = BBCMicroMMIOScopeFlag_XFJ | BBCMicroMMIOScopeFlag_XTU | BBCMicroMMIOScopeFlag_ITU;
        for (uint16_t i = 0; i < m_state.disc_interface->fdc_num_addrs; ++i) {
            uint16_t addr = (uint16_t)(m_state.disc_interface->fdc_addr + i);

            this->SetMMIOFnsInternal(addr, g_WD1770_read_fns[i], &m_state.fdc, g_WD1770_write_fns[i], &m_state.fdc, fdc_scope);
#if BBCMICRO_DEBUGGER
            this->SetDebugMMIOFnsInternal(addr, g_WD1770_debug_read_fns[i], &GetDebugMMIOReadFDCContext, fdc_scope);
#endif
        }

        if (m_state.disc_interface->control_addr != 0) {
            this->SetMMIOFnsInternal(m_state.disc_interface->control_addr, &Read1770ControlRegister, this, &Write1770ControlRegister, this, fdc_scope);

#if BBCMICRO_DEBUGGER
            // TODO: should really handle the read only case in a similar way with
            // the ordinary I/O functions too.
            if (m_state.disc_interface->flags & DiscInterfaceFlag_ControlIsReadOnly) {
                this->SetDebugMMIOFnsInternal(m_state.disc_interface->control_addr, nullptr, nullptr, fdc_scope);
            } else {
                this->SetDebugMMIOFnsInternal(m_state.disc_interface->control_addr, &DebugRead1770ControlRegister, &GetDebugMMIORead1770ControlRegisterContext, fdc_scope);
            }
#endif
        }

        m_state.disc_interface->InstallExtraHardware(this, m_state.disc_interface_extra_hardware);
    } else {
        m_state.fdc.SetHandler(nullptr);
    }

    if (m_beeblink_handler) {
        m_beeblink = std::make_unique<BeebLink>(m_beeblink_handler);

        if (IsBBCMicro(m_state.type->type_id)) {
            this->SetSIO(0xfe9e, &BeebLink::ReadControl, m_beeblink.get(), &BeebLink::WriteControl, m_beeblink.get());
            this->SetSIO(0xfe9f, &BeebLink::ReadData, m_beeblink.get(), &BeebLink::WriteData, m_beeblink.get());
        } else {
            this->SetXFJIO(0xfc8e, &BeebLink::ReadControl, m_beeblink.get(), &BeebLink::WriteControl, m_beeblink.get());
            this->SetXFJIO(0xfc8f, &BeebLink::ReadData, m_beeblink.get(), &BeebLink::WriteData, m_beeblink.get());
        }
    }

    this->UpdateCPUDataBusFn();

    switch (m_state.type->type_id) {
    default:
        ASSERT(false);
        [[fallthrough]];
    case BBCMicroTypeID_Electron:
        // Plus 1 stuff.
        this->SetXFJIO(0xfc70, &Plus1::Read0, &m_state.plus1, &Plus1::Write0, &m_state.plus1);
        this->SetXFJIO(0xfc71, nullptr, nullptr, &Plus1::Write1, &m_state.plus1);
        this->SetXFJIO(0xfc72, &Plus1::Read2, &m_state.plus1, nullptr, nullptr);
        this->SetXFJIO(0xfc73, nullptr, nullptr, &Plus1::Write3, &m_state.plus1);
#if BBCMICRO_DEBUGGER
        this->SetDebugXFJIO(0xfc70, &Plus1::DebugRead0, &GetDebugMMIOReadPlus1Context);
        this->SetDebugXFJIO(0xfc72, &Plus1::DebugRead2, &GetDebugMMIOReadPlus1Context);
#endif
        break;

    case BBCMicroTypeID_B:
        // The non-zero ROMSEL OR_VALUE will end up reflected in any reads, but:
        // no problem. You can't read ROMSEL on the B.
        for (uint16_t i = 0; i < 16; ++i) {
            uint16_t romsel_addr = (uint16_t)(0xfe30 + i);
            this->SetSIO(romsel_addr, &ReadUnmappedMMIO, this, &WriteROMSEL<0x0f>, this);
#if BBCMICRO_DEBUGGER
            this->SetDebugSIO(romsel_addr, nullptr, nullptr);
#endif
        }
        break;

    case BBCMicroTypeID_BPlus:
        for (uint16_t i = 0; i < 4; ++i) {
            uint16_t romsel_addr = (uint16_t)(0xfe30 + i);
            uint16_t acccon_addr = (uint16_t)(0xfe34 + i);
            this->SetSIO(romsel_addr, &ReadUnmappedMMIO, this, &WriteROMSEL<0x8f>, this);
            this->SetSIO(acccon_addr, &ReadUnmappedMMIO, this, &WriteACCCON<0x80>, this);
#if BBCMICRO_DEBUGGER
            this->SetDebugSIO(romsel_addr, nullptr, nullptr);
            this->SetDebugSIO(acccon_addr, nullptr, nullptr);
#endif
        }
        break;

    case BBCMicroTypeID_Master:
    case BBCMicroTypeID_MasterCompact:
        for (uint16_t i = 0; i < 4; ++i) {
            uint16_t romsel_addr = (uint16_t)(0xfe30 + i);
            uint16_t acccon_addr = (uint16_t)(0xfe34 + i);
            this->SetSIO(romsel_addr, &ReadROMSEL, this, &WriteROMSEL<0x8f>, this);
            this->SetSIO(acccon_addr, &ReadACCCON, this, &WriteACCCON<0xff>, this);
#if BBCMICRO_DEBUGGER
            this->SetDebugSIO(romsel_addr, &DebugReadROMSEL, &GetDebugMMIOReadROMSELContext);
            this->SetDebugSIO(romsel_addr, &DebugReadACCCON, &GetDebugMMIOReadACCCONContext);
#endif
        }
        break;
    }

    //
    if (m_state.type->adc_addr != 0) {
        ASSERT(m_state.type->adc_count % 4 == 0);
        for (unsigned i = 0; i < m_state.type->adc_count; i += 4) {
            uint16_t addr = (uint16_t)(m_state.type->adc_addr + i);
            this->SetSIO(addr + 0u, &ADC::Read0, &m_state.adc, &ADC::Write0, &m_state.adc);
            this->SetSIO(addr + 1u, &ADC::Read1, &m_state.adc, &ADC::Write1, &m_state.adc);
            this->SetSIO(addr + 2u, &ADC::Read2, &m_state.adc, &ADC::Write2, &m_state.adc);
            this->SetSIO(addr + 3u, &ADC::Read3, &m_state.adc, &ADC::Write3, &m_state.adc);
#if BBCMICRO_DEBUGGER
            this->SetDebugSIO(addr + 0u, &ADC::DebugRead0, &GetDebugMMIOReadADCContext);
            this->SetDebugSIO(addr + 1u, &ADC::DebugRead1, &GetDebugMMIOReadADCContext);
            this->SetDebugSIO(addr + 2u, &ADC::DebugRead2, &GetDebugMMIOReadADCContext);
            this->SetDebugSIO(addr + 3u, &ADC::DebugRead3, &GetDebugMMIOReadADCContext);
#endif
        }
    }

    if (m_state.init_flags & BBCMicroInitFlag_SCSI) {
        ASSERT(!!m_state.scsi);
        this->SetXFJIO(0xfc40, &SCSI::Read0, m_state.scsi.get(), &SCSI::Write0, m_state.scsi.get());
        this->SetXFJIO(0xfc41, &SCSI::Read1, m_state.scsi.get(), &SCSI::Write1, m_state.scsi.get());
        this->SetXFJIO(0xfc42, nullptr, nullptr, &SCSI::Write2, m_state.scsi.get());
        this->SetXFJIO(0xfc43, nullptr, nullptr, &SCSI::Write3, m_state.scsi.get());
    }

    if (m_state.init_flags & BBCMicroInitFlag_MMFS) {
        ASSERT(!!m_state.mmfs);
        if (IsBBCMicro(m_state.type->type_id)) {
            uint16_t addr;
            if (IsMasterSeries(m_state.type->type_id)) {
                addr = 0xfedc;
            } else {
                addr = 0xfe1c;
            }
            this->SetSIO(addr, &MMFS::ReadMMFS, m_state.mmfs.get(), &MMFS::WriteMMFS, m_state.mmfs.get());
        } else {
            this->SetXFJIO(0xfc8c, &MMFS::ReadMMFS, m_state.mmfs.get(), &MMFS::WriteMMFS, m_state.mmfs.get());
        }
    }

    if (m_state.parasite_type != BBCMicroParasiteType_None) {
        m_state.parasite_itu = 0;

        if (m_state.parasite_type == BBCMicroParasiteType_MasterTurbo) {
            if (m_state.type->type_id == BBCMicroTypeID_Master) {
                m_state.parasite_itu = 1;
            }
        }

        if (IsElectron(m_state.type->type_id)) {
            for (uint16_t a = 0xfce0; a < 0xfcf0; a += 8) {
                this->SetXFJIO(a + 0, &ReadHostTube0, &m_state.parasite_tube, &WriteHostTube0Wrapper, this);
                for (uint16_t i = 0; i < 7; ++i) {
                    this->SetXFJIO(a + 1 + i, g_tube_host_read_fns[i], &m_state.parasite_tube, g_tube_host_write_fns[i], &m_state.parasite_tube);
                }
#if BBCMICRO_DEBUGGER
                for (uint16_t i = 0; i < 8; ++i) {
                    this->SetDebugXFJIO(a + i, g_tube_host_debug_read_fns[i], &GetDebugMMIOReadTubeContext);
                }
#endif
            }
        } else {
            for (uint16_t a = 0xfee0; a < 0xff00; a += 8) {
                this->SetSIO(a + 0, &ReadHostTube0, &m_state.parasite_tube, &WriteHostTube0Wrapper, this, !m_state.parasite_itu, !!m_state.parasite_itu);
                for (uint16_t i = 0; i < 7; ++i) {
                    this->SetSIO(a + 1 + i, g_tube_host_read_fns[i], &m_state.parasite_tube, g_tube_host_write_fns[i], &m_state.parasite_tube, !m_state.parasite_itu, !!m_state.parasite_itu);
                }
#if BBCMICRO_DEBUGGER
                for (uint16_t i = 0; i < 8; ++i) {
                    this->SetDebugSIO(a + i, g_tube_host_debug_read_fns[i], &GetDebugMMIOReadTubeContext, !m_state.parasite_itu, !!m_state.parasite_itu);
                }
#endif
            }
        }
    }

    // Set up TST=1 tables.
    m_read_mmios_rom = std::vector<ReadMMIO>(768, {&ReadROMMMIO, this});
    m_mmios_new_run_state_rom = std::vector<BBCMicroCPURunState>(768, BBCMicroCPURunState_Running);

    // Set up TST=0/write tables.
    for (uint8_t flags = 0; flags < 4; ++flags) {
        BBCMicroCPURunState xfj_new_run_state = flags & HostIOFlag_IFJ ? BBCMicroCPURunState_Running : BBCMicroCPURunState_1MHzAccess;

        // FRED = external stretched, internal not
        for (size_t i = 0; i < 0x100; ++i) {
            m_mmios_new_run_state_hw[flags][i] = xfj_new_run_state;
        }

        // JIM = external stretched, internal not
        for (size_t i = 0x100; i < 0x200; ++i) {
            m_mmios_new_run_state_hw[flags][i] = xfj_new_run_state;
        }

        // SHEILA = part stretched (IFJ/XFJ irrelevant)
        for (size_t i = 0x200; i < 0x300; ++i) {
            m_mmios_new_run_state_hw[flags][i] = BBCMicroCPURunState_Running;
        }

        for (const BBCMicroType::SHEILACycleStretchRegion &region : m_state.type->sheila_cycle_stretch_regions) {
            ASSERT(region.first < region.last);
            for (unsigned i = region.first; i <= region.last; ++i) {
                m_mmios_new_run_state_hw[flags][0x200u + i] = BBCMicroCPURunState_1MHzAccess;
            }
        }
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
#endif
    m_state.cpu.context = &m_host_cpu_metadata;

    m_parasite_cpu_metadata.name = "parasite";
#if BBCMICRO_DEBUGGER
    m_parasite_cpu_metadata.dso = BBCMicroDebugStateOverride_Parasite;
#endif
    m_state.parasite_cpu.context = &m_parasite_cpu_metadata;

    m_state.adc.SetHandler(&ReadAnalogueChannel, this);
    m_state.plus1.SetADCHandler(&ReadAnalogueChannel, this);

    // Page in current ROM bank and sort out ACCCON.
    this->InitPaging();

    this->InitDiscDriveSounds(m_state.type->default_disc_drive_type);

#if BBCMICRO_TRACE
    this->SetTrace(nullptr, 0);
#endif

    m_state.rtc.SetNVRAMChangeCallback(&HandleRTCNVRAMChange, this);

    m_state.eeprom.nvram_changed_callback_fn = &HandleEEPROMNVRAMChange;
    m_state.eeprom.nvram_changed_callback_context = this;
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

void BBCMicro::StepOut(int step_rate_ms) {
    int drive;
    if (BBCMicroState::DiscDrive *dd = this->GetDiscDrive(&drive)) {
#if BBCMICRO_TRACE
        uint8_t old_track = dd->track;
#endif
        if (dd->track > 0) {
            --dd->track;

            this->StepSound(dd, step_rate_ms);
        }

#if BBCMICRO_TRACE
        if (m_disk_drive_trace) {
            m_disk_drive_trace->AllocStringf(TraceEventSource_Host, "DiskDrive - Step out drive %d: track was %u, now %u", drive, old_track, dd->track);
        }
#endif
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BBCMicro::StepIn(int step_rate_ms) {
    int drive;
    if (BBCMicroState::DiscDrive *dd = this->GetDiscDrive(&drive)) {
#if BBCMICRO_TRACE
        uint8_t old_track = dd->track;
#endif

        if (dd->track < 255) {
            ++dd->track;

            this->StepSound(dd, step_rate_ms);
        }

#if BBCMICRO_TRACE
        if (m_disk_drive_trace) {
            m_disk_drive_trace->AllocStringf(TraceEventSource_Host, "DiskDrive - Step in drive %d: track was %u, now %u", drive, old_track, dd->track);
        }
#endif
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

BBCMicroState::DiscDrive *BBCMicro::GetDiscDrive(int *drive) {
    if (m_state.disc_control.drive >= 0 && m_state.disc_control.drive < NUM_DRIVES) {
        if (drive) {
            *drive = m_state.disc_control.drive;
        }
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

#define SEEK_SOUND(N)               \
    {                               \
        SOUND_CLOCKS_FROM_MS(N),    \
        DiscDriveSound_Seek##N##ms, \
    }

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

void BBCMicro::StepSound(BBCMicroState::DiscDrive *dd, int step_rate_ms) {
    (void)step_rate_ms;

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

// TODO: the name for this is now completely bogus.
void BBCMicro::UpdateCPUDataBusFn() {
    uint32_t update_flags = 0;

    if (m_state.hack_flags != 0) {
        update_flags |= BBCMicroUpdateFlag_NonFastPath;
    }

#if BBCMICRO_TRACE
    if (m_trace) {
        update_flags |= BBCMicroUpdateFlag_Trace;
    }
#endif

#if BBCMICRO_DEBUGGER
    if (m_debug) {
        if (m_debug->m_step_type == BBCMicroStepType_None) {
            if (m_debug->num_breakpoint_bytes > 0) {
                update_flags |= BBCMicroUpdateFlag_Debug;
            }
        } else {
            ASSERT(m_debug->m_step_cpu);
            update_flags |= BBCMicroUpdateFlag_Debug | BBCMicroUpdateFlag_RareNonFastPath;
        }

        if (((m_debug->hw.system_via_irq_breakpoints.value | m_debug->hw.user_via_irq_breakpoints.value) & 0x7f) != 0) {
            update_flags |= BBCMicroUpdateFlag_Debug;
        }
    }

    if (m_state.ram_and != 0xff || m_state.ram_or != 0x00) {
        update_flags |= BBCMicroUpdateFlag_RareNonFastPath;
    }
#endif

    if (m_host_instruction_callbacks.GetNumCallbacks() > 0) {
        update_flags |= BBCMicroUpdateFlag_NonFastPath;
    }

    if (m_host_write_callbacks.GetNumCallbacks() > 0) {
        update_flags |= BBCMicroUpdateFlag_NonFastPath;
    }

    switch (m_state.type->type_id) {
    default:
        [[fallthrough]];
    case BBCMicroTypeID_B:
        [[fallthrough]];
    case BBCMicroTypeID_BPlus:
        update_flags |= BBCMicroUpdateSystemType_BBCMicro << BBCMicroUpdateFlag_UpdateSystemTypeShift;
        break;

    case BBCMicroTypeID_Master:
        update_flags |= BBCMicroUpdateSystemType_Master128 << BBCMicroUpdateFlag_UpdateSystemTypeShift;
        break;

    case BBCMicroTypeID_MasterCompact:
        update_flags |= BBCMicroUpdateSystemType_MasterCompact << BBCMicroUpdateFlag_UpdateSystemTypeShift;
        break;

    case BBCMicroTypeID_Electron:
        update_flags |= BBCMicroUpdateSystemType_ElectronWithPlus1 << BBCMicroUpdateFlag_UpdateSystemTypeShift;

        if (m_state.electron_ula.misc.bits.motor) {
            update_flags |= BBCMicroUpdateFlag_NonFastPath;
        }
        break;
    }

    if (m_state.parasite_type != BBCMicroParasiteType_None) {
        update_flags |= BBCMicroUpdateFlag_Parasite;

        if (m_state.parasite_type == BBCMicroParasiteType_External3MHz6502) {
            update_flags |= BBCMicroUpdateFlag_Parasite3MHzExternal;
        }

        if (m_state.parasite_boot_mode ||
            m_state.parasite_tube.status.bits.p ||
            m_state.parasite_tube.status.bits.t) {
            update_flags |= BBCMicroUpdateFlag_RareNonFastPath;
        }
    }

    if (m_state.printer_enabled) {
        update_flags |= BBCMicroUpdateFlag_ParallelPrinter;
    }

    if (m_state.init_flags & BBCMicroInitFlag_Mouse) {
        update_flags |= BBCMicroUpdateFlag_Mouse;
    }

    if (m_state.init_flags & BBCMicroInitFlag_Serial) {
        update_flags |= BBCMicroUpdateFlag_Serial;
    }

#if BBCMICRO_DEBUGGER
    this->UpdateUpdateMFnData();
    if (update_flags != m_update_flags) {
        ++m_update_mfn_data->num_update_mfn_changes;
    }

    ++m_update_mfn_data->num_UpdateCpuDataBusFn_calls;
#endif

    update_flags |= (uint32_t)m_state.update_rom_types[m_state.paging.romsel.b_bits.pr] << BBCMicroUpdateFlag_UpdateROMTypeShift;

    ASSERT(update_flags < sizeof ms_update_mfns / sizeof ms_update_mfns[0]);
    m_update_flags = update_flags;
    m_update_mfn = ms_update_mfns[update_flags];
    ASSERT(m_update_mfn);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BBCMicro::GetKeyColumnAndMask(BeebKey key, uint8_t **column_ptr, uint8_t *mask_ptr) {
    ASSERT(key >= 0);

    int8_t code = (int8_t)key;

    if (IsElectron(m_state.type->type_id)) {
        code = GetElectronKeyFromBeebKey(key);
        if (code < 0) {
            *column_ptr = nullptr;
            *mask_ptr = 0;
            return;
        }
    }

    *column_ptr = &m_state.key_columns[code & 0xf];
    *mask_ptr = 1 << (code >> 4);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static bool IsAddressInScopeForFlags(uint16_t addr, uint8_t scope, uint8_t host_io_flags) {
    ASSERT(addr >= IO_BEGIN_ADDRESS.w && addr < IO_END_ADDRESS.w);

    if (addr >= FJ_IO_BEGIN_ADDRESS.w && addr < FJ_IO_END_ADDRESS.w) {
        if (host_io_flags & HostIOFlag_IFJ) {
            if (scope & BBCMicroMMIOScopeFlag_IFJ) {
                return true;
            }
        } else {
            if (scope & BBCMicroMMIOScopeFlag_XFJ) {
                return true;
            }
        }
    } else {
        if (host_io_flags & HostIOFlag_ITU) {
            if (scope & BBCMicroMMIOScopeFlag_ITU) {
                return true;
            }
        } else {
            if (scope & BBCMicroMMIOScopeFlag_XTU) {
                return true;
            }
        }
    }

    return false;
}

void BBCMicro::SetMMIOFnsInternal(uint16_t addr, ReadMMIOFn read_fn, void *read_context, WriteMMIOFn write_fn, void *write_context, uint8_t scope) {
    ASSERT(scope != 0);
    ASSERT(addr >= IO_BEGIN_ADDRESS.w && addr < IO_END_ADDRESS.w);

    uint16_t index = addr - IO_BEGIN_ADDRESS.w;

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

    for (uint8_t host_io_flags = 0; host_io_flags < 4; ++host_io_flags) {
        if (IsAddressInScopeForFlags(addr, scope, host_io_flags)) {
            m_write_mmios_hw[host_io_flags][index] = write_mmio;
            m_read_mmios_hw[host_io_flags][index] = read_mmio;
        }
    }
}

#if BBCMICRO_DEBUGGER
void BBCMicro::SetDebugMMIOFnsInternal(uint16_t addr, DebugReadMMIOFn debug_read_fn, DebugGetReadMMIOContextFn debug_get_context_fn, uint8_t scope) {
    ASSERT(scope != 0);
    ASSERT(addr >= IO_BEGIN_ADDRESS.w && addr < IO_END_ADDRESS.w);

    DebugReadMMIO debug_read_mmio;
    debug_read_mmio.set = true;
    debug_read_mmio.fn = debug_read_fn;
    debug_read_mmio.context_fn = debug_get_context_fn;

    uint16_t index = addr - IO_BEGIN_ADDRESS.w;

    for (uint8_t host_io_flags = 0; host_io_flags < 4; ++host_io_flags) {
        if (IsAddressInScopeForFlags(addr, scope, host_io_flags)) {
            m_debug_read_mmio_data->debug_read_mmios[host_io_flags][index] = debug_read_mmio;
        }
    }
}
#endif

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
        ASSERT(m_update_flags < NUM_BBCMICRO_UPDATE_MFNS);
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

#if BBCMICRO_DEBUGGER
uint8_t *BBCMicro::DebugGetByteDebugFlags(const BigPageMetadata *metadata,
                                          uint8_t *byte_debug_flags,
                                          uint8_t *const *read_io_byte_debug_flags,
                                          uint8_t *const *write_io_byte_debug_flags,
                                          M6502Word offset,
                                          bool write) {
    if (!(metadata->host_io_flags & HostIOFlag_NoIO)) {
        if (offset.p.o >= IO_BEGIN_ADDRESS.p.o && offset.p.o < IO_END_ADDRESS.p.o) {
            uint8_t *flags;
            if (write) {
                flags = write_io_byte_debug_flags[offset.io.r];
            } else {
                flags = read_io_byte_debug_flags[offset.io.r];
            }

            if (flags) {
                return &flags[offset.io.o];
            } else {
                return nullptr;
            }
        }
    }

    if (byte_debug_flags) {
        return &byte_debug_flags[offset.p.o];
    } else {
        return nullptr;
    }
}
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if BBCMICRO_DEBUGGER
void BBCMicro::DebugModifyByteDebugFlags(BigPageIndex big_page_index, M6502Word offset, bool write, uint8_t clear_flags, uint8_t set_flags) {
    ASSERT(big_page_index.i < NUM_BIG_PAGES);
    BigPage *big_page = &m_big_pages[big_page_index.i];

    if (uint8_t *byte_flags = DebugGetByteDebugFlags(big_page->metadata,
                                                     big_page->byte_debug_flags,
                                                     big_page->read_io_byte_debug_flags,
                                                     big_page->write_io_byte_debug_flags,
                                                     offset,
                                                     write)) {
        uint8_t new_flags = (*byte_flags & ~clear_flags) | set_flags;

        if (*byte_flags != new_flags) {
            if (*byte_flags == 0) {
                ++m_debug->num_breakpoint_bytes;
            } else if (new_flags == 0) {
                ASSERT(m_debug->num_breakpoint_bytes > 0);
                --m_debug->num_breakpoint_bytes;
            }

            *byte_flags = new_flags;

            m_debug->BreakpointsDidChange();

            if (new_flags & BBCMicroByteDebugFlag_TempBreakExecute) {
                ASSERT((uintptr_t)byte_flags >= (uintptr_t)m_debug->m_debug_flags &&
                       (uintptr_t)byte_flags < (uintptr_t)m_debug->m_debug_flags + sizeof m_debug->m_debug_flags);
                m_debug->m_temp_execute_breakpoints.push_back((uint32_t)(byte_flags - m_debug->m_debug_flags));
            }
        }

        this->UpdateCPUDataBusFn();
    }
}
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BBCMicro::HandleRTCNVRAMChange(void *context) {
    auto m = (BBCMicro *)context;

    if (m->m_nvram_changed_callback_fn) {
        (*m->m_nvram_changed_callback_fn)(m, m->m_nvram_changed_callback_context);
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BBCMicro::HandleEEPROMNVRAMChange(void *context) {
    HandleRTCNVRAMChange(context); //currently, they're the same...
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void BBCMicro::EnsureUpdateMFnsTableIsReady() {
    if (!ms_update_mfns[0]) {
        size_t group_idx = 0;
        size_t group_fn_idx = 0;
        for (size_t i = 0; i < NUM_BBCMICRO_UPDATE_MFNS; ++i) {
            ms_update_mfns[i] = ms_update_mfn_groups[group_idx][group_fn_idx];

            ++group_idx;
            if (!ms_update_mfn_groups[group_idx]) {
                group_idx = 0;
                ++group_fn_idx;
            }
        }
    }

    for (uint32_t i = 0; i < NUM_BBCMICRO_UPDATE_MFNS; ++i) {
        ASSERT(ms_update_mfns[i]);
        ASSERT(ms_update_mfns[i] == ms_update_mfns[BBCMicro::GetNormalizedBBCMicroUpdateFlags(i)]);
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if BBCMICRO_DEBUGGER
const void *BBCMicro::GetDebugMMIOReadSystemVIAContext(const BBCMicroReadOnlyState *state) {
    return &state->system_via;
}
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if BBCMICRO_DEBUGGER
const void *BBCMicro::GetDebugMMIOReadUserVIAContext(const BBCMicroReadOnlyState *state) {
    return &state->user_via;
}
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if BBCMICRO_DEBUGGER
const void *BBCMicro::GetDebugMMIOReadFDCContext(const BBCMicroReadOnlyState *state) {
    return &state->fdc;
}
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if BBCMICRO_DEBUGGER
const void *BBCMicro::GetDebugMMIORead1770ControlRegisterContext(const BBCMicroReadOnlyState *state) {
    return state;
}
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if BBCMICRO_DEBUGGER
const void *BBCMicro::GetDebugMMIOReadROMSELContext(const BBCMicroReadOnlyState *state) {
    return state;
}
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if BBCMICRO_DEBUGGER
const void *BBCMicro::GetDebugMMIOReadACCCONContext(const BBCMicroReadOnlyState *state) {
    return state;
}
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if BBCMICRO_DEBUGGER
const void *BBCMicro::GetDebugMMIOReadTubeContext(const BBCMicroReadOnlyState *state) {
    return &state->parasite_tube;
}
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if BBCMICRO_DEBUGGER
const void *BBCMicro::GetDebugMMIOReadADCContext(const BBCMicroReadOnlyState *state) {
    return &state->adc;
}
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if BBCMICRO_DEBUGGER
const void *BBCMicro::GetDebugMMIOReadACIAContext(const BBCMicroReadOnlyState *state) {
    return &state->acia;
}
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if BBCMICRO_DEBUGGER
const void *BBCMicro::GetDebugMMIOReadADJIContext(const BBCMicroReadOnlyState *state) {
    return &state->digital_joystick_state;
}
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if BBCMICRO_DEBUGGER
const void *BBCMicro::GetDebugMMIOReadPlus1Context(const BBCMicroReadOnlyState *state) {
    return &state->plus1;
}
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if BBCMICRO_DEBUGGER
uint8_t *BBCMicro::GetByteDebugFlagsForBigPage(const BigPageMetadata *metadata, BBCMicroDebugState *debug) {
    if (debug) {
        if (metadata->addr != BigPageMetadata::INVALID_ADDR) {
            return &debug->m_debug_flags[BBCMicroDebugState::BIG_PAGES_BYTE_DEBUG_FLAGS_INDEX + metadata->debug_flags_index.i * BIG_PAGE_SIZE_BYTES];
        }
    }

    return nullptr;
}
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if BBCMICRO_DEBUGGER
uint8_t *BBCMicro::GetAddressDebugFlagsForBigPage(const BigPageMetadata *metadata, BBCMicroDebugState *debug) {
    if (debug) {
        if (metadata->addr != BigPageMetadata::INVALID_ADDR) {
            if (metadata->is_parasite) {
                return &debug->m_debug_flags[BBCMicroDebugState::PARASITE_ADDRESS_DEBUG_FLAGS_INDEX + metadata->addr];
            } else {
                return &debug->m_debug_flags[BBCMicroDebugState::HOST_ADDRESS_DEBUG_FLAGS_INDEX + metadata->addr];
            }
        }
    }

    return nullptr;
}
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if BBCMICRO_DEBUGGER
void BBCMicro::GetIOByteDebugFlagsForBigPage(uint8_t **read_io_debug_flags, uint8_t **write_io_debug_flags, const BigPageMetadata *metadata, BBCMicroDebugState *debug) {
    if (debug) {
        if (metadata->addr != BigPageMetadata::INVALID_ADDR) {
            if (!(metadata->host_io_flags & HostIOFlag_NoIO)) {
                // TODO: "region" is the wrong name for these values
                if (metadata->host_io_flags & HostIOFlag_IFJ) {
                    for (uint8_t region = 0; region < 16; ++region) {
                        write_io_debug_flags[region] = &debug->m_debug_flags[BBCMicroDebugState::IO_BYTE_DEBUG_FLAGS_INDEX +
                                                                             (BBCMicroIOByteDebugFlagRegion_IFJ + region) * BBCMicroDebugState::IO_BYTE_DEBUG_FLAG_REGION_SIZE_BYTES];
                    }
                } else {
                    for (uint8_t region = 0; region < 16; ++region) {
                        write_io_debug_flags[region] = &debug->m_debug_flags[BBCMicroDebugState::IO_BYTE_DEBUG_FLAGS_INDEX +
                                                                             (BBCMicroIOByteDebugFlagRegion_XFJ + region) * BBCMicroDebugState::IO_BYTE_DEBUG_FLAG_REGION_SIZE_BYTES];
                    }
                }

                for (uint8_t region = 0; region < 7; ++region) {
                    write_io_debug_flags[16 + region] = &debug->m_debug_flags[BBCMicroDebugState::IO_BYTE_DEBUG_FLAGS_INDEX +
                                                                              (BBCMicroIOByteDebugFlagRegion_S_XTU + region) * BBCMicroDebugState::IO_BYTE_DEBUG_FLAG_REGION_SIZE_BYTES];
                }

                if (metadata->host_io_flags & HostIOFlag_ITU) {
                    write_io_debug_flags[23] = &debug->m_debug_flags[BBCMicroDebugState::IO_BYTE_DEBUG_FLAGS_INDEX + BBCMicroIOByteDebugFlagRegion_S_ITU * BBCMicroDebugState::IO_BYTE_DEBUG_FLAG_REGION_SIZE_BYTES];
                } else {
                    write_io_debug_flags[23] = &debug->m_debug_flags[BBCMicroDebugState::IO_BYTE_DEBUG_FLAGS_INDEX +
                                                                     (BBCMicroIOByteDebugFlagRegion_S_XTU + 7) * BBCMicroDebugState::IO_BYTE_DEBUG_FLAG_REGION_SIZE_BYTES];
                }

                if (metadata->host_io_flags & HostIOFlag_TST) {
                    for (uint8_t region = 0; region < 24; ++region) {
                        read_io_debug_flags[region] = &debug->m_debug_flags[(BBCMicroDebugState::BIG_PAGES_BYTE_DEBUG_FLAGS_INDEX + metadata->debug_flags_index.i) * BIG_PAGE_SIZE_BYTES] + IO_BEGIN_ADDRESS.p.o + region * BBCMicroDebugState::IO_BYTE_DEBUG_FLAG_REGION_SIZE_BYTES;
                    }
                } else {
                    for (uint8_t region = 0; region < 24; ++region) {
                        read_io_debug_flags[region] = write_io_debug_flags[region];
                    }
                }

                return;
            }
        }
    }

    for (uint8_t region = 0; region < 24; ++region) {
        read_io_debug_flags[region] = nullptr;
        write_io_debug_flags[region] = nullptr;
    }
}
#endif
