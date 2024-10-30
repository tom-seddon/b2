#include <shared/system.h>
#include <beeb/SaveTrace.h>

#include <shared/enum_def.h>
#include <beeb/SaveTrace.inl>
#include <shared/enum_end.h>

#if BBCMICRO_TRACE

#include <shared/debug.h>
#include <beeb/Trace.h>
#include <beeb/BBCMicro.h>
#include <beeb/6522.h>
#include <math.h>
#include <string.h>
#include <beeb/tube.h>
#include <beeb/BBCMicroParasiteType.h>

#include <shared/enum_decl.h>
#include "SaveTrace_private.inl"
#include <shared/enum_end.h>

#include <shared/enum_def.h>
#include "SaveTrace_private.inl"
#include <shared/enum_end.h>

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

class TraceSaver {
  public:
    TraceSaver(std::shared_ptr<Trace> trace,
               uint32_t output_flags,
               SaveTraceSaveDataFn save_data_fn,
               void *save_data_context,
               SaveTraceWasCanceledFn was_canceled_fn,
               void *was_canceled_context,
               SaveTraceProgress *progress)
        : m_trace(std::move(trace))
        , m_output_flags(output_flags)
        , m_save_data_fn(save_data_fn)
        , m_save_data_context(save_data_context)
        , m_was_canceled_fn(was_canceled_fn)
        , m_was_canceled_context(was_canceled_context)
        , m_progress(progress) {
    }

    bool Execute() {
        // It would be nice to have the TraceEventType handle the conversion to
        // strings itself. The INSTRUCTION_EVENT handler has to be able to read
        // the config stored by the INITIAL_EVENT handler, though...
        this->SetHandler(BBCMicro::INSTRUCTION_EVENT, &TraceSaver::HandleInstruction);
        this->SetHandler(Trace::WRITE_ROMSEL_EVENT, &TraceSaver::HandleWriteROMSEL);
        this->SetHandler(Trace::WRITE_ACCCON_EVENT, &TraceSaver::HandleWriteACCCON);
        this->SetHandler(Trace::PARASITE_BOOT_MODE_EVENT, &TraceSaver::HandleParasiteBootModeEvent, HandlerFlag_PrintPrefix);
        this->SetHandler(Trace::SET_MAPPER_REGION_EVENT, &TraceSaver::HandleSetMapperRegionEvent);
        this->SetHandler(Trace::STRING_EVENT, &TraceSaver::HandleString, HandlerFlag_PrintPrefix);
        this->SetHandler(SN76489::WRITE_EVENT, &TraceSaver::HandleSN76489WriteEvent, HandlerFlag_PrintPrefix);
        this->SetHandler(SN76489::UPDATE_EVENT, &TraceSaver::HandleSN76489UpdateEvent, HandlerFlag_PrintPrefix);
        this->SetHandler(R6522::IRQ_EVENT, &TraceSaver::HandleR6522IRQEvent, HandlerFlag_PrintPrefix);
        //this->SetMFn(Trace::BLANK_LINE_EVENT, &TraceSaver::HandleBlankLine);
        this->SetHandler(R6522::TIMER_TICK_EVENT, &TraceSaver::HandleR6522TimerTickEvent);
        this->SetHandler(TUBE_WRITE_FIFO1_EVENT, &TraceSaver::HandleTubeWriteFIFO1Event, HandlerFlag_PrintPrefix);
        this->SetHandler(TUBE_WRITE_FIFO2_EVENT, &TraceSaver::HandleTubeWriteFIFO2Event, HandlerFlag_PrintPrefix);
        this->SetHandler(TUBE_WRITE_FIFO3_EVENT, &TraceSaver::HandleTubeWriteFIFO3Event, HandlerFlag_PrintPrefix);
        this->SetHandler(TUBE_WRITE_FIFO4_EVENT, &TraceSaver::HandleTubeWriteFIFO4Event, HandlerFlag_PrintPrefix);
        this->SetHandler(TUBE_READ_FIFO1_EVENT, &TraceSaver::HandleTubeReadFIFO1Event, HandlerFlag_PrintPrefix);
        this->SetHandler(TUBE_READ_FIFO2_EVENT, &TraceSaver::HandleTubeReadFIFO2Event, HandlerFlag_PrintPrefix);
        this->SetHandler(TUBE_READ_FIFO3_EVENT, &TraceSaver::HandleTubeReadFIFO3Event, HandlerFlag_PrintPrefix);
        this->SetHandler(TUBE_READ_FIFO4_EVENT, &TraceSaver::HandleTubeReadFIFO4Event, HandlerFlag_PrintPrefix);
        this->SetHandler(TUBE_WRITE_STATUS_EVENT, &TraceSaver::HandleTubeWriteStatusEvent, HandlerFlag_PrintPrefix);

        {
            TraceStats stats;
            m_trace->GetStats(&stats);

            if (m_progress) {
                m_progress->num_events = stats.num_events;
            }

            m_time_initial_value = 0;
            if (stats.max_time.n > 0) {
                // fingers crossed this is actually accurate enough??
                double exp = floor(1. + log10((double)stats.max_time.n));
                m_time_initial_value = (uint64_t)pow(10., exp - 1.);

                // Try to ensure the time column stays the same width for any
                // reasonable trace size when output with relative cycle counts.
                // I've got tripped up by this when trying to diff diverging
                // traces that aren't the same length.
                //
                // No point trying to make room for the full 1<<64 cycles, as
                // (a) that's like 20+ columns, and (b) that's nearly 150,000
                // years of emulated time. But a 1-minute trace would have a 10
                // digit cycle count, and that's not a ridiculous number of
                // columns to reserve.
                m_time_initial_value = std::max(m_time_initial_value,
                                                (uint64_t)10000000000);
            }
        }

        LogPrinterTraceSaver printer(this);

        m_output = std::make_unique<Log>("", &printer);

        //uint64_t start_ticks=GetCurrentTickCount();

        m_type = m_trace->GetBBCMicroType();
        m_paging = m_trace->GetInitialPagingState();
        m_parasite_m6502_config = m_trace->GetParasiteM6502Config();
        m_parasite_boot_mode = m_trace->GetInitialParasiteBootMode();
        m_paging_dirty = true;
        m_parasite_type = m_trace->GetParasiteType();

        std::vector<char> host_m6502_padded_mnemonics_buffer;
        this->InitPaddedMnemonics(&host_m6502_padded_mnemonics_buffer, m_host_m6502_padded_mnemonics, m_type->m6502_config);

        std::vector<char> parasite_m6502_padded_mnemonics_buffer;
        this->InitPaddedMnemonics(&parasite_m6502_padded_mnemonics_buffer, m_parasite_m6502_padded_mnemonics, m_parasite_m6502_config);

        bool completed;
        if (m_trace->ForEachEvent(&PrintTrace, this)) {
            completed = true;
        } else {
            m_output->f("(trace file output was canceled)\n");

            completed = false;
        }

        m_output->Flush();

        //            m_msgs.i.f(
        //                       "trace output file saved: %s\n",
        //                       m_file_name.c_str());
        //        } else {
        //            m_output->f("(trace file output was canceled)\n");
        //
        //            m_msgs.w.f(
        //                       "trace output file canceled: %s\n",
        //                       m_file_name.c_str());
        //        }

        //        double secs=GetSecondsFromTicks(GetCurrentTickCount()-start_ticks);
        //        if(secs!=0.) {
        //            double mbytes=m_num_bytes_written/1024./1024.;
        //            m_msgs.i.f("(%.2f MBytes/sec)\n",mbytes/secs);
        //        }

        //        fclose(m_f);
        //        m_f=NULL;

        m_output = nullptr;

        return completed;
    }

