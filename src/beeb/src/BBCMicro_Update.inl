#include <shared/system.h>
#include <shared/debug.h>
#include <beeb/BBCMicro.h>
#include <beeb/BeebLink.h>
#include <beeb/sound.h>
#include <beeb/Trace.h>

#include <shared/enum_decl.h>
#include "BBCMicro_private.inl"
#include <shared/enum_end.h>

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// Separate source file for the templated update function, as there are a lot of
// instantiations - some would say too many - and it can take a while to
// compile.

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// The mouse can interrupt the BBC as fast as you can move it, so strictly
// speaking there isn't really a clock. But having one ensures that IRQs can't
// come too rapidly. Also easy to do the mouse update as part of the sound
// update, so only check every 16th update.
//
// Fastest I could get a Quest Mouse to interrupt the BBC was ~700 Hz. So
// 2e6/2048 = ~975 Hz seems reasonable.
static constexpr uint64_t SHIFT_CONSTANTS(2MHZ, MOUSE_CLOCK, 11);
static constexpr uint64_t SHIFT_CONSTANTS(CYCLE_COUNT, MOUSE_CLOCK, LSHIFT_MOUSE_CLOCK_TO_2MHZ + LSHIFT_2MHZ_TO_CYCLE_COUNT);

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#ifdef BBCMICRO_UPDATE_DEFINE_FUNCTIONS
uint64_t Get3MHzCycleCount(CycleCount n) {
    uint64_t n_4mhz = n.n >> RSHIFT_CYCLE_COUNT_TO_4MHZ;
    uint64_t result = n.n / 4 * 3;

    uint64_t rem = n_4mhz & 3;
    if (rem > 0) {
        result += rem - 1;
    }

    return result;
}
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if ENABLE_ELECTRON
static constexpr bool IsElectronUpdate(uint32_t update_flags) {
    return GetBBCMicroUpdateFlagsUpdateSystemType(update_flags) == BBCMicroUpdateSystemType_ElectronWithPlus1;
}
#endif

