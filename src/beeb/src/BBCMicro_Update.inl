#include <shared/system.h>
#include <shared/debug.h>
#include <beeb/BBCMicro.h>
#include <beeb/BeebLink.h>
#include <beeb/sound.h>
#include <beeb/Trace.h>

#include <shared/enum_decl.h>
#include "BBCMicro_private.inl"
#include <shared/enum_end.h>

#if BBCMICRO_UPDATE_GROUP >= BBCMICRO_NUM_UPDATE_GROUPS

// Skip it...

#else

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

        if constexpr ((UPDATE_FLAGS & BBCMicroUpdateFlag_ParasiteSpecial) != 0) {
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
                // parasite special mode.
                if constexpr ((UPDATE_FLAGS & BBCMicroUpdateFlag_ParasiteSpecial) != 0) {
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
                if constexpr ((UPDATE_FLAGS & BBCMicroUpdateFlag_ParasiteSpecial) != 0) {
                    if (m_state.parasite_boot_mode && (m_state.parasite_cpu.abus.w & 0xf800) == 0xf800) {
                        // Really not concerned about the efficiency of special
                        // mode. The emulator is not in this state for long.
                        if (!m_state.parasite_rom_buffer) {
                            m_state.parasite_cpu.dbus = 0;
                        } else {
                            m_state.parasite_cpu.dbus = m_state.parasite_rom_buffer->at(m_state.parasite_cpu.abus.w & 0x7ff);
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
                uint8_t flags = (m_debug->parasite_address_debug_flags[m_state.parasite_cpu.abus.w] |
                                 *((uint8_t *)m_debug->big_pages_byte_debug_flags[PARASITE_BIG_PAGE_INDEX.i] + m_state.parasite_cpu.abus.w));

                if constexpr ((UPDATE_FLAGS & BBCMicroUpdateFlag_ParasiteSpecial) != 0) {
                    // Really not concerned about the efficiency of special
                    // mode. The emulator is not in this state for long.
                    if (m_state.parasite_boot_mode && (m_state.parasite_cpu.abus.w & 0xf000) == 0xf000) {
                        flags = (m_debug->parasite_address_debug_flags[m_state.parasite_cpu.abus.w] |
                                 m_debug->big_pages_byte_debug_flags[PARASITE_ROM_BIG_PAGE_INDEX.i][m_state.parasite_cpu.abus.p.o]);
                    }
                }

                if (flags & BBCMicroByteDebugFlag_AnyBreakReadMask) {
                    this->DebugHitBreakpoint(&m_state.parasite_cpu, &m_debug->parasite_relative_base, flags);
                }
            }
#endif
        } else {
            if ((m_state.parasite_cpu.abus.w & 0xfff0) == 0xfef0) {
                (*m_parasite_write_mmio_fns[m_state.parasite_cpu.abus.w & 7])(&m_state.parasite_tube, m_state.parasite_cpu.abus, m_state.parasite_cpu.dbus);
                if constexpr ((UPDATE_FLAGS & BBCMicroUpdateFlag_ParasiteSpecial) != 0) {
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
                uint8_t flags = (m_debug->parasite_address_debug_flags[m_state.parasite_cpu.abus.w] |
                                 *((uint8_t *)m_debug->big_pages_byte_debug_flags[PARASITE_BIG_PAGE_INDEX.i] + m_state.parasite_cpu.abus.w));
                if (flags & BBCMicroByteDebugFlag_AnyBreakWriteMask) {
                    this->DebugHitBreakpoint(&m_state.parasite_cpu, &m_debug->parasite_relative_base, flags);
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

        if constexpr ((UPDATE_FLAGS & BBCMicroUpdateFlag_DebugStepParasite) != 0) {
#if BBCMICRO_DEBUGGER
            this->DebugHandleStep();
#endif
        }
    }

parasite_update_done:

    if (!phi2_2MHz_trailing_edge) {
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
            result |= BBCMicroUpdateResultFlag_Host;
            (*m_state.cpu.tfn)(&m_state.cpu);

            M6502Word mmio_addr = {(uint16_t)(m_state.cpu.abus.w - 0xfc00u)};
            if (mmio_addr.b.h < 3) {
                if (m_state.cpu.read) {
                    m_state.stretch = m_read_mmios_stretch[mmio_addr.w];
                } else {
                    m_state.stretch = m_write_mmios_stretch[mmio_addr.w];
                }
            }
        }

        if (!m_state.stretch) {
            // Update CPU data bus.
            if constexpr ((UPDATE_FLAGS & BBCMicroUpdateFlag_Hacks) != 0) {
                if (m_state.cpu.read == 0) {
                    if (!m_host_write_fns.empty()) {
                        // Same deal as instruction fns.
                        auto *fn = m_host_write_fns.data();
                        auto *fns_end = fn + m_host_write_fns.size();
                        bool any_removed = false;

                        while (fn != fns_end) {
                            if ((*fn->first)(this, &m_state.cpu, fn->second)) {
                                ++fn;
                            } else {
                                any_removed = true;
                                *fn = *--fns_end;
                            }
                        }

                        if (any_removed) {
                            m_host_write_fns.resize((size_t)(fns_end - m_host_write_fns.data()));

                            UpdateCPUDataBusFn();
                        }
                    }
                }
            }

            M6502Word mmio_addr = {(uint16_t)(m_state.cpu.abus.w - 0xfc00u)};
            if (const uint8_t read = m_state.cpu.read) {
                if (mmio_addr.b.h < 3) {
                    const ReadMMIO *read_mmio = &m_read_mmios[mmio_addr.w];
                    m_state.cpu.dbus = (*read_mmio->fn)(read_mmio->context, m_state.cpu.abus);
                } else {
                    if constexpr (GetBBCMicroUpdateFlagsROMType(UPDATE_FLAGS) == ROMType_16KB) {
                        // nothing extra to do here
                    } else if constexpr (GetBBCMicroUpdateFlagsROMType(UPDATE_FLAGS) == ROMType_CCIWORD) {
                        switch (m_state.cpu.abus.w & 0xffe0) {
                        case 0x8060:
                        case 0xbfc0:
                            this->UpdateMapperRegion(0);
                            break;

                        case 0x8040:
                        case 0xbfa0:
                        case 0x0fe0:
                            this->UpdateMapperRegion(1);
                            break;
                        }
                    } else if constexpr (GetBBCMicroUpdateFlagsROMType(UPDATE_FLAGS) == ROMType_CCIBASE) {
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
                    } else if constexpr (GetBBCMicroUpdateFlagsROMType(UPDATE_FLAGS) == ROMType_CCISPELL) {
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
                    } else if constexpr (GetBBCMicroUpdateFlagsROMType(UPDATE_FLAGS) == ROMType_PALQST) {
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
                    } else if constexpr (GetBBCMicroUpdateFlagsROMType(UPDATE_FLAGS) == ROMType_PALWAP) {
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
                    } else if constexpr (GetBBCMicroUpdateFlagsROMType(UPDATE_FLAGS) == ROMType_PALTED) {
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
                    } else if constexpr (GetBBCMicroUpdateFlagsROMType(UPDATE_FLAGS) == ROMType_ABEP) {
                        switch (m_state.cpu.abus.w & 0xfffc) {
                        case 0xbff8:
                            this->UpdateMapperRegion(0);
                            break;

                        case 0xbffc:
                            this->UpdateMapperRegion(1);
                            break;
                        }
                    } else if constexpr (GetBBCMicroUpdateFlagsROMType(UPDATE_FLAGS) == ROMType_ABE) {
                        switch (m_state.cpu.abus.w & 0xfffc) {
                        case 0xbff8:
                            this->UpdateMapperRegion(1);
                            break;

                        case 0xbffc:
                            this->UpdateMapperRegion(0);
                            break;
                        }
                    } else {
#ifdef _MSC_VER
                        // TODO can probably perform this check without relying
                        // on VC++'s non-standard template instantiation...
                        static_assert(false, "unhandled ROMType");
#endif
                    }
                    m_state.cpu.dbus = m_pc_mem_big_pages[m_state.cpu.opcode_pc.p.p]->r[m_state.cpu.abus.p.p][m_state.cpu.abus.p.o];
                }

#if BBCMICRO_DEBUGGER
                if constexpr ((UPDATE_FLAGS & BBCMicroUpdateFlag_Debug) != 0) {
                    uint8_t flags = (m_debug->host_address_debug_flags[m_state.cpu.abus.w] |
                                     m_pc_mem_big_pages[m_state.cpu.opcode_pc.p.p]->byte_debug_flags[m_state.cpu.abus.p.p][m_state.cpu.abus.p.o]);
                    if (flags & BBCMicroByteDebugFlag_AnyBreakReadMask) {
                        this->DebugHitBreakpoint(&m_state.cpu, &m_debug->host_relative_base, flags);
                    }

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
                    const WriteMMIO *write_mmio = &m_write_mmios[mmio_addr.w];
                    (*write_mmio->fn)(write_mmio->context, m_state.cpu.abus, m_state.cpu.dbus);
                } else {
                    m_pc_mem_big_pages[m_state.cpu.opcode_pc.p.p]->w[m_state.cpu.abus.p.p][m_state.cpu.abus.p.o] = m_state.cpu.dbus;
                }

#if BBCMICRO_DEBUGGER
                if constexpr ((UPDATE_FLAGS & BBCMicroUpdateFlag_Debug) != 0) {
                    uint8_t flags = (m_debug->host_address_debug_flags[m_state.cpu.abus.w] |
                                     m_pc_mem_big_pages[m_state.cpu.opcode_pc.p.p]->byte_debug_flags[m_state.cpu.abus.p.p][m_state.cpu.abus.p.o]);

                    if (flags & BBCMicroByteDebugFlag_AnyBreakWriteMask) {
                        this->DebugHitBreakpoint(&m_state.cpu, &m_debug->host_relative_base, flags);
                    }
                }
#endif
            }

            if constexpr ((UPDATE_FLAGS & BBCMicroUpdateFlag_Hacks) != 0) {
                if (M6502_IsAboutToExecute(&m_state.cpu)) {
                    if (!m_host_instruction_fns.empty()) {

                        // This is a bit bizarre, but I just can't stomach the
                        // idea of literally like 1,000,000 std::vector calls per
                        // second. But this way, it's hopefully more like only
                        // 300,000.

                        auto *fn = m_host_instruction_fns.data();
                        auto *fns_end = fn + m_host_instruction_fns.size();
                        bool removed = false;

                        while (fn != fns_end) {
                            if ((*fn->first)(this, &m_state.cpu, fn->second)) {
                                ++fn;
                            } else {
                                removed = true;
                                *fn = *--fns_end;
                            }
                        }

                        if (removed) {
                            m_host_instruction_fns.resize((size_t)(fns_end - m_host_instruction_fns.data()));

                            UpdateCPUDataBusFn();
                        }
                    }

                    if (m_state.hack_flags & BBCMicroHackFlag_Paste) {
                        ASSERT(m_state.paste_state != BBCMicroPasteState_None);

                        if (m_state.cpu.pc.w == 0xffe1) {
                            // OSRDCH

                            // Put next byte in A.
                            switch (m_state.paste_state) {
                            case BBCMicroPasteState_None:
                                ASSERT(false);
                                break;

                            case BBCMicroPasteState_Wait:
                                SetKeyState(PASTE_START_KEY, false);
                                m_state.paste_state = BBCMicroPasteState_Delete;
                                // fall through
                            case BBCMicroPasteState_Delete:
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

            if constexpr ((UPDATE_FLAGS & BBCMicroUpdateFlag_DebugStepHost) != 0) {
#if BBCMICRO_DEBUGGER
                this->DebugHandleStep();
#endif
            }
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

        result |= BBCMicroUpdateResultFlag_VideoUnit;

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

            if constexpr ((UPDATE_FLAGS & BBCMicroUpdateFlag_IsMasterCompact) != 0 && (UPDATE_FLAGS & BBCMicroUpdateFlag_Mouse) == 0) {
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

            if constexpr ((UPDATE_FLAGS & BBCMicroUpdateFlag_IsMasterCompact) == 0) {
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
                if (m_trace) {
                    if (m_trace_flags & BBCMicroTraceFlag_SystemVIA) {
                        TracePortB(pb);
                    }
                }
#endif

                if constexpr ((UPDATE_FLAGS & BBCMicroUpdateFlag_IsMaster128) != 0) {
                    if (pb.m128_bits.rtc_chip_select &&
                        m_state.old_system_via_pb.m128_bits.rtc_address_strobe &&
                        !pb.m128_bits.rtc_address_strobe) {
                        // Latch address on AS 1->0 transition.
                        m_state.rtc.SetAddress(m_state.system_via.a.p);
                    }
                } else if constexpr ((UPDATE_FLAGS & BBCMicroUpdateFlag_IsMasterCompact) != 0) {
                    UpdatePCD8572(&m_state.eeprom, pb.mcompact_bits.clk, pb.mcompact_bits.data);
                }

                m_state.old_system_via_pb = pb;
            }

            if constexpr ((UPDATE_FLAGS & BBCMicroUpdateFlag_IsMasterCompact) != 0) {
                // Update EEPROM data output bit.
                m_state.system_via.b.p = (m_state.system_via.b.p & ~(1u << BBCMicroState::MasterCompactSystemVIAPBBits::DATA_BIT)) | (uint8_t)(m_state.eeprom.data_output << BBCMicroState::MasterCompactSystemVIAPBBits::DATA_BIT);
            }

            if constexpr ((UPDATE_FLAGS & BBCMicroUpdateFlag_IsMaster128) != 0) {
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
                m_state.user_via.a.c1 = m_state.printer_busy_counter != 1;
                if (m_state.printer_busy_counter > 0) {
                    --m_state.printer_busy_counter;
                } else {
                    if (!m_state.user_via.a.c2) {
                        if (m_printer_buffer) {
                            m_printer_buffer->push_back(m_state.user_via.a.p);
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
        } else {
            m_state.system_via_irq_pending = m_state.system_via.UpdatePhi2LeadingEdge();
            m_state.user_via_irq_pending = m_state.user_via.UpdatePhi2LeadingEdge();
        }

        if (phi2_1MHz_trailing_edge) {
            // Update 1770.
            M6502_SetDeviceNMI(&m_state.cpu, BBCMicroNMIDevice_1770, m_state.fdc.Update().value);

            if constexpr ((UPDATE_FLAGS & BBCMicroUpdateFlag_IsMasterCompact) == 0) {
                // Update ADC.
                m_state.system_via.b.c1 = m_state.adc.Update();
            }
        }

        // Update sound.
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
                            if constexpr ((UPDATE_FLAGS & BBCMicroUpdateFlag_IsMasterCompact) != 0) {
                                m_state.mouse_data.compact_bits.x = !m_state.mouse_signal_x;
                            } else {
                                m_state.mouse_data.amx_bits.x = !m_state.mouse_signal_x;
                            }

                            --m_state.mouse_dx;
                        } else {
                            if constexpr ((UPDATE_FLAGS & BBCMicroUpdateFlag_IsMasterCompact) != 0) {
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
                            if constexpr ((UPDATE_FLAGS & BBCMicroUpdateFlag_IsMasterCompact) != 0) {
                                m_state.mouse_data.compact_bits.y = m_state.mouse_signal_y;
                            } else {
                                m_state.mouse_data.amx_bits.y = m_state.mouse_signal_y;
                            }

                            --m_state.mouse_dy;
                        } else {
                            if constexpr ((UPDATE_FLAGS & BBCMicroUpdateFlag_IsMasterCompact) != 0) {
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

    if (flags & BBCMicroUpdateFlag_IsMasterCompact) {
        flags &= ~(BBCMicroUpdateFlag_IsMaster128 | BBCMicroUpdateFlag_Parasite);

        // (the parasite-specific debug flags are dealt with below)
    }

    if (!(flags & BBCMicroUpdateFlag_Parasite)) {
        flags &= ~(BBCMicroUpdateFlag_DebugStepParasite | BBCMicroUpdateFlag_Parasite3MHzExternal | BBCMicroUpdateFlag_ParasiteSpecial);
    }

    if (!(flags & BBCMicroUpdateFlag_Debug)) {
        flags &= ~(BBCMicroUpdateFlag_DebugStepHost | BBCMicroUpdateFlag_DebugStepParasite);
    }

    if ((flags & BBCMicroUpdateFlag_ROMTypeMask << BBCMicroUpdateFlag_ROMTypeShift) >= (ROMType_Count << BBCMicroUpdateFlag_ROMTypeShift)) {
        // out of bounds ROMType... reset to 0.
        flags &= ~(BBCMicroUpdateFlag_ROMTypeMask << BBCMicroUpdateFlag_ROMTypeShift);
    }

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

#ifdef BBCMICRO_UPDATE_GROUP

#define UPDATE1(N) &BBCMicro::UpdateTemplated<::GetNormalizedBBCMicroUpdateFlags((N)*BBCMICRO_NUM_UPDATE_GROUPS + BBCMICRO_UPDATE_GROUP)>
#define UPDATE2(N) UPDATE1((N) + 0), UPDATE1((N) + 1)
#define UPDATE4(N) UPDATE2((N) + 0), UPDATE2((N) + 2)
#define UPDATE8(N) UPDATE4((N) + 0), UPDATE4((N) + 4)
#define UPDATE16(N) UPDATE8((N) + 0), UPDATE8((N) + 8)
#define UPDATE32(N) UPDATE16((N) + 0), UPDATE16((N) + 16)
#define UPDATE64(N) UPDATE32((N) + 0), UPDATE32((N) + 32)
#define UPDATE128(N) UPDATE64((N) + 0), UPDATE64((N) + 64)
#define UPDATE256(N) UPDATE128((N) + 0), UPDATE128((N) + 128)
#define UPDATE512(N) UPDATE256((N) + 0), UPDATE256((N) + 256)
#define UPDATE1024(N) UPDATE512((N) + 0), UPDATE512((N) + 512)
#define UPDATE2048(N) UPDATE1024((N) + 0), UPDATE1024((N) + 1024)
#define UPDATE4096(N) UPDATE2048((N) + 0), UPDATE2048((N) + 2048)

#if BBCMICRO_NUM_UPDATE_GROUPS == 1
#define MEMBER_NAME ms_update_mfns
#else
#define MEMBER_NAME CONCAT2(ms_update_mfns, BBCMICRO_UPDATE_GROUP)
#endif

const BBCMicro::UpdateMFn BBCMicro::MEMBER_NAME[NUM_UPDATE_MFNS / BBCMICRO_NUM_UPDATE_GROUPS] = {
    UPDATE4096(0x0 * 4096),
    UPDATE4096(0x1 * 4096),
    UPDATE4096(0x2 * 4096),
    UPDATE4096(0x3 * 4096),
#if BBCMICRO_NUM_UPDATE_GROUPS <= 2
    UPDATE4096(0x4 * 4096),
    UPDATE4096(0x5 * 4096),
    UPDATE4096(0x6 * 4096),
    UPDATE4096(0x7 * 4096),
#endif
#if BBCMICRO_NUM_UPDATE_GROUPS == 1
    UPDATE4096(0x8 * 4096),
    UPDATE4096(0x9 * 4096),
    UPDATE4096(0xa * 4096),
    UPDATE4096(0xb * 4096),
    UPDATE4096(0xc * 4096),
    UPDATE4096(0xd * 4096),
    UPDATE4096(0xe * 4096),
    UPDATE4096(0xf * 4096),
#endif
};

#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#endif