  protected:
  private:
    typedef void (TraceSaver::*MFn)(const TraceEvent *);

    struct R6522IRQEvent {
        bool valid = false;
        CycleCount time;
        R6522::IRQ ifr, ier;
    };

    struct Handler {
        MFn mfn = nullptr;
        uint32_t flags = 0; //combination of HandlerFlag
    };

    std::shared_ptr<Trace> m_trace;
    std::string m_file_name;
    uint32_t m_output_flags = DEFAULT_TRACE_OUTPUT_FLAGS;
    Handler m_handlers[256] = {};
    std::unique_ptr<Log> m_output;
    std::shared_ptr<const BBCMicroType> m_type;
    int m_sound_channel2_value = -1;
    R6522IRQEvent m_last_6522_irq_event_by_via_id[256];
    CycleCount m_last_instruction_time = {0};
    bool m_got_first_event_time = false;
    CycleCount m_first_event_time = {0};
    PagingState m_paging;
    bool m_parasite_boot_mode = false;
    bool m_paging_dirty = true;
    MemoryBigPageTables m_paging_tables = {};
    uint32_t m_paging_flags = 0;
    std::vector<uint8_t> m_tube_fifo1;
    const M6502Config *m_parasite_m6502_config = nullptr;
    BBCMicroParasiteType m_parasite_type = BBCMicroParasiteType_None;

    // State appropriate for current event.
    const M6502Config *m_m6502_config = nullptr;
    const char *const *m_m6502_padded_mnemonics = nullptr;

    // memcpy-friendly.
    static constexpr size_t PADDED_MNEMONIC_SIZE = 5; //padded with trailing spaces
    const char *m_host_m6502_padded_mnemonics[256] = {};
    const char *m_parasite_m6502_padded_mnemonics[256] = {};

    // <pre>
    // 0         1         2
    // 01234567890123456789012
    // 18446744073709551616
    // </pre>
    //
    // But the time prefix is still larger than that, to accommodate some
    // indentation.
    char m_time_prefix[100];
    size_t m_time_prefix_len = 0;
    uint64_t m_time_initial_value = 0;