static constexpr bool IsBBCMicroUpdate(uint32_t update_flags) {
#if ENABLE_ELECTRON
    return !IsElectronUpdate(update_flags);
#else
    (void)update_flags;
    return true;
#endif
}

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
template <uint32_t UPDATE_FLAGS>
uint32_t BBCMicro::UpdateTemplated(VideoDataUnit *video_unit, SoundDataUnit *sound_unit) {
    static_assert(CYCLES_PER_SECOND == 4000000, "BBCMicro::Update needs updating");

    uint8_t phi2_2MHz_trailing_edge = m_state.cycle_count.n & 1;
    uint8_t phi2_1MHz_trailing_edge = m_state.cycle_count.n & 2;
    uint32_t result = 0;

    if constexpr ((UPDATE_FLAGS & BBCMicroUpdateFlag_Parasite) != 0) {
        if constexpr ((UPDATE_FLAGS & BBCMicroUpdateFlag_Parasite3MHzExternal) != 0) {
            // When running in 3 MHz mode, just cheekily skip every 4th update.
            //
            // If tweaking this logic, update Get3MHzCycleCount, conveniently
            // also located in this file.
            if ((m_state.cycle_count.n & 3) == 0) {
                goto parasite_update_done;
            }
        }

        result |= BBCMicroUpdateResultFlag_Parasite;
        (*m_state.parasite_cpu.tfn)(&m_state.parasite_cpu);

        if constexpr ((UPDATE_FLAGS & BBCMicroUpdateFlag_RareNonFastPath) != 0) {
            if (m_state.parasite_tube.status.bits.t) {
                ResetTube(&m_state.parasite_tube);
            }

            if (m_state.parasite_tube.status.bits.p) {
                M6502_Reset(&m_state.parasite_cpu);
                m_state.parasite_boot_mode = true;
                this->UpdateCPUDataBusFn();
            }
        }

        M6502_SetDeviceIRQ(&m_state.cpu, BBCMicroIRQDevice_HostTube, m_state.parasite_accessible && m_state.parasite_tube.hirq.bits.hirq);
        M6502_SetDeviceIRQ(&m_state.parasite_cpu, BBCMicroIRQDevice_ParasiteTube, m_state.parasite_tube.pirq.bits.pirq);
        M6502_SetDeviceNMI(&m_state.parasite_cpu, BBCMicroNMIDevice_ParasiteTube, m_state.parasite_tube.pirq.bits.pnmi);

        if (m_state.parasite_cpu.read) {
            if ((m_state.parasite_cpu.abus.w & 0xfff0) == 0xfef0) {
                m_state.parasite_cpu.dbus = (*m_parasite_read_mmio_fns[m_state.parasite_cpu.abus.w & 7])(&m_state.parasite_tube, m_state.parasite_cpu.abus);

                // This bit is a bit careless about checking for the `Trace`
                // flag, but that's only an efficiency issue, not important for
                // parasite special modes.
                if constexpr ((UPDATE_FLAGS & BBCMicroUpdateFlag_RareNonFastPath) != 0) {
                    if (m_state.parasite_boot_mode) {
#if BBCMICRO_TRACE
                        if (m_trace) {
                            m_trace->AllocParasiteBootModeEvent(false);
                        }
#endif
                        m_state.parasite_boot_mode = false;
                        this->UpdateCPUDataBusFn();
                    }
                }
            } else {
                if constexpr ((UPDATE_FLAGS & BBCMicroUpdateFlag_RareNonFastPath) != 0) {
                    if (m_state.parasite_boot_mode && (m_state.parasite_cpu.abus.w & 0xf000) == 0xf000) {
                        // Really not concerned about the efficiency of special
                        // mode. The emulator is not in this state for long.
                        if (!m_state.parasite_rom_buffer) {
                            m_state.parasite_cpu.dbus = 0;
                        } else {
                            m_state.parasite_cpu.dbus = m_state.parasite_rom_buffer->at(m_state.parasite_cpu.abus.w & 0xfff);
                        }
                    } else {
                        m_state.parasite_cpu.dbus = m_parasite_ram[m_state.parasite_cpu.abus.w];
                    }
                } else {
                    m_state.parasite_cpu.dbus = m_parasite_ram[m_state.parasite_cpu.abus.w];
                }
            }

#if BBCMICRO_DEBUGGER
            if constexpr ((UPDATE_FLAGS & BBCMicroUpdateFlag_Debug) != 0) {
                // The parasite paging is uncomplicated, and the byte address
                // flags can be treated as a single 64 KB array.
                uint8_t flags = (m_debug->m_debug_flags[BBCMicroDebugState::PARASITE_ADDRESS_DEBUG_FLAGS_INDEX +
                                                        m_state.parasite_cpu.abus.w] |
                                 m_debug->m_debug_flags[BBCMicroDebugState::BIG_PAGES_BYTE_DEBUG_FLAGS_INDEX +
                                                        PARASITE_BIG_PAGE_INDEX.i * BIG_PAGE_SIZE_BYTES +
                                                        m_state.parasite_cpu.abus.w]);

                if constexpr ((UPDATE_FLAGS & BBCMicroUpdateFlag_RareNonFastPath) != 0) {
                    // Really not concerned about the efficiency of special
                    // mode. The emulator is not in this state for long.
                    if (m_state.parasite_boot_mode && (m_state.parasite_cpu.abus.w & 0xf000) == 0xf000) {
                        flags = (m_debug->m_debug_flags[BBCMicroDebugState::PARASITE_ADDRESS_DEBUG_FLAGS_INDEX +
                                                        m_state.parasite_cpu.abus.w] |
                                 m_debug->m_debug_flags[BBCMicroDebugState::BIG_PAGES_BYTE_DEBUG_FLAGS_INDEX +
                                                        PARASITE_ROM_BIG_PAGE_INDEX.i * BIG_PAGE_SIZE_BYTES +
                                                        m_state.parasite_cpu.abus.p.o]);
                    }
                }

                if (flags & BBCMicroByteDebugFlag_AnyBreakReadMask) {
                    this->DebugHitBreakpoint(&m_state.parasite_cpu, &m_debug->m_parasite_relative_base, flags);
                }
            }
#endif
        } else {
            if ((m_state.parasite_cpu.abus.w & 0xfff0) == 0xfef0) {
                (*m_parasite_write_mmio_fns[m_state.parasite_cpu.abus.w & 7])(&m_state.parasite_tube, m_state.parasite_cpu.abus, m_state.parasite_cpu.dbus);
                if constexpr ((UPDATE_FLAGS & BBCMicroUpdateFlag_RareNonFastPath) != 0) {
                    if (m_state.parasite_boot_mode) {
#if BBCMICRO_TRACE
                        if (m_trace) {
                            m_trace->AllocParasiteBootModeEvent(false);
                        }
#endif
                        m_state.parasite_boot_mode = false;
                        this->UpdateCPUDataBusFn();
                    }
                }
            } else {
                m_parasite_ram[m_state.parasite_cpu.abus.w] = m_state.parasite_cpu.dbus;
            }

#if BBCMICRO_DEBUGGER
            if constexpr ((UPDATE_FLAGS & BBCMicroUpdateFlag_Debug) != 0) {
                // The parasite paging is uncomplicated, and the byte address
                // flags can be treated as a single 64 KB array.
                uint8_t flags = (m_debug->m_debug_flags[BBCMicroDebugState::PARASITE_ADDRESS_DEBUG_FLAGS_INDEX +
                                                        m_state.parasite_cpu.abus.w] |
                                 m_debug->m_debug_flags[BBCMicroDebugState::BIG_PAGES_BYTE_DEBUG_FLAGS_INDEX +
                                                        PARASITE_BIG_PAGE_INDEX.i * BIG_PAGE_SIZE_BYTES +
                                                        m_state.parasite_cpu.abus.w]);
                if (flags & BBCMicroByteDebugFlag_AnyBreakWriteMask) {
                    this->DebugHitBreakpoint(&m_state.parasite_cpu, &m_debug->m_parasite_relative_base, flags);
                }
            }
#endif
        }

        if constexpr ((UPDATE_FLAGS & BBCMicroUpdateFlag_Trace) != 0) {
#if BBCMICRO_TRACE
            if (M6502_IsAboutToExecute(&m_state.parasite_cpu)) {
                if (m_trace) {
                    InstructionTraceEvent *e;

                    if ((e = m_trace_parasite_current_instruction) != nullptr) {
                        e->a = m_state.parasite_cpu.a;
                        e->x = m_state.parasite_cpu.x;
                        e->y = m_state.parasite_cpu.y;
                        e->p = m_state.parasite_cpu.p.value;
                        e->data = m_state.parasite_cpu.data;
                        e->opcode = m_state.parasite_cpu.opcode;
                        e->s = m_state.parasite_cpu.s.b.l;
                        //e->pc=m_state.parasite_cpu.pc.w;//...for next instruction
                        e->ad = m_state.parasite_cpu.ad.w;
                        e->ia = m_state.parasite_cpu.ia.w;
                    }

                    e = m_trace_parasite_current_instruction = (InstructionTraceEvent *)m_trace->AllocEvent(INSTRUCTION_EVENT, TraceEventSource_Parasite);

                    if (e) {
                        e->pc = m_state.parasite_cpu.abus.w;
                    }
                }
            }
#endif
        }

        if constexpr ((UPDATE_FLAGS & BBCMicroUpdateFlag_RareNonFastPath) != 0) {
#if BBCMICRO_DEBUGGER
            if (m_debug) {
                if (m_debug->m_step_cpu == &m_state.parasite_cpu) {
                    this->DebugHandleStep();
                }
            }
#endif
        }
    }

parasite_update_done:

    if constexpr ((UPDATE_FLAGS & BBCMicroUpdateFlag_Serial) != 0) {
        static_assert(CYCLES_PER_SECOND == 4000000, "BBCMicro::Update needs updating");

        //if (m_state.serproc_update_counter-- == 0) {

        //    m_state.serproc_update_counter = 13;
        //}

        if (m_state.cycle_count.n % 13 == 0) {
            m_state.serproc.Update();
        }
    }

    if (!phi2_2MHz_trailing_edge) {
#if VIDEO_TRACK_METADATA
        video_unit->metadata.flags = 0;

        if (phi2_1MHz_trailing_edge) {
            video_unit->metadata.flags |= VideoDataUnitMetadataFlag_OddCycle;
        }
#endif

        // Update CPU.
#if ENABLE_ELECTRON
        if constexpr (IsElectronUpdate(UPDATE_FLAGS)) {
            if (m_state.cpu_run_state == BBCMicroCPURunState_Running) {
                result |= BBCMicroUpdateResultFlag_Host;
                (*m_state.cpu.tfn)(&m_state.cpu);

                if (m_state.cpu.abus.w < 0x8000) {
                    // RAM access. ULA mediates.
                    m_state.cpu_run_state = BBCMicroCPURunState_RAMAccess;
                } else {
                    // TODO: keyboard ROM bank access needs to induce the right state

                    // possibly select 1MHzAccess state.
                    M6502Word mmio_addr = {(uint16_t)(m_state.cpu.abus.w - IO_BEGIN_ADDRESS.w)};
                    if (mmio_addr.b.h < 3) {
                        if (m_state.cpu.read) {
                            m_state.cpu_run_state = m_read_mmios_new_run_state[mmio_addr.w];
                        } else {
                            m_state.cpu_run_state = m_write_mmios_new_run_state[mmio_addr.w];
                        }
                    }
                }
            }
        } else //<--note
#endif         //<--note
        {
            if (m_state.cpu_run_state != BBCMicroCPURunState_Running) {
                // The only non-Running state is the 1 MHz cycle stretch case.
                if (phi2_1MHz_trailing_edge) {
                    m_state.cpu_run_state = BBCMicroCPURunState_Running;
                }
            } else {
                result |= BBCMicroUpdateResultFlag_Host;
                (*m_state.cpu.tfn)(&m_state.cpu);

                M6502Word mmio_addr = {(uint16_t)(m_state.cpu.abus.w - IO_BEGIN_ADDRESS.w)};
                if (mmio_addr.b.h < 3) {
                    if (m_state.cpu.read) {
                        m_state.cpu_run_state = m_read_mmios_new_run_state[mmio_addr.w];
                    } else {
                        m_state.cpu_run_state = m_write_mmios_new_run_state[mmio_addr.w];
                    }
                }
            }
        }

#if ENABLE_ELECTRON
        if constexpr (IsElectronUpdate(UPDATE_FLAGS)) {
            uint8_t ula_used_cycle = !phi2_1MHz_trailing_edge;

            // TODO: not sure any of this is the right logic?

            if (m_state.electron_ula.rtc_interrupt_timer > 0) {
                --m_state.electron_ula.rtc_interrupt_timer;
                if (m_state.electron_ula.rtc_interrupt_timer == 0) {
                    m_state.electron_ula.irq.bits.rtc = 1;

                    TRACEF(m_trace, "Electron ULA - RTC");
                }
            }

            if (m_state.electron_ula.display_column < ElectronULA::NUM_HSYNC_COLUMNS) {
                // Horizontal sync
                video_unit->pixels.values[1] = video_unit->pixels.values[0] = 0;
                video_unit->pixels.pixels[1].bits.x |= VideoDataUnitFlag_HSync;
            } else if (m_state.electron_ula.display_column < ElectronULA::NUM_HSYNC_COLUMNS + ElectronULA::NUM_BACK_PORCH_COLUMNS) {
                // Back porch
                video_unit->pixels.values[1] = video_unit->pixels.values[0] = 0;
            } else if (m_state.electron_ula.display_column < ElectronULA::NUM_HSYNC_COLUMNS + ElectronULA::NUM_BACK_PORCH_COLUMNS + ElectronULA::NUM_DISPLAY_COLUMNS) {
                // Display area
                if (m_state.electron_ula.display_state == ElectronULADisplayState_Display && m_state.electron_ula.display_raster < 8) {
                    // Display data from RAM.
                    if (m_state.electron_ula.misc.bits.display_mode <= 3 || ula_used_cycle) {
                        if (m_state.electron_ula.display_fetch_address >= 0x8000) {
                            m_state.electron_ula.display_fetch_address -= ElectronULA::DISPLAY_WRAPAROUND_SIZES[m_state.electron_ula.misc.bits.display_mode];
                        }

                        m_state.electron_ula.display_byte = m_ram[m_state.electron_ula.display_fetch_address];
#if VIDEO_TRACK_METADATA
                        m_state.electron_ula.display_fetched_byte = m_state.electron_ula.display_byte;
                        m_state.electron_ula.display_byte_address = m_state.electron_ula.display_fetch_address;

#endif
                        m_state.electron_ula.display_fetch_address += 8;

                        // the ULA steals every cycle in the mode 0-3 case.
                        ula_used_cycle = true;
                    }

                    (*ElectronULA::EMIT_PIXELS_FNS[m_state.electron_ula.misc.bits.display_mode])(video_unit, &m_state.electron_ula);

#if VIDEO_TRACK_METADATA
                    video_unit->metadata.flags |= VideoDataUnitMetadataFlag_HasAddress | VideoDataUnitMetadataFlag_HasValue | VideoDataUnitMetadataFlag_6845DISPEN;
                    video_unit->metadata.address = m_state.electron_ula.display_byte_address;
                    video_unit->metadata.value = m_state.electron_ula.display_fetched_byte;
#endif
                } else {
                    // Display nothing.
                    //
                    // In a text mode, there are always 2 non-display
                    // scanlines at the end of the display area. They count
                    // as display too.
                    video_unit->pixels.values[1] = video_unit->pixels.values[0] = 0;
                }
            } else {
                // Front porch
                video_unit->pixels.values[1] = video_unit->pixels.values[0] = 0;
            }

            result |= BBCMicroUpdateResultFlag_VideoUnit;

            ++m_state.electron_ula.display_column;
            if (m_state.electron_ula.display_column == 128) {
                // handle vertical counters in the visible region only. The
                // non-visible parts are dealt with by timers.
                if (m_state.electron_ula.display_state == ElectronULADisplayState_Display) {
                    ++m_state.electron_ula.display_scanline;
                    ++m_state.electron_ula.display_raster;

                    //m_state.electron_ula.display_fetched_byte = 0;

                    bool is_graphics = ElectronULA::IS_GRAPHICS_MODE[m_state.electron_ula.misc.bits.display_mode];

                    if (m_state.electron_ula.display_raster >= ElectronULA::NUM_RASTERS[is_graphics]) {
                        m_state.electron_ula.display_raster = 0;
                        ++m_state.electron_ula.display_row;
                        m_state.electron_ula.display_row_address += ElectronULA::DISPLAY_ROW_STRIDES[m_state.electron_ula.misc.bits.display_mode];

                        if (m_state.electron_ula.display_row >= ElectronULA::NUM_ROWS[is_graphics]) {
                            m_state.electron_ula.display_state = ElectronULADisplayState_BeforeVSync;

                            // do the display end interrupt.
                            m_state.electron_ula.irq.bits.display_end = 1;

                            TRACEF(m_trace, "Electron ULA - Display End");

                            // Queue up the vsync.
                            //
                            // Add 1 to the count, because the vsync counter
                            // handling is the next step, so the counter will
                            // get immediately decremented.
                            m_state.electron_ula.display_vsync_counter = (ElectronULA::VSYNC_SCANLINE - m_state.electron_ula.display_scanline) * 128 + 1;
                            //if (m_state.electron_ula.display_even_field) {
                            //m_state.electron_ula.display_vsync_counter += 64;
                            //}

                            m_state.electron_ula.display_state = ElectronULADisplayState_BeforeVSync;
                        }
                    }

                    m_state.electron_ula.display_fetch_address = m_state.electron_ula.display_row_address + m_state.electron_ula.display_raster;
                }

                // Always keep on top of the horizontal counter.
                m_state.electron_ula.display_column = 0;
            }

            if (m_state.electron_ula.display_vsync_counter > 0) {
                if (m_state.electron_ula.display_state == ElectronULADisplayState_VSync) {
                    video_unit->pixels.pixels[1].bits.x |= VideoDataUnitFlag_VSync;
                }

                --m_state.electron_ula.display_vsync_counter;
                if (m_state.electron_ula.display_vsync_counter == 0) {
                    switch (m_state.electron_ula.display_state) {
                    default:
                        ASSERT(false);
                        break;

                    case ElectronULADisplayState_BeforeVSync:
                        m_state.electron_ula.display_state = ElectronULADisplayState_VSync;
                        m_state.electron_ula.display_vsync_counter = 2 * 160;
                        break;

                    case ElectronULADisplayState_VSync:
                        m_state.electron_ula.display_state = ElectronULADisplayState_AfterVSync;
                        m_state.electron_ula.rtc_interrupt_timer = 2 * 8192;
                        m_state.electron_ula.display_vsync_counter = (312 - (ElectronULA::VSYNC_SCANLINE + 2)) * 128;
                        //m_state.electron_ula.display_vsync_counter += 64;
                        break;

                    case ElectronULADisplayState_AfterVSync:
                        m_state.electron_ula.display_even_field = !m_state.electron_ula.display_even_field;
                        m_state.electron_ula.display_scanline = 0;
                        m_state.electron_ula.display_column = 0;
                        m_state.electron_ula.display_row = 0;
                        m_state.electron_ula.display_raster = 0;
                        m_state.electron_ula.display_state = ElectronULADisplayState_Display;
                        m_state.electron_ula.display_row_address = m_state.electron_ula.display_start_address;
                        m_state.electron_ula.display_fetch_address = m_state.electron_ula.display_row_address;
                        break;
                    }
                }
            }

            m_state.plus1.Update(m_printer_buffer);

            M6502_SetDeviceIRQ(&m_state.cpu, BBCMicroIRQDevice_ElectronULA, m_state.electron_ula.irq.flag_bits.flags & m_state.electron_ula.irq_mask.flag_bits.flags);

            // Unblock the CPU when possible.
            //
            // TODO: this is probably not the right logic...
            if (!ula_used_cycle) {
                if (m_state.cpu_run_state == BBCMicroCPURunState_RAMAccess) {
                    m_state.cpu_run_state = BBCMicroCPURunState_Running;
                }
            }

            if (phi2_1MHz_trailing_edge) {
                if (m_state.cpu_run_state == BBCMicroCPURunState_1MHzAccess) {
                    m_state.cpu_run_state = BBCMicroCPURunState_Running;
                }
            }
        }
#endif

        if (m_state.cpu_run_state == BBCMicroCPURunState_Running) {
            // Update CPU data bus.
            if constexpr ((UPDATE_FLAGS & BBCMicroUpdateFlag_NonFastPath) != 0) {
                if (m_state.cpu.read == 0) {
                    if (Callbacks<WriteFn>::Callback *callback = m_host_write_callbacks.begin) {
                        bool any_removed = false;

                        while (callback != m_host_write_callbacks.end) {
                            if (!(*callback->fn)(this, &m_state.cpu, callback->fn_context)) {
                                any_removed = true;
                                callback->fn = nullptr;
                            }

                            ++callback;
                        }

                        if (any_removed) {
                            m_host_write_callbacks.DidChange();
                            this->CallbacksDidChange();
                        }
                    }
                }
            }

            if constexpr (GetBBCMicroUpdateFlagsUpdateROMType(UPDATE_FLAGS) == BBCMicroUpdateROMType_16KB ||
                          GetBBCMicroUpdateFlagsUpdateROMType(UPDATE_FLAGS) == BBCMicroUpdateROMType_EmptySocket) {
                // nothing extra to do here
            } else if constexpr (GetBBCMicroUpdateFlagsUpdateROMType(UPDATE_FLAGS) == BBCMicroUpdateROMType_CCIWORD) {
                switch (m_state.cpu.abus.w & 0xffe0) {
                case 0x8060:
                case 0xbfc0:
                    this->UpdateMapperRegion(0);
                    break;

                case 0x8040:
                case 0xbfa0:
                case 0xbfe0:
                    this->UpdateMapperRegion(1);
                    break;
                }
            } else if constexpr (GetBBCMicroUpdateFlagsUpdateROMType(UPDATE_FLAGS) == BBCMicroUpdateROMType_CCIBASE) {
                switch (m_state.cpu.abus.w & 0xffe0) {
                case 0xbf80:
                    this->UpdateMapperRegion(0);
                    break;

                case 0xbfa0:
                    this->UpdateMapperRegion(1);
                    break;

                case 0xbfc0:
                    this->UpdateMapperRegion(2);
                    break;

                case 0xbfe0:
                    this->UpdateMapperRegion(3);
                    break;
                }
            } else if constexpr (GetBBCMicroUpdateFlagsUpdateROMType(UPDATE_FLAGS) == BBCMicroUpdateROMType_CCISPELL) {
                if (m_state.cpu.abus.w == 0xbfe0) {
                    this->UpdateMapperRegion(0);
                } else if (m_state.paging.rom_regions[m_state.paging.romsel.b_bits.pr] == 0) {
                    switch (m_state.cpu.abus.w & 0xffe0) {
                    case 0xbfc0:
                        this->UpdateMapperRegion(1);
                        break;

                    case 0xbfa0:
                        this->UpdateMapperRegion(2);
                        break;

                    case 0xbf80:
                        this->UpdateMapperRegion(3);
                        break;

                    case 0xbf60:
                        this->UpdateMapperRegion(4);
                        break;

                    case 0xbf40:
                        this->UpdateMapperRegion(5);
                        break;

                    case 0xbf20:
                        this->UpdateMapperRegion(6);
                        break;

                    case 0xbf00:
                        this->UpdateMapperRegion(7);
                        break;
                    }
                }
            } else if constexpr (GetBBCMicroUpdateFlagsUpdateROMType(UPDATE_FLAGS) == BBCMicroUpdateROMType_PALQST) {
                switch (m_state.cpu.abus.w & 0xffe0) {
                case 0x8820:
                    this->UpdateMapperRegion(2);
                    break;

                case 0x91e0:
                    this->UpdateMapperRegion(1);
                    break;

                case 0x92c0:
                    this->UpdateMapperRegion(3);
                    break;

                case 0x9340:
                    this->UpdateMapperRegion(0);
                    break;
                }
            } else if constexpr (GetBBCMicroUpdateFlagsUpdateROMType(UPDATE_FLAGS) == BBCMicroUpdateROMType_PALWAP) {
                switch (m_state.cpu.abus.w & 0xffe0) {
                case 0x9f00:
                    this->UpdateMapperRegion(0);
                    break;

                case 0x9f20:
                    this->UpdateMapperRegion(1);
                    break;

                case 0x9f40:
                    this->UpdateMapperRegion(2);
                    break;

                case 0x9f60:
                    this->UpdateMapperRegion(3);
                    break;

                case 0x9f80:
                    this->UpdateMapperRegion(4);
                    break;

                case 0x9fa0:
                    this->UpdateMapperRegion(5);
                    break;

                case 0x9fc0:
                    this->UpdateMapperRegion(6);
                    break;

                case 0x9fe0:
                    this->UpdateMapperRegion(7);
                    break;
                }
            } else if constexpr (GetBBCMicroUpdateFlagsUpdateROMType(UPDATE_FLAGS) == BBCMicroUpdateROMType_PALTED) {
                switch (m_state.cpu.abus.w & 0xffe0) {
                case 0x9f80:
                    this->UpdateMapperRegion(0);
                    break;

                case 0x9fa0:
                    this->UpdateMapperRegion(1);
                    break;

                case 0x9fc0:
                    this->UpdateMapperRegion(2);
                    break;

                case 0x9fe0:
                    this->UpdateMapperRegion(3);
                    break;
                }
            } else if constexpr (GetBBCMicroUpdateFlagsUpdateROMType(UPDATE_FLAGS) == BBCMicroUpdateROMType_ABEP_OR_ABE) {
                switch (m_state.cpu.abus.w & 0xfffc) {
                case 0xbff8:
                    this->UpdateMapperRegion(0);
                    break;

                case 0xbffc:
                    this->UpdateMapperRegion(1);
                    break;
                }
            } else if constexpr (GetBBCMicroUpdateFlagsUpdateROMType(UPDATE_FLAGS) == BBCMicroUpdateROMType_Trilogy) {
                if ((m_state.cpu.abus.w & 0xfff8) == 0xbff8) {
                    this->UpdateMapperRegion(m_state.cpu.abus.w & 3);
                }
            } else if constexpr (GetBBCMicroUpdateFlagsUpdateROMType(UPDATE_FLAGS) == BBCMicroUpdateROMType_MO2) {
                if ((m_state.cpu.abus.w & 0xfff0) == 0xa000) {
                    this->UpdateMapperRegion(m_state.cpu.abus.w & 0xf);
                }
            } //<--note
#if ENABLE_ELECTRON //<--note
            else if constexpr (GetBBCMicroUpdateFlagsUpdateROMType(UPDATE_FLAGS) == BBCMicroUpdateROMType_ElectronKeyboard) {
                // nothing to do at this point, but at least cover the case.
            }
#endif             //<--note
            else { //<--note
                static_assert(AlwaysFalseUInt<UPDATE_FLAGS>::value);
            }

            M6502Word mmio_addr = {(uint16_t)(m_state.cpu.abus.w - IO_BEGIN_ADDRESS.w)};

            if (const uint8_t read = m_state.cpu.read) {
                (void)read;
                if (mmio_addr.b.h < 3) {
                    //
                    // Handle reads from the memory-mapped I/O region.
                    //
                    const ReadMMIO *read_mmio = &m_read_mmios[mmio_addr.w];
                    m_state.cpu.dbus = (*read_mmio->fn)(read_mmio->context, m_state.cpu.abus);

#if BBCMICRO_DEBUGGER
                    if constexpr ((UPDATE_FLAGS & BBCMicroUpdateFlag_Debug) != 0) {
                        uint8_t flags = (m_debug->m_debug_flags[BBCMicroDebugState::HOST_ADDRESS_DEBUG_FLAGS_INDEX + m_state.cpu.abus.w] |
                                         m_pc_mem_big_pages[m_state.cpu.opcode_pc.p.p]->read_io_byte_debug_flags[m_state.cpu.abus.io.r][m_state.cpu.abus.io.o]);
                        if (flags & BBCMicroByteDebugFlag_AnyBreakReadMask) {
                            this->DebugHitBreakpoint(&m_state.cpu, &m_debug->m_host_relative_base, flags);
                        }
                    }
#endif
                } else {
                    //
                    // Handle reads from other regions.
                    //
                    if constexpr (GetBBCMicroUpdateFlagsUpdateROMType(UPDATE_FLAGS) == BBCMicroUpdateROMType_EmptySocket) {
                        //
                        // Handle read from memory when there might be unmapped
                        // regions.
                        //
                        const uint8_t *r = m_pc_mem_big_pages[m_state.cpu.opcode_pc.p.p]->r[m_state.cpu.abus.p.p];
                        if (r) {
                            //
                            // Handle read from readable region.
                            //
                            m_state.cpu.dbus = r[m_state.cpu.abus.p.o];

                            if constexpr ((UPDATE_FLAGS & BBCMicroUpdateFlag_RareNonFastPath) != 0) {
#if BBCMICRO_DEBUGGER
                                // Only affect the first 64 KB:
                                // RAM/ANDY/HAZEL/shadow RAM.
                                //
                                // Checking for sideways RAM banks 4-7 on Master
                                // (as opposed to overlaid ROMs) is mildly
                                // inconvenient, and there's no pressing need
                                // for it.
                                if (m_pc_mem_big_pages[m_state.cpu.opcode_pc.p.p]->bp[m_state.cpu.abus.p.p]->index.i < 64 / 4) {
                                    m_state.cpu.dbus = (m_state.cpu.dbus & m_state.ram_and) | m_state.ram_or;
                                }
#endif
                            }
                        } else {
                            //
                            // Handle read from unreadable region.
                            //

                            // (see corresponding logic in BBCMicro::GetStaleDatabusByte.)
                            if constexpr (GetBBCMicroUpdateFlagsUpdateSystemType(UPDATE_FLAGS) == BBCMicroUpdateSystemType_BBCMicro) {
                                // For B/B+, leave the previous value in place.
                                // The CPU data bus is buffered so it'll read
                                // whatever was last written.
                            } else {
                                // For Master, use the last read video data
                                // byte. The CPU data bus is not buffered but
                                // the video data seems to hang around for long
                                // enough.
                                m_state.cpu.dbus = m_state.last_fetched_video_byte;
                            }
                        }
                    } else {
                        //
                        // Handle read from memory when there are no unmapped
                        // regions, but (Electron only) the Electron keyboard
                        // bank could be mapped in.
                        //
#if ENABLE_ELECTRON
                        if constexpr (GetBBCMicroUpdateFlagsUpdateROMType(UPDATE_FLAGS) == BBCMicroUpdateROMType_ElectronKeyboard) {
                            if (m_state.cpu.abus.b.h >= 0x80 && m_state.cpu.abus.b.h < 0xc0) {
                                //
                                // Handle read from keyboard ROM.
                                //
                                uint8_t value = 0;

                                for (uint16_t column = 0; column < 14; ++column) {
                                    if (!(m_state.cpu.abus.w & 1 << column)) {
                                        value |= m_state.key_columns[column];
                                    }
                                }

                                m_state.cpu.dbus = 0xf0 | (value & 0xf);
                            } else {
                                //
                                // Handle ordinary memory read.
                                //
                                m_state.cpu.dbus = m_pc_mem_big_pages[m_state.cpu.opcode_pc.p.p]->r[m_state.cpu.abus.p.p][m_state.cpu.abus.p.o];
                            }
                        } else //<--note
#endif                         //<--note
                        {
                            //
                            // Handle ordinary memory read.
                            //
                            m_state.cpu.dbus = m_pc_mem_big_pages[m_state.cpu.opcode_pc.p.p]->r[m_state.cpu.abus.p.p][m_state.cpu.abus.p.o];
                        }

                        if constexpr ((UPDATE_FLAGS & BBCMicroUpdateFlag_RareNonFastPath) != 0) {
#if BBCMICRO_DEBUGGER
                            // See comment above.
                            if (m_pc_mem_big_pages[m_state.cpu.opcode_pc.p.p]->bp[m_state.cpu.abus.p.p]->index.i < 64 / 4) {
                                m_state.cpu.dbus = (m_state.cpu.dbus & m_state.ram_and) | m_state.ram_or;
                            }
#endif
                        }

#if BBCMICRO_DEBUGGER
                        if constexpr ((UPDATE_FLAGS & BBCMicroUpdateFlag_Debug) != 0) {
                            uint8_t flags = (m_debug->m_debug_flags[BBCMicroDebugState::HOST_ADDRESS_DEBUG_FLAGS_INDEX + m_state.cpu.abus.w] |
                                             m_pc_mem_big_pages[m_state.cpu.opcode_pc.p.p]->byte_debug_flags[m_state.cpu.abus.p.p][m_state.cpu.abus.p.o]);
                            if (flags & BBCMicroByteDebugFlag_AnyBreakReadMask) {
                                this->DebugHitBreakpoint(&m_state.cpu, &m_debug->m_host_relative_base, flags);
                            }
                        }
#endif
                    }
                }

#if BBCMICRO_DEBUGGER
                if constexpr ((UPDATE_FLAGS & BBCMicroUpdateFlag_Debug) != 0) {
                    if (read == M6502ReadType_Interrupt) {
                        if (M6502_IsProbablyIRQ(&m_state.cpu)) {
                            if ((m_state.system_via.ifr.value & m_state.system_via.ier.value & m_debug->hw.system_via_irq_breakpoints.value) ||
                                (m_state.user_via.ifr.value & m_state.user_via.ier.value & m_debug->hw.user_via_irq_breakpoints.value)) {
                                this->SetDebugStepType(BBCMicroStepType_StepIntoIRQHandler, &m_state.cpu);
                            }
                        }
                    }
                }
#endif
            } else {
                if (mmio_addr.b.h < 3) {
                    //
                    // Handle writes to the memory-mapped I/O region.
                    //
                    const WriteMMIO *write_mmio = &m_write_mmios[mmio_addr.w];
                    (*write_mmio->fn)(write_mmio->context, m_state.cpu.abus, m_state.cpu.dbus);
#if BBCMICRO_DEBUGGER
                    if constexpr ((UPDATE_FLAGS & BBCMicroUpdateFlag_Debug) != 0) {
                        uint8_t flags = (m_debug->m_debug_flags[BBCMicroDebugState::HOST_ADDRESS_DEBUG_FLAGS_INDEX + m_state.cpu.abus.w] |
                                         m_pc_mem_big_pages[m_state.cpu.opcode_pc.p.p]->write_io_byte_debug_flags[m_state.cpu.abus.io.r][m_state.cpu.abus.io.o]);
                        if (flags & BBCMicroByteDebugFlag_AnyBreakWriteMask) {
                            this->DebugHitBreakpoint(&m_state.cpu, &m_debug->m_host_relative_base, flags);
                        }
                    }
#endif
                } else {
                    //
                    // Handle writes to other regions.
                    //
                    m_pc_mem_big_pages[m_state.cpu.opcode_pc.p.p]->w[m_state.cpu.abus.p.p][m_state.cpu.abus.p.o] = m_state.cpu.dbus;

#if BBCMICRO_DEBUGGER
                    if constexpr ((UPDATE_FLAGS & BBCMicroUpdateFlag_Debug) != 0) {
                        uint8_t flags = (m_debug->m_debug_flags[BBCMicroDebugState::HOST_ADDRESS_DEBUG_FLAGS_INDEX + m_state.cpu.abus.w] |
                                         m_pc_mem_big_pages[m_state.cpu.opcode_pc.p.p]->byte_debug_flags[m_state.cpu.abus.p.p][m_state.cpu.abus.p.o]);

                        if (flags & BBCMicroByteDebugFlag_AnyBreakWriteMask) {
                            this->DebugHitBreakpoint(&m_state.cpu, &m_debug->m_host_relative_base, flags);
                        }
                    }
#endif
                }
            }

            if constexpr ((UPDATE_FLAGS & BBCMicroUpdateFlag_NonFastPath) != 0) {
                if (M6502_IsAboutToExecute(&m_state.cpu)) {
                    if (Callbacks<InstructionFn>::Callback *callback = m_host_instruction_callbacks.begin) {
                        bool any_removed = false;

                        while (callback != m_host_instruction_callbacks.end) {
                            if (!(*callback->fn)(this, &m_state.cpu, callback->fn_context)) {
                                callback->fn = nullptr;
                                any_removed = true;
                            }

                            ++callback;
                        }

                        if (any_removed) {
                            m_host_instruction_callbacks.DidChange();
                            this->CallbacksDidChange();
                        }
                    }

                    if (m_state.hack_flags & BBCMicroHackFlag_Paste) {
                        ASSERT(m_state.paste_state != BBCMicroPasteState_None);

                        if (m_state.paste_state == BBCMicroPasteState_DelayBeforeStartKey) {
                            ASSERT(m_state.paste_delay_cycles > 0);
                            --m_state.paste_delay_cycles;
                            if (m_state.paste_delay_cycles == 0) {
                                this->SetKeyState(PASTE_START_KEY, true);
                                m_state.paste_state = BBCMicroPasteState_WaitForFirstOSRDCH;
                            }
                        } else {
                            if (m_state.cpu.pc.w == 0xffe1) {
                                // OSRDCH

                                // Put next byte in A.
                                switch (m_state.paste_state) {
                                case BBCMicroPasteState_None:
                                    ASSERT(false);
                                    break;

                                case BBCMicroPasteState_DelayBeforeStartKey:
                                    // could happen! Just ignore it.
                                    break;

                                case BBCMicroPasteState_WaitForFirstOSRDCH:
                                    SetKeyState(PASTE_START_KEY, false);
                                    m_state.cpu.a = 127;
                                    m_state.paste_state = BBCMicroPasteState_Paste;
                                    break;

                                case BBCMicroPasteState_Paste:
                                    ASSERT(m_state.paste_index < m_state.paste_text->size());
                                    m_state.cpu.a = (uint8_t)m_state.paste_text->at(m_state.paste_index);

                                    ++m_state.paste_index;
                                    if (m_state.paste_index == m_state.paste_text->size()) {
                                        StopPaste();
                                    }
                                    break;
                                }

                                // No Escape.
                                m_state.cpu.p.bits.c = 0;

                                // Pretend the instruction was RTS.
                                m_state.cpu.dbus = 0x60;
                            }
                        }
                    }
                }
            }

            if constexpr ((UPDATE_FLAGS & BBCMicroUpdateFlag_Trace) != 0) {
#if BBCMICRO_TRACE
                if (M6502_IsAboutToExecute(&m_state.cpu)) {
                    if (m_trace) {
                        InstructionTraceEvent *e;

                        // Fill out results of last instruction.
                        if ((e = m_trace_current_instruction) != NULL) {
                            e->a = m_state.cpu.a;
                            e->x = m_state.cpu.x;
                            e->y = m_state.cpu.y;
                            e->p = m_state.cpu.p.value;
                            e->data = m_state.cpu.data;
                            e->opcode = m_state.cpu.opcode;
                            e->s = m_state.cpu.s.b.l;
                            //e->pc=m_state.cpu.pc.w;//...for next instruction
                            e->ad = m_state.cpu.ad.w;
                            e->ia = m_state.cpu.ia.w;
                        }

                        // Allocate event for next instruction.
                        e = m_trace_current_instruction = (InstructionTraceEvent *)m_trace->AllocEvent(INSTRUCTION_EVENT, TraceEventSource_Host);

                        if (e) {
                            e->pc = m_state.cpu.abus.w;

                            // doesn't matter if the last instruction ends up
                            // bogus... there are no invalid values.
                        }
                    }
                }
#endif
            }

            if constexpr ((UPDATE_FLAGS & BBCMicroUpdateFlag_RareNonFastPath) != 0) {
#if BBCMICRO_DEBUGGER
                if (m_debug) {
                    if (m_debug->m_step_cpu == &m_state.cpu) {
                        this->DebugHandleStep();
                    }
                }
#endif
            }
        }

#if ENABLE_ELECTRON
        //if constexpr (IsElectronUpdate(UPDATE_FLAGS)) {
        //    ASSERT(false);
        //}
#endif

        if constexpr (IsBBCMicroUpdate(UPDATE_FLAGS)) {
            // Update video hardware.
            if (m_state.video_ula.control.bits.fast_6845 | phi2_1MHz_trailing_edge) {
                const CRTC::Output output = m_state.crtc.Update(m_state.system_via.b.c2);

                uint16_t addr = (uint16_t)output.address;

                if (addr & 0x2000) {
                    addr = (addr & 0x3ff) | m_teletext_bases[addr >> 11 & 1];
                } else {
                    if (addr & 0x1000) {
                        addr -= SCREEN_WRAP_ADJUSTMENTS[m_state.addressable_latch.bits.screen_base];
                        addr &= ~0x1000u;
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
                            m_state.last_frame_cycle_count.n = m_state.cycle_count.n - m_state.last_vsync_cycle_count.n;
                            m_state.last_vsync_cycle_count = m_state.cycle_count;

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
                        if constexpr ((UPDATE_FLAGS & BBCMicroUpdateFlag_RareNonFastPath) != 0) {
#if BBCMICRO_DEBUGGER
                            m_state.ic15_byte = (m_state.ic15_byte & m_state.ram_and) | m_state.ram_or;
#endif
                        }
                    } else {
                        m_state.ic15_byte = 0;
                    }

#if VIDEO_TRACK_METADATA
                    video_unit->metadata.flags |= VideoDataUnitMetadataFlag_HasValue;
                    video_unit->metadata.value = m_state.ic15_byte;
#endif
                }

                uint8_t value = m_ram[addr];
                if constexpr ((UPDATE_FLAGS & BBCMicroUpdateFlag_RareNonFastPath) != 0) {
#if BBCMICRO_DEBUGGER
                    value = (value & m_state.ram_and) | m_state.ram_or;
#endif
                }

                if constexpr (GetBBCMicroUpdateFlagsUpdateSystemType(UPDATE_FLAGS) == BBCMicroUpdateSystemType_Master128 ||
                              GetBBCMicroUpdateFlagsUpdateSystemType(UPDATE_FLAGS) == BBCMicroUpdateSystemType_MasterCompact) {
                    m_state.last_fetched_video_byte = value;
                }

                if (!m_state.video_ula.control.bits.teletext) {
                    if (!m_state.crtc_last_output.display) {
                        m_state.video_ula.DisplayEnabled();
                    }

#if VIDEO_TRACK_METADATA
                    video_unit->metadata.flags |= VideoDataUnitMetadataFlag_HasValue;
                    video_unit->metadata.value = value;
#endif
                }

                // Do this even in teletext mode - the cursor flag then sets up the
                // new cursor state. The byte value only sets some state, so no harm
                // in doing it.
                m_state.video_ula.Byte(value, output.cudisp & m_cursor_mask);

#if VIDEO_TRACK_METADATA
                video_unit->metadata.flags |= VideoDataUnitMetadataFlag_HasAddress | VideoDataUnitMetadataFlag_HasCRTCAddress;
                video_unit->metadata.address = addr;
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

                if (m_state.video_ula.cursor_pattern & 1) {
                    video_unit->pixels.pixels[0].all ^= 0x0fff;
                    video_unit->pixels.pixels[1].all ^= 0x0fff;
                }

                m_state.video_ula.cursor_pattern >>= 1;
            } else {
                if (m_state.crtc_last_output.display && m_state.crtc_last_output.raster < 8) {
                    m_state.video_ula.EmitPixels(&video_unit->pixels);
                } else {
                    m_state.video_ula.EmitBlank(&video_unit->pixels);
                }
            }

            video_unit->pixels.pixels[1].bits.x = 0;

            if (m_state.crtc_last_output.hsync) {
                video_unit->pixels.pixels[1].bits.x |= VideoDataUnitFlag_HSync;
            }

            if (m_state.crtc_last_output.vsync) {
                video_unit->pixels.pixels[1].bits.x |= VideoDataUnitFlag_VSync;
            }

            result |= BBCMicroUpdateResultFlag_VideoUnit;
        }

        if constexpr (IsBBCMicroUpdate(UPDATE_FLAGS)) {
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

                if constexpr ((UPDATE_FLAGS & BBCMicroUpdateFlag_Mouse) != 0) {
                    m_state.user_via.b.p = m_state.mouse_data.value;
                    m_state.user_via.b.c1 = m_state.mouse_signal_x;
                    m_state.user_via.b.c2 = m_state.mouse_signal_y;
                }

                if constexpr (GetBBCMicroUpdateFlagsUpdateSystemType(UPDATE_FLAGS) == BBCMicroUpdateSystemType_MasterCompact &&
                              (UPDATE_FLAGS & BBCMicroUpdateFlag_Mouse) == 0) {
                    // <pre>
                    //  PB4 PB3 PB2 PB1 PB0
                    // +---+---+---+---+---+
                    // | R | U | D | L | F |
                    // +---+---+---+---+---+
                    // </pre>
                    //
                    // Annoyingly, this is completely different from the First Byte layout.
                    m_state.user_via.b.p = (uint8_t)(m_state.user_via.b.p & ~0x1f) |
                                           (0x1f ^ ((m_state.digital_joystick_state.bits.right << 4) |
                                                    (m_state.digital_joystick_state.bits.up << 3) |
                                                    (m_state.digital_joystick_state.bits.down << 2) |
                                                    (m_state.digital_joystick_state.bits.left << 1) |
                                                    (uint8_t)m_state.digital_joystick_state.bits.fire1 | (uint8_t)m_state.digital_joystick_state.bits.fire0));
                }

                if constexpr (GetBBCMicroUpdateFlagsUpdateSystemType(UPDATE_FLAGS) == BBCMicroUpdateSystemType_BBCMicro ||
                              GetBBCMicroUpdateFlagsUpdateSystemType(UPDATE_FLAGS) == BBCMicroUpdateSystemType_Master128) {
                    // Update analogue joystick buttons.
                    m_state.system_via.b.p = (m_state.system_via.b.p & ~(1u << BBCMicroState::SystemVIAPBBits::NOT_JOYSTICK0_FIRE_BIT | 1u << BBCMicroState::SystemVIAPBBits::NOT_JOYSTICK1_FIRE_BIT)) | m_state.not_joystick_buttons;
                }

                // Update addressable latch and RTC.
                const BBCMicroState::SystemVIAPB pb = {m_state.system_via.b.p};

                if (m_state.old_system_via_pb.value != pb.value) {
                    uint8_t mask = 1 << pb.bits.latch_index;

                    m_state.addressable_latch.value &= ~mask;
                    if (pb.bits.latch_value) {
                        m_state.addressable_latch.value |= mask;
                    }

#if BBCMICRO_TRACE
                    if constexpr ((UPDATE_FLAGS & BBCMicroUpdateFlag_Trace) != 0) {
                        if (m_trace) {
                            if (m_trace_flags & BBCMicroTraceFlag_SystemVIA) {
                                TracePortB(pb);
                            }
                        }
                    }
#endif

                    if constexpr (GetBBCMicroUpdateFlagsUpdateSystemType(UPDATE_FLAGS) == BBCMicroUpdateSystemType_Master128) {
                        if (pb.m128_bits.rtc_chip_select &&
                            m_state.old_system_via_pb.m128_bits.rtc_address_strobe &&
                            !pb.m128_bits.rtc_address_strobe) {
                            // Latch address on AS 1->0 transition.
                            m_state.rtc.SetAddress(m_state.system_via.a.p);
                        }
                    } else if constexpr (GetBBCMicroUpdateFlagsUpdateSystemType(UPDATE_FLAGS) == BBCMicroUpdateSystemType_MasterCompact) {
                        UpdatePCD8572(&m_state.eeprom, pb.mcompact_bits.clk, pb.mcompact_bits.data);
                    }

                    m_state.old_system_via_pb = pb;
                }

                if constexpr (GetBBCMicroUpdateFlagsUpdateSystemType(UPDATE_FLAGS) == BBCMicroUpdateSystemType_MasterCompact) {
                    // Update EEPROM data output bit.
                    m_state.system_via.b.p = (m_state.system_via.b.p & ~(1u << BBCMicroState::MasterCompactSystemVIAPBBits::DATA_BIT)) | (uint8_t)(m_state.eeprom.data_output << BBCMicroState::MasterCompactSystemVIAPBBits::DATA_BIT);
                }

                if constexpr (GetBBCMicroUpdateFlagsUpdateSystemType(UPDATE_FLAGS) == BBCMicroUpdateSystemType_Master128) {
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

                if constexpr ((UPDATE_FLAGS & BBCMicroUpdateFlag_ParallelPrinter) != 0) {
                    m_state.user_via.a.c1 = m_state.printer_busy_counter != 1; // TODO: should this be >=1??
                    if (m_state.printer_busy_counter > 0) {
                        --m_state.printer_busy_counter;
                    } else {
                        if (!m_state.user_via.a.c2) {
                            if (m_printer_buffer) {
                                m_printer_buffer->AddByte(m_state.user_via.a.p);
                            }
                            //uint8_t printer_byte = m_state.user_via.a.p;
                            //printf("Printer byte: %03d 0x%02x ", printer_byte, printer_byte);
                            //if (printer_byte >= 32 && printer_byte < 127) {
                            //    printf(" '%c'", printer_byte);
                            //}
                            //printf("\n");
                            m_state.printer_busy_counter = 10;
                        }
                    }
                }

                m_state.old_addressable_latch = m_state.addressable_latch;

                if constexpr ((UPDATE_FLAGS & BBCMicroUpdateFlag_Serial) != 0) {
                    M6502_SetDeviceIRQ(&m_state.cpu, BBCMicroIRQDevice_ACIA, m_state.acia.irq.value);
                }
            } else {
                m_state.system_via_irq_pending = m_state.system_via.UpdatePhi2LeadingEdge();
                m_state.user_via_irq_pending = m_state.user_via.UpdatePhi2LeadingEdge();
            }
        }

        if (phi2_1MHz_trailing_edge) {
            // Update 1770.
            M6502_SetDeviceNMI(&m_state.cpu, BBCMicroNMIDevice_1770, m_state.fdc.Update().value);

            if constexpr (GetBBCMicroUpdateFlagsUpdateSystemType(UPDATE_FLAGS) == BBCMicroUpdateSystemType_BBCMicro ||
                          GetBBCMicroUpdateFlagsUpdateSystemType(UPDATE_FLAGS) == BBCMicroUpdateSystemType_Master128) {
                // Update ADC.
                m_state.system_via.b.c1 = m_state.adc.Update();
            }
        }

        if constexpr (IsBBCMicroUpdate(UPDATE_FLAGS)) {
            // Update sound and mouse. Both require low-frequency updates.
            if ((m_state.cycle_count.n & ((1 << LSHIFT_SOUND_CLOCK_TO_CYCLE_COUNT) - 1)) == 0) {
                sound_unit->sn_output = m_state.sn76489.Update(!m_state.addressable_latch.bits.not_sound_write,
                                                               m_state.system_via.a.p);

                sound_unit->disc_drive_sound = this->UpdateDiscDriveSound(&m_state.drives[0]);
                sound_unit->disc_drive_sound += this->UpdateDiscDriveSound(&m_state.drives[1]);
                result |= BBCMicroUpdateResultFlag_AudioUnit;

                if constexpr ((UPDATE_FLAGS & BBCMicroUpdateFlag_Mouse) != 0) {
                    static_assert(LSHIFT_MOUSE_CLOCK_TO_CYCLE_COUNT > LSHIFT_SOUND_CLOCK_TO_CYCLE_COUNT);
                    if ((m_state.cycle_count.n & ((1 << LSHIFT_MOUSE_CLOCK_TO_CYCLE_COUNT) - 1)) == 0) {
                        if (m_state.mouse_dx != 0) {
                            m_state.mouse_signal_x ^= 1;

                            if (m_state.mouse_dx > 0) {
                                if constexpr (GetBBCMicroUpdateFlagsUpdateSystemType(UPDATE_FLAGS) == BBCMicroUpdateSystemType_MasterCompact) {
                                    m_state.mouse_data.compact_bits.x = !m_state.mouse_signal_x;
                                } else {
                                    m_state.mouse_data.amx_bits.x = !m_state.mouse_signal_x;
                                }

                                --m_state.mouse_dx;
                            } else {
                                if constexpr (GetBBCMicroUpdateFlagsUpdateSystemType(UPDATE_FLAGS) == BBCMicroUpdateSystemType_MasterCompact) {
                                    m_state.mouse_data.compact_bits.x = m_state.mouse_signal_x;
                                } else {
                                    m_state.mouse_data.amx_bits.x = m_state.mouse_signal_x;
                                }

                                ++m_state.mouse_dx;
                            }
                        }

                        if (m_state.mouse_dy != 0) {
                            m_state.mouse_signal_y ^= 1;

                            if (m_state.mouse_dy > 0) {
                                if constexpr (GetBBCMicroUpdateFlagsUpdateSystemType(UPDATE_FLAGS) == BBCMicroUpdateSystemType_MasterCompact) {
                                    m_state.mouse_data.compact_bits.y = m_state.mouse_signal_y;
                                } else {
                                    m_state.mouse_data.amx_bits.y = m_state.mouse_signal_y;
                                }

                                --m_state.mouse_dy;
                            } else {
                                if constexpr (GetBBCMicroUpdateFlagsUpdateSystemType(UPDATE_FLAGS) == BBCMicroUpdateSystemType_MasterCompact) {
                                    m_state.mouse_data.compact_bits.y = !m_state.mouse_signal_y;
                                } else {
                                    m_state.mouse_data.amx_bits.y = !m_state.mouse_signal_y;
                                }

                                ++m_state.mouse_dy;
                            }
                        }
                    }
                }
            }
        }

#if ENABLE_ELECTRON
        if constexpr (IsElectronUpdate(UPDATE_FLAGS)) {
            // Update sound.
            if ((m_state.cycle_count.n & ((1 << LSHIFT_SOUND_CLOCK_TO_CYCLE_COUNT) - 1)) == 0) {
                // For now, completely silent. Also, the update rate is bogus.

                *sound_unit = {};
                result |= BBCMicroUpdateResultFlag_AudioUnit;
            }
        }
#endif
    }

    ++m_state.cycle_count.n;

    return result;

    // Dumb way of inhibiting unreferenced label warning. And you
    // can't goto into an if constexpr, so the label can't be
    // surrounded by one whose condition matches the goto.
    //
    // Luckily, neither VC++ nor gcc seems not to mind that this code
    // is unreachable...
    goto parasite_update_done;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// Not all combinations of update flags are meaningful. (For example, if the
// Parasite flag isn't set, the ParasiteSpecial flag is irrelevant.) Given an
// arbitrary set of update flags, this function sets any ignored bits to 0.
//
//
constexpr uint32_t GetNormalizedBBCMicroUpdateFlags(uint32_t flags) {
    // If no debugger or no tracing, clear the relevant flags, no questions
    // asked. Clear the minimum amount necessary and let the logic below do the
    // rest.
#if !BBCMICRO_TRACE
    flags &= ~BBCMicroUpdateFlag_Trace;
#endif

#if !BBCMICRO_DEBUGGER
    flags &= ~BBCMicroUpdateFlag_Debug;
#endif

    switch (GetBBCMicroUpdateFlagsUpdateSystemType(flags)) {
    default:
        // normalize to BBC Micro type.
        flags &= ~(BBCMicroUpdateFlag_UpdateSystemTypeMask << BBCMicroUpdateFlag_UpdateSystemTypeShift);
        [[fallthrough]];
    case BBCMicroUpdateSystemType_BBCMicro:
        break;

    case BBCMicroUpdateSystemType_Master128:
        break;

    case BBCMicroUpdateSystemType_MasterCompact:
        flags &= ~BBCMicroUpdateFlag_Parasite;
        break;

#if ENABLE_ELECTRON
    case BBCMicroUpdateSystemType_ElectronWithPlus1:
        flags &= ~BBCMicroUpdateFlag_Mouse;
        flags &= ~BBCMicroUpdateFlag_Serial;
        break;
#endif
    }

    if (!(flags & BBCMicroUpdateFlag_Parasite)) {
        flags &= ~BBCMicroUpdateFlag_Parasite3MHzExternal;
    }

    BBCMicroUpdateROMType update_rom_type = (BBCMicroUpdateROMType)((flags >> BBCMicroUpdateFlag_UpdateROMTypeShift) & BBCMicroUpdateFlag_UpdateROMTypeMask);

    if (update_rom_type >= BBCMicroUpdateROMType_Count) {
        // out of bounds ROMType... reset to 0.
        update_rom_type = (BBCMicroUpdateROMType)0;
    }

#if ENABLE_ELECTRON
    if (update_rom_type == BBCMicroUpdateROMType_ElectronKeyboard && GetBBCMicroUpdateFlagsUpdateSystemType(flags) != BBCMicroUpdateSystemType_ElectronWithPlus1) {
        // Invalid combination. Treat as 16 KB.
        update_rom_type = BBCMicroUpdateROMType_16KB;
    }
#endif

    flags = (flags & ~(BBCMicroUpdateFlag_UpdateROMTypeMask << BBCMicroUpdateFlag_UpdateROMTypeShift)) | (uint32_t)update_rom_type << BBCMicroUpdateFlag_UpdateROMTypeShift;

    return flags;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#ifdef BBCMICRO_UPDATE_DEFINE_FUNCTIONS
// non-constexpr thunk for the above
uint32_t BBCMicro::GetNormalizedBBCMicroUpdateFlags(uint32_t flags) {
    return ::GetNormalizedBBCMicroUpdateFlags(flags);
}
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