    SaveTraceSaveDataFn m_save_data_fn = nullptr;
    void *m_save_data_context = nullptr;

    SaveTraceWasCanceledFn m_was_canceled_fn = nullptr;
    void *m_was_canceled_context = nullptr;

    SaveTraceProgress *m_progress = nullptr;

    class LogPrinterTraceSaver : public LogPrinter {
      public:
        explicit LogPrinterTraceSaver(TraceSaver *saver)
            : m_saver(saver) {
        }

        void Print(const char *str, size_t str_len) override {
            (*m_saver->m_save_data_fn)(str, str_len, m_saver->m_save_data_context);
        }

      protected:
      private:
        TraceSaver *m_saver = nullptr;
    };

    void InitPaddedMnemonics(std::vector<char> *buffer, const char **mnemonics, const M6502Config *config) {
        buffer->resize(256 * (PADDED_MNEMONIC_SIZE + 1));

        for (size_t opcode = 0; opcode < 256; ++opcode) {
            char *padded_mnemonic = &buffer->at(opcode * (PADDED_MNEMONIC_SIZE + 1));

            const char *mnemonic = config ? config->disassembly_info[opcode].mnemonic : "???";
            ASSERT(strlen(mnemonic) <= PADDED_MNEMONIC_SIZE);

            snprintf(padded_mnemonic, PADDED_MNEMONIC_SIZE + 1, "%-*s", (int)PADDED_MNEMONIC_SIZE, mnemonic);
            mnemonics[opcode] = padded_mnemonic;
        }
    }

    static char *AddByte(char *c, const char *prefix, uint8_t value, const char *suffix) {
        while ((*c = *prefix++) != 0) {
            ++c;
        }

        *c++ = HEX_CHARS_LC[value >> 4 & 15];
        *c++ = HEX_CHARS_LC[value & 15];

        while ((*c = *suffix++) != 0) {
            ++c;
        }

        return c;
    }

    static char *AddWord(char *c, const char *prefix, uint16_t value, const char *suffix) {
        while ((*c = *prefix++) != 0) {
            ++c;
        }

        *c++ = HEX_CHARS_LC[value >> 12];
        *c++ = HEX_CHARS_LC[value >> 8 & 15];
        *c++ = HEX_CHARS_LC[value >> 4 & 15];
        *c++ = HEX_CHARS_LC[value & 15];

        while ((*c = *suffix++) != 0) {
            ++c;
        }

        return c;
    }

    char *AddAddress(const TraceEvent *ev, char *c, const char *prefix, uint16_t pc_, uint16_t value, const char *suffix, bool align = false) {
        while ((*c = *prefix++) != 0) {
            ++c;
        }

        if (m_paging_dirty) {
            (*m_type->get_mem_big_page_tables_fn)(&m_paging_tables,
                                                  &m_paging_flags,
                                                  m_paging);
            m_paging_dirty = false;
        }

        //const BigPageType *big_page_type=m_paging.GetBigPageTypeForAccess({pc},{value});
        M6502Word addr = {value};

        const char *codes;
        switch (ev->source) {
        default:
            ASSERT(false);
            // fall through
        case TraceEventSource_None:
            codes = "?";
            break;

        case TraceEventSource_Host:
            if (addr.b.h >= 0xfc && addr.b.h <= 0xfe && !(m_paging_flags & PagingFlags_ROMIO)) {
                codes = "i";
            } else {
                M6502Word pc = {pc_};
                BigPageIndex big_page = m_paging_tables.mem_big_pages[m_paging_tables.pc_mem_big_pages_set[pc.p.p]][addr.p.p];
                ASSERT(big_page.i < NUM_BIG_PAGES);
                const BigPageMetadata *bp = &m_type->big_pages_metadata[big_page.i];
                codes = align ? bp->aligned_codes : bp->minimal_codes;
            }
            break;

        case TraceEventSource_Parasite:
            if (m_parasite_boot_mode && addr.b.h >= 0xf0) {
                codes = "r";
            } else {
                codes = "p";
            }
            break;
        }

        *c++ = '$';
        *c++ = HEX_CHARS_LC[value >> 12];
        *c++ = HEX_CHARS_LC[value >> 8 & 15];
        *c++ = HEX_CHARS_LC[value >> 4 & 15];
        *c++ = HEX_CHARS_LC[value & 15];
        *c++ = ADDRESS_SUFFIX_SEPARATOR;
        *c++ = codes[0];
        if (codes[1] != 0) {
            *c++ = codes[1];
        }

        while ((*c = *suffix++) != 0) {
            ++c;
        }

        return c;
    }

    void HandleString(const TraceEvent *e) {
        m_output->s((const char *)e->event);
        m_output->EnsureBOL();
    }

    static const char *GetSoundChannelName(uint8_t reg) {
        static const char *const SOUND_CHANNEL_NAMES[] = {
            "tone 0",
            "tone 1",
            "tone 2",
            "noise",
        };

        return SOUND_CHANNEL_NAMES[reg >> 1 & 3];
    }

    static double GetSoundHz(uint16_t value) {
        if (value == 1) {
            return 0;
        } else {
            if (value == 0) {
                value = 1024;
            }

            return 4e6 / (32. * value);
        }
    }

    void PrintVIAIRQ(const char *name, R6522::IRQ irq) {
        m_output->f("%s: $%02x (%%%s)", name, irq.value, BINARY_BYTE_STRINGS[irq.value]);

        if (irq.value != 0) {
            m_output->s(":");

            if (irq.bits.t1) {
                m_output->s(" t1");
            }

            if (irq.bits.t2) {
                m_output->s(" t2");
            }

            if (irq.bits.cb1) {
                m_output->s(" cb1");
            }

            if (irq.bits.cb2) {
                m_output->s(" cb2");
            }

            if (irq.bits.sr) {
                m_output->s(" sr");
            }

            if (irq.bits.ca1) {
                m_output->s(" ca1");
            }

            if (irq.bits.ca2) {
                m_output->s(" ca2");
            }
        }
    }

    void HandleR6522IRQEvent(const TraceEvent *e) {
        auto ev = (const R6522::IRQEvent *)e->event;
        R6522IRQEvent *last_ev = &m_last_6522_irq_event_by_via_id[ev->id];

        // Try not to spam the output file with too much useless junk when
        // interrupts are disabled.
        if (last_ev->valid) {
            if (last_ev->time.n > m_last_instruction_time.n &&
                ev->ifr.value == last_ev->ifr.value &&
                ev->ier.value == last_ev->ier.value) {
                // skip it...
                return;
            }
        }

        last_ev->valid = true;
        last_ev->time = e->time;
        last_ev->ifr = ev->ifr;
        last_ev->ier = ev->ier;

        //m_output->s(m_time_prefix);
        m_output->f("%s - IRQ state: ", GetBBCMicroVIAIDEnumName(ev->id));
        LogIndenter indent(m_output.get());

        PrintVIAIRQ("IFR", ev->ifr);
        m_output->EnsureBOL();

        PrintVIAIRQ("IER", ev->ier);
        m_output->EnsureBOL();
    }

    void HandleR6522TimerTickEvent(const TraceEvent *e) {
        auto ev = (const R6522::TimerTickEvent *)e->event;

        if (ev->t1_ticked || ev->t2_ticked) {
            m_output->s(m_time_prefix);
            m_output->f("%s -", GetBBCMicroVIAIDEnumName(ev->id));
        }

        if (ev->t1_ticked) {
            m_output->f(" T1=$%04x->$%04x", (uint16_t)(ev->new_t1 + 1), ev->new_t1);
        }

        if (ev->t2_ticked) {
            m_output->f(" T2=$%04x->$%04x", (uint16_t)(ev->new_t2 + 1), ev->new_t2);
        }

        m_output->EnsureBOL();
    }

    void HandleSN76489UpdateEvent(const TraceEvent *e) {
        auto ev = (const SN76489::UpdateEvent *)e->event;

        m_output->f("SN76489 - Update - ");

        LogIndenter indent(m_output.get());

        m_output->f("output: [$%02x $%02x $%02x $%02x]\n",
                    ev->output.ch[0],
                    ev->output.ch[1],
                    ev->output.ch[2],
                    ev->output.ch[3]);
        m_output->f("state: reg=%u noise=%u noise_seed=$%04x noise_toggle=%d\n",
                    ev->state.reg,
                    ev->state.noise,
                    ev->state.noise_seed,
                    ev->state.noise_toggle);

        for (size_t i = 0; i < 4; ++i) {
            auto ch = &ev->state.channels[i];

            m_output->f("ch%zu: freq=%u ($%04x) vol=$%x counter=%u mask=$%02x\n",
                        i,
                        ch->values.freq, ch->values.freq,
                        ch->values.vol,
                        ch->counter,
                        ch->mask);
        }
    }

    void HandleSN76489WriteEvent(const TraceEvent *e) {
        auto ev = (const SN76489::WriteEvent *)e->event;

        //m_output->s(m_time_prefix);
        //LogIndenter indent(m_output.get());

        m_output->f("SN76489 - $%02x (%d; %%%s) - write ",
                    ev->write_value,
                    ev->write_value,
                    BINARY_BYTE_STRINGS[ev->write_value]);

        if (ev->reg & 1) {
            m_output->f("%s volume: %u", GetSoundChannelName(ev->reg), ev->reg_value);
        } else {
            switch (ev->reg >> 1) {
            case 2:
                m_sound_channel2_value = ev->reg_value;
                // fall through
            case 0:
            case 1:
                m_output->f("%s freq: %u ($%03x) (%.1fHz)",
                            GetSoundChannelName(ev->reg),
                            ev->reg_value,
                            ev->reg_value,
                            GetSoundHz(ev->reg_value));
                break;

            case 3:
                m_output->s("noise mode: ");
                if (ev->reg_value & 4) {
                    m_output->s("white noise");
                } else {
                    m_output->s("periodic noise");
                }

                m_output->f(", %u (", ev->reg_value & 3);

                switch (ev->reg_value & 3) {
                case 0:
                case 1:
                case 2:
                    m_output->f("%.1fHz", GetSoundHz(0x10 << (ev->reg_value & 3)));
                    break;

                case 3:
                    if (m_sound_channel2_value < 0) {
                        m_output->s("unknown");
                    } else {
                        ASSERT(m_sound_channel2_value < 65536);
                        m_output->f("%.1fHz", GetSoundHz((uint16_t)m_sound_channel2_value));
                    }
                    break;
                }
                m_output->f(")");
                break;
            }
        }

        m_output->EnsureBOL();
    }

    //void HandleBlankLine(const TraceEvent *e) {
    //    (void)e;

    //    static const char BLANK_LINE_CHAR = '\n';

    //    (*m_save_data_fn)(&BLANK_LINE_CHAR, 1, m_save_data_context);
    //}

    void HandleInstruction(const TraceEvent *e) {
        auto ev = (const BBCMicro::InstructionTraceEvent *)e->event;

        m_last_instruction_time = e->time;

        const M6502DisassemblyInfo *i = &m_m6502_config->disassembly_info[ev->opcode];

        // This buffer size has been carefully selected to be Big
        // Enough(tm).
        char line[1000], *c = line;

        if (m_time_prefix_len > 0) {
            memcpy(c, m_time_prefix, m_time_prefix_len);
            c += m_time_prefix_len;
        }

        c = AddAddress(e, c, "", 0, ev->pc, ":", true); //true=align

        *c++ = i->undocumented ? '*' : ' ';

        char *mnemonic_begin = c;
        memcpy(c, m_m6502_padded_mnemonics[ev->opcode], PADDED_MNEMONIC_SIZE);
        c += PADDED_MNEMONIC_SIZE;

        // This logic is a bit gnarly, and probably wants hiding away
        // somewhere closer to the 6502 code.
        switch (i->mode) {
        default:
            ASSERT(0);
            // fall through
        case M6502AddrMode_IMP:
            break;

        case M6502AddrMode_REL:
            {
                uint16_t tmp;

                if (!ev->data) {
                    tmp = (uint16_t)(ev->pc + 2 + (uint16_t)(int16_t)(int8_t)ev->ad);
                } else {
                    tmp = ev->ad;
                }

                c = AddWord(c, "$", tmp, "");
                //c+=sprintf(c,"$%04X",tmp);
            }
            break;

        case M6502AddrMode_IMM:
            c = AddByte(c, "#$", ev->data, "");
            break;

        case M6502AddrMode_ZPG:
            c = AddByte(c, "$", (uint8_t)ev->ad, "");
            c = AddAddress(e, c, " [", ev->pc, (uint8_t)ev->ad, "]");
            break;

        case M6502AddrMode_ZPX:
            c = AddByte(c, "$", (uint8_t)ev->ad, ",X");
            c = AddAddress(e, c, " [", ev->pc, (uint8_t)(ev->ad + ev->x), "]");
            break;

        case M6502AddrMode_ZPY:
            c = AddByte(c, "$", (uint8_t)ev->ad, ",Y");
            c = AddAddress(e, c, " [", ev->pc, (uint8_t)(ev->ad + ev->y), "]");
            break;

        case M6502AddrMode_ABS:
            c = AddWord(c, "$", ev->ad, "");
            if (i->branch_condition != M6502Condition_None) {
                // don't add the address for JSR/JMP - for
                // consistency with Bxx and JMP indirect. The addresses
                // aren't useful anyway, since the next line shows where
                // execution ended up.
            } else {
                c = AddAddress(e, c, " [", ev->pc, ev->ad, "]");
            }
            break;

        case M6502AddrMode_ABX:
            c = AddWord(c, "$", ev->ad, ",X");
            c = AddAddress(e, c, " [", ev->pc, (uint16_t)(ev->ad + ev->x), "]");
            break;

        case M6502AddrMode_ABY:
            c = AddWord(c, "$", ev->ad, ",Y");
            c = AddAddress(e, c, " [", ev->pc, (uint16_t)(ev->ad + ev->y), "]");
            break;

        case M6502AddrMode_INX:
            c = AddByte(c, "($", (uint8_t)ev->ia, ",X)");
            c = AddAddress(e, c, " [", ev->pc, ev->ad, "]");
            break;

        case M6502AddrMode_INY:
            c = AddByte(c, "($", (uint8_t)ev->ia, "),Y");
            c = AddAddress(e, c, " [", ev->pc, (uint16_t)(ev->ad + ev->y), "]");
            break;

        case M6502AddrMode_IND:
            c = AddWord(c, "($", ev->ia, ")");
            // the effective address isn't stored anywhere - it's
            // loaded straight into the program counter. But it's not
            // really a problem... a JMP is easy to follow.
            break;

        case M6502AddrMode_ACC:
            *c++ = 'A';
            break;

        case M6502AddrMode_INZ:
            {
                c = AddByte(c, "($", (uint8_t)ev->ia, ")");
                c = AddAddress(e, c, " [", ev->pc, ev->ad, "]");
            }
            break;

        case M6502AddrMode_INDX:
            c = AddWord(c, "($", ev->ia, ",X)");
            // the effective address isn't stored anywhere - it's
            // loaded straight into the program counter. But it's not
            // really a problem... a JMP is easy to follow.
            break;

        case M6502AddrMode_ZPG_REL_ROCKWELL:
            {
                uint16_t tmp;

                if (!ev->data) {
                    tmp = (uint16_t)(ev->pc + 3 + (uint16_t)(int16_t)(int8_t)ev->ad);
                } else {
                    tmp = ev->ad;
                }
                c = AddByte(c, "$", ev->ia & 0xff, "");
                c = AddWord(c, ",$", tmp, "");
            }
            break;
        }

        M6502P p;
        p.value = ev->p;

        // 0         1         2
        // 0123456789012345678901234
        // xxx ($xx),Y [$xxxx]

        while (c - mnemonic_begin < 25) {
            *c++ = ' ';
        }

        c = AddByte(c, m_output_flags & TraceOutputFlags_RegisterNames ? "A=" : "", ev->a, " ");
        c = AddByte(c, m_output_flags & TraceOutputFlags_RegisterNames ? "X=" : "", ev->x, " ");
        c = AddByte(c, m_output_flags & TraceOutputFlags_RegisterNames ? "Y=" : "", ev->y, " ");
        c = AddByte(c, m_output_flags & TraceOutputFlags_RegisterNames ? "S=" : "", ev->s, m_output_flags & TraceOutputFlags_RegisterNames ? " P=" : " ");
        *c++ = "nN"[p.bits.n];
        *c++ = "vV"[p.bits.v];
        *c++ = "dD"[p.bits.d];
        *c++ = "iI"[p.bits.i];
        *c++ = "zZ"[p.bits.z];
        *c++ = "cC"[p.bits.c];
        c = AddByte(c, m_output_flags & TraceOutputFlags_RegisterNames ? " (D=" : " (", ev->data, "");

        // Add some BBC-specific annotations
        if (ev->pc == 0xffee ||
            ev->pc == 0xffe3 ||
            (ev->opcode == 0x6c && ev->ia == 0x20e)) {
            // If the output does overflow, the return value is no good,
            // because it's the length of the full expansion. But it's no
            // problem, because it won't overflow.
            //
            // (Of course, this means that then sprintf would actually be fine.
            // But calling sprintf means a deprecation warning on macOS. And I
            // just choose to avoid the deprecation warning this particular
            // way.)
            c += snprintf(c, (size_t)(line + sizeof line - c), "; %d", ev->a);

            if (isprint(ev->a)) {
                *c++ = ';';
                *c++ = ' ';
                *c++ = '\'';
                *c++ = (char)ev->a;
                *c++ = '\'';
            }
        }

        *c++ = ')';

        *c++ = '\n';
        *c = 0;

        size_t num_chars = (size_t)(c - line);
        ASSERT(num_chars < sizeof line);

        (*m_save_data_fn)(line, num_chars, m_save_data_context);
        //m_output.s(line);

        if (m_progress) {
            m_progress->num_bytes_written += num_chars;
        }
    }

    void HandleWriteROMSEL(const TraceEvent *e) {
        auto ev = (const Trace::WriteROMSELEvent *)e->event;

        m_paging.romsel = ev->romsel;
        m_paging_dirty = true;
    }

    void HandleWriteACCCON(const TraceEvent *e) {
        auto ev = (const Trace::WriteACCCONEvent *)e->event;

        m_paging.acccon = ev->acccon;
        m_paging_dirty = true;
    }

    void HandleParasiteBootModeEvent(const TraceEvent *e) {
        auto ev = (const Trace::ParasiteBootModeEvent *)e->event;

        m_parasite_boot_mode = ev->parasite_boot_mode;
        m_output->f("Parasite boot mode: %s\n", BOOL_STR(m_parasite_boot_mode));
    }

    void HandleSetMapperRegionEvent(const TraceEvent *e) {
        auto ev = (const Trace::SetMapperRegionEvent *)e->event;

        m_paging.rom_regions[m_paging.romsel.b_bits.pr] = ev->region;
        m_paging_dirty = true;

        if (m_output_flags & TraceOutputFlags_ROMMapper) {
            m_output->f("Set ROM mapper region: %c\n", GetMapperRegionCode(ev->region));
        }
    }

    void HandleTubeWriteFIFO1Event(const TraceEvent *e) {
        // The asymmetry is a bit annoying.
        if (e->source == TraceEventSource_Host) {
            this->HandleTubeWriteLatchEvent(e, 1);
        } else {
            auto ev = (TubeFIFOEvent *)e->event;

            this->HandleTubeWriteLatchEvent(e, 1);
            m_tube_fifo1.push_back(ev->value);
            this->DumpTubeFIFO1();
        }
    }

    void HandleTubeWriteFIFO2Event(const TraceEvent *e) {
        this->HandleTubeWriteLatchEvent(e, 2);
    }

    void HandleTubeWriteFIFO3Event(const TraceEvent *e) {
        // TODO: deal with this properly...
        this->HandleTubeWriteLatchEvent(e, 3);
    }

    void HandleTubeWriteFIFO4Event(const TraceEvent *e) {
        this->HandleTubeWriteLatchEvent(e, 4);
    }

    void HandleTubeReadFIFO1Event(const TraceEvent *e) {
        // The asymmetry is a bit annoying.
        if (e->source == TraceEventSource_Host) {
            this->HandleTubeReadLatchEvent(e, 1);
            ASSERT(!m_tube_fifo1.empty());
            m_tube_fifo1.erase(m_tube_fifo1.begin());
            this->DumpTubeFIFO1();
        } else {
            this->HandleTubeReadLatchEvent(e, 1);
        }
    }

    void HandleTubeReadFIFO2Event(const TraceEvent *e) {
        this->HandleTubeReadLatchEvent(e, 2);
    }

    void HandleTubeReadFIFO3Event(const TraceEvent *e) {
        // TODO: deal with this properly...
        this->HandleTubeReadLatchEvent(e, 3);
    }

    void HandleTubeReadFIFO4Event(const TraceEvent *e) {
        this->HandleTubeReadLatchEvent(e, 4);
    }

    void HandleTubeReadLatchEvent(const TraceEvent *e, int index) {
        this->HandleTubeLatchEvent(e, index, "Read");
    }

    void HandleTubeWriteLatchEvent(const TraceEvent *e, int index) {
        this->HandleTubeLatchEvent(e, index, "Write");
    }

    void HandleTubeLatchEvent(const TraceEvent *e, int index, const char *operation) {
        auto ev = (TubeFIFOEvent *)e->event;

        m_output->f("%s FIFO%d: %u $%02x %%%s", operation, index, ev->value, ev->value, BINARY_BYTE_STRINGS[ev->value]);
        if (ev->value >= 32 && ev->value <= 126) {
            m_output->f(" '%c'", ev->value);
        }

        m_output->f(": H !full=%d avail=%d IRQ=%d; P !full=%d avail=%d IRQ=%d NMI=%d\n",
                    ev->h_not_full,
                    ev->h_available,
                    ev->h_irq,
                    ev->p_not_full,
                    ev->p_available,
                    ev->p_irq,
                    ev->p_nmi);
    }

    void DumpTubeFIFO1() {
        size_t n = m_tube_fifo1.size();

        m_output->s("Index: ");
        for (size_t i = 0; i < n; ++i) {
            m_output->f("%-4zu", i);
        }
        m_output->s("\n");

        m_output->s("Dec:   ");
        for (size_t i = 0; i < n; ++i) {
            m_output->f("%-4u", m_tube_fifo1[i]);
        }
        m_output->s("\n");

        m_output->s("Hex:   ");
        for (size_t i = 0; i < n; ++i) {
            m_output->f("$%02x ", m_tube_fifo1[i]);
        }
        m_output->s("\n");

        m_output->s("ASCII: ");
        for (size_t i = 0; i < n; ++i) {
            uint8_t c = m_tube_fifo1[i];

            if (c >= 32 && c <= 126) {
                m_output->f("%c   ", c);
            } else {
                switch (c) {
                case 10:
                    m_output->s("\\n  ");
                    break;
                case 13:
                    m_output->s("\\r  ");
                    break;
                case 9:
                    m_output->s("\\t  ");
                    break;
                default:
                    m_output->s("    ");
                    break;
                }
            }
        }
        m_output->s("\n");
    }

    void HandleTubeWriteStatusEvent(const TraceEvent *e) {
        auto ev = (TubeWriteStatusEvent *)e->event;

        m_output->f("Q: Enable HIRQ from R4: %s\n", BOOL_STR(ev->new_status.bits.q));
        m_output->f("I: Enable PIRQ from R1: %s\n", BOOL_STR(ev->new_status.bits.i));
        m_output->f("J: Enable PIRQ from R4: %s\n", BOOL_STR(ev->new_status.bits.j));
        m_output->f("M: Enable PNMI from R3: %s\n", BOOL_STR(ev->new_status.bits.m));
        m_output->f("V: Enable 2-byte R3   : %s\n", BOOL_STR(ev->new_status.bits.v));
        m_output->f("P: PRST               : %s\n", BOOL_STR(ev->new_status.bits.p));
        m_output->f("T: Clear Tube state   : %s\n", BOOL_STR(ev->new_status.bits.t));
        m_output->f("FIFO1: h->p available: %d\n", ev->h2p1_available);
        m_output->f("FIFO3: # h->p: %u; # p->h: %u\n", ev->h2p3_n, ev->p2h3_n);
        m_output->f("FIFO4: h->p available: %u; p->h available: %u\n", ev->h2p4_available, ev->p2h4_available);
        m_output->f("HIRQ=%u PIRQ=%u PNMI=%u\n", ev->h_irq, ev->p_irq, ev->p_nmi);
    }

    static bool PrintTrace(Trace *t, const TraceEvent *e, void *context) {
        (void)t;

        auto this_ = (TraceSaver *)context;

        uint64_t display_time; //in whatever units make sense for the event source
        char *c = this_->m_time_prefix;
        {
            CycleCount time = e->time;
            if ((this_->m_output_flags & (TraceOutputFlags_Cycles | TraceOutputFlags_AbsoluteCycles)) == TraceOutputFlags_Cycles) {
                if (!this_->m_got_first_event_time) {
                    this_->m_got_first_event_time = true;
                    this_->m_first_event_time = e->time;
                }

                time.n -= this_->m_first_event_time.n;
            }

            switch (e->source) {
            case TraceEventSource_Host:
                *c++ = 'H';
                display_time = time.n >> RSHIFT_CYCLE_COUNT_TO_2MHZ;
                this_->m_m6502_config = this_->m_type->m6502_config;
                this_->m_m6502_padded_mnemonics = this_->m_host_m6502_padded_mnemonics;
                break;

            case TraceEventSource_Parasite:
                {
                    *c++ = 'P';
                    size_t n = 80;
                    memset(c, ' ', n);
                    c += n;
                    if (this_->m_parasite_type == BBCMicroParasiteType_External3MHz6502) {
                        display_time = Get3MHzCycleCount(time);
                    } else {
                        display_time = time.n >> RSHIFT_CYCLE_COUNT_TO_4MHZ;
                    }
                    this_->m_m6502_config = this_->m_parasite_m6502_config;
                    this_->m_m6502_padded_mnemonics = this_->m_parasite_m6502_padded_mnemonics;
                }
                break;

            default:
                *c++ = '?';
                display_time = time.n;
                this_->m_m6502_config = this_->m_type->m6502_config;
                this_->m_m6502_padded_mnemonics = this_->m_host_m6502_padded_mnemonics;
                break;
            }

            // Maintain a gap between event source and whatever comes next.
            *c++ = ' ';
        }

        if (this_->m_output_flags & TraceOutputFlags_Cycles) {
            char zero = ' ';

            for (uint64_t value = this_->m_time_initial_value; value != 0; value /= 10) {
                uint64_t digit = display_time / value % 10;

                if (digit != 0) {
                    *c++ = (char)('0' + digit);
                    zero = '0';
                } else {
                    *c++ = zero;
                }
            }

            if (display_time == 0) {
                c[-1] = '0';
            }

            *c++ = ' ';
            *c++ = ' ';
        }

        this_->m_time_prefix_len = (size_t)(c - this_->m_time_prefix);

        *c++ = 0;
        ASSERT(c <= this_->m_time_prefix + sizeof this_->m_time_prefix);

        Handler *h = &this_->m_handlers[e->type->type_id];
        bool need_pop_indent = false;
        if (h->flags & HandlerFlag_PrintPrefix) {
            this_->m_output->s(this_->m_time_prefix);
            this_->m_output->PushIndent();
            need_pop_indent = true;
        }

        if (h->mfn) {
            (this_->*h->mfn)(e);
        } else {
            this_->m_output->f("EVENT: type=%s; size=%zu\n", e->type->GetName().c_str(), e->size);
        }

        if (need_pop_indent) {
            this_->m_output->PopIndent();
        }

        //this_->m_output->Flush();

        if (this_->m_was_canceled_fn) {
            if ((*this_->m_was_canceled_fn)(this_->m_was_canceled_context)) {
                return false;
            }
        }

        if (this_->m_progress) {
            ++this_->m_progress->num_events_handled;
        }

        return true;
    }

    void
    SetHandler(const TraceEventType &type, MFn mfn, uint32_t flags = 0) {
        Handler *h = &m_handlers[type.type_id];

        ASSERT(!h->mfn);

        h->mfn = mfn;
        h->flags = flags;
    }
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool SaveTrace(std::shared_ptr<Trace> trace,
               uint32_t output_flags,
               SaveTraceSaveDataFn save_data_fn,
               void *save_data_context,
               SaveTraceWasCanceledFn was_canceled_fn,
               void *was_canceled_context,
               SaveTraceProgress *progress) {
    TraceSaver saver(std::move(trace),
                     output_flags,
                     save_data_fn, save_data_context,
                     was_canceled_fn, was_canceled_context,
                     progress);

    bool canceled = saver.Execute();
    return canceled;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#endif
