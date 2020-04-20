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

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

class TraceSaver {
public:
    TraceSaver(std::shared_ptr<Trace> trace,
               TraceCyclesOutput cycles_output,
               SaveTraceSaveDataFn save_data_fn,
               void *save_data_context,
               SaveTraceWasCanceledFn was_canceled_fn,
               void *was_canceled_context,
               SaveTraceProgress *progress):
    m_trace(std::move(trace)),
    m_cycles_output(cycles_output),
    m_save_data_fn(save_data_fn),
    m_save_data_context(save_data_context),
    m_was_canceled_fn(was_canceled_fn),
    m_was_canceled_context(was_canceled_context),
    m_progress(progress)
    {
    }

    bool Execute() {
        // It would be nice to have the TraceEventType handle the conversion to
        // strings itself. The INSTRUCTION_EVENT handler has to be able to read
        // the config stored by the INITIAL_EVENT handler, though...
        this->SetMFn(BBCMicro::INSTRUCTION_EVENT,&TraceSaver::HandleInstruction);
        this->SetMFn(Trace::WRITE_ROMSEL_EVENT,&TraceSaver::HandleWriteROMSEL);
        this->SetMFn(Trace::WRITE_ACCCON_EVENT,&TraceSaver::HandleWriteACCCON);
        this->SetMFn(Trace::STRING_EVENT,&TraceSaver::HandleString);
        this->SetMFn(SN76489::WRITE_EVENT,&TraceSaver::HandleSN76489WriteEvent);
        this->SetMFn(SN76489::UPDATE_EVENT,&TraceSaver::HandleSN76489UpdateEvent);
        this->SetMFn(R6522::IRQ_EVENT,&TraceSaver::HandleR6522IRQEvent);
        this->SetMFn(Trace::BLANK_LINE_EVENT,&TraceSaver::HandleBlankLine);

        {
            TraceStats stats;
            m_trace->GetStats(&stats);

            if(m_progress) {
                m_progress->num_events=stats.num_events;
            }

            m_time_initial_value=0;
            if(stats.max_time>0) {
                // fingers crossed this is actually accurate enough??
                double exp=floor(1.+log10(stats.max_time));
                m_time_initial_value=(uint64_t)pow(10.,exp-1.);
            }
        }

        LogPrinterTraceSaver printer(this);

        m_output=std::make_unique<Log>("",&printer);

        //uint64_t start_ticks=GetCurrentTickCount();

        m_type=m_trace->GetBBCMicroType();
        m_romsel=m_trace->GetInitialROMSEL();
        m_acccon=m_trace->GetInitialACCCON();
        m_paging_dirty=true;

        bool completed;
        if(m_trace->ForEachEvent(&PrintTrace,this)) {
            completed=true;
        } else {
            m_output->f("(trace file output was canceled)\n");

            completed=false;
        }

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

        m_output=nullptr;

        return completed;
    }
protected:
private:
    typedef void (TraceSaver::*MFn)(const TraceEvent *);

    struct R6522IRQEvent {
        bool valid=false;
        uint64_t time;
        R6522::IRQ ifr,ier;
    };

    std::shared_ptr<Trace> m_trace;
    std::string m_file_name;
    TraceCyclesOutput m_cycles_output=TraceCyclesOutput_Relative;
    MFn m_mfns[256]={};
    std::unique_ptr<Log> m_output;
    const BBCMicroType *m_type=nullptr;
    int m_sound_channel2_value=-1;
    R6522IRQEvent m_last_6522_irq_event_by_via_id[256];
    uint64_t m_last_instruction_time=0;
    bool m_got_first_event_time=false;
    uint64_t m_first_event_time=0;
    ROMSEL m_romsel={};
    ACCCON m_acccon={};
    bool m_paging_dirty=true;
    MemoryBigPageTables m_paging_tables={};
    bool m_io=true;

    // 0         1         2
    // 01234567890123456789012
    // 18446744073709551616
    char m_time_prefix[23];
    size_t m_time_prefix_len=0;
    uint64_t m_time_initial_value=0;

    SaveTraceSaveDataFn m_save_data_fn=nullptr;
    void *m_save_data_context=nullptr;

    SaveTraceWasCanceledFn m_was_canceled_fn=nullptr;
    void *m_was_canceled_context=nullptr;

    SaveTraceProgress *m_progress=nullptr;

    class LogPrinterTraceSaver:
    public LogPrinter
    {
    public:
        explicit LogPrinterTraceSaver(TraceSaver *saver):
        m_saver(saver)
        {
        }

        void Print(const char *str,size_t str_len) override {
            (*m_saver->m_save_data_fn)(str,str_len,m_saver->m_save_data_context);
        }
    protected:
    private:
        TraceSaver *m_saver=nullptr;
    };

    static char *AddByte(char *c,const char *prefix,uint8_t value,const char *suffix) {
        while((*c=*prefix++)!=0) {
            ++c;
        }

        *c++=HEX_CHARS_LC[value>>4&15];
        *c++=HEX_CHARS_LC[value&15];

        while((*c=*suffix++)!=0) {
            ++c;
        }

        return c;
    }

    static char *AddWord(char *c,const char *prefix,uint16_t value,const char *suffix) {
        while((*c=*prefix++)!=0) {
            ++c;
        }

        *c++=HEX_CHARS_LC[value>>12];
        *c++=HEX_CHARS_LC[value>>8&15];
        *c++=HEX_CHARS_LC[value>>4&15];
        *c++=HEX_CHARS_LC[value&15];

        while((*c=*suffix++)!=0) {
            ++c;
        }

        return c;
    }

    char *AddAddress(char *c,const char *prefix,uint16_t pc_,uint16_t value,const char *suffix) {
        while((*c=*prefix++)!=0) {
            ++c;
        }

        if(m_paging_dirty) {
            bool crt_shadow;
            (*m_type->get_mem_big_page_tables_fn)(&m_paging_tables,
                                                  &m_io,
                                                  &crt_shadow,
                                                  m_romsel,
                                                  m_acccon);
            m_paging_dirty=false;
        }

        //const BigPageType *big_page_type=m_paging.GetBigPageTypeForAccess({pc},{value});
        M6502Word addr={value};

        char code;
        if(addr.b.h>=0xfc&&addr.b.h<=0xfe&&m_io) {
            code='i';
        } else {
            M6502Word pc={pc_};
            uint8_t big_page=m_paging_tables.mem_big_pages[m_paging_tables.pc_mem_big_pages_set[pc.p.p]][addr.p.p];
            ASSERT(big_page<NUM_BIG_PAGES);
            code=m_type->big_pages_metadata[big_page].code;
        }

        *c++=code;
        *c++=ADDRESS_PREFIX_SEPARATOR;
        *c++='$';
        *c++=HEX_CHARS_LC[value>>12];
        *c++=HEX_CHARS_LC[value>>8&15];
        *c++=HEX_CHARS_LC[value>>4&15];
        *c++=HEX_CHARS_LC[value&15];

        while((*c=*suffix++)!=0) {
            ++c;
        }

        return c;
    }

    void HandleString(const TraceEvent *e) {
        m_output->s(m_time_prefix);
        LogIndenter indent(m_output.get());
        m_output->s((const char *)e->event);
        m_output->EnsureBOL();
    }

    static const char *GetSoundChannelName(uint8_t reg) {
        static const char *const SOUND_CHANNEL_NAMES[]={"tone 0","tone 1","tone 2","noise",};

        return SOUND_CHANNEL_NAMES[reg>>1&3];
    }

    static double GetSoundHz(uint16_t value) {
        if(value==1) {
            return 0;
        } else {
            if(value==0) {
                value=1024;
            }

            return 4e6/(32.*value);
        }
    }

    void PrintVIAIRQ(const char *name,R6522::IRQ irq) {
        m_output->f("%s: $%02x (%%%s)",name,irq.value,BINARY_BYTE_STRINGS[irq.value]);

        if(irq.value!=0) {
            m_output->s(":");

            if(irq.bits.t1) {
                m_output->s(" t1");
            }

            if(irq.bits.t2) {
                m_output->s(" t2");
            }

            if(irq.bits.cb1) {
                m_output->s(" cb1");
            }

            if(irq.bits.cb2) {
                m_output->s(" cb2");
            }

            if(irq.bits.sr) {
                m_output->s(" sr");
            }

            if(irq.bits.ca1) {
                m_output->s(" ca1");
            }

            if(irq.bits.ca2) {
                m_output->s(" ca2");
            }
        }
    }

    void HandleR6522IRQEvent(const TraceEvent *e) {
        auto ev=(const R6522::IRQEvent *)e->event;
        R6522IRQEvent *last_ev=&m_last_6522_irq_event_by_via_id[ev->id];

        // Try not to spam the output file with too much useless junk when
        // interrupts are disabled.
        if(last_ev->valid) {
            if(last_ev->time>m_last_instruction_time&&
               ev->ifr.value==last_ev->ifr.value&&
               ev->ier.value==last_ev->ier.value)
            {
                // skip it...
                return;
            }
        }

        last_ev->valid=true;
        last_ev->time=e->time;
        last_ev->ifr=ev->ifr;
        last_ev->ier=ev->ier;

        m_output->s(m_time_prefix);
        m_output->f("%s - IRQ state: ",GetBBCMicroVIAIDEnumName(ev->id));
        LogIndenter indent(m_output.get());

        PrintVIAIRQ("IFR",ev->ifr);
        m_output->EnsureBOL();

        PrintVIAIRQ("IER",ev->ier);
        m_output->EnsureBOL();
    }

    void HandleSN76489UpdateEvent(const TraceEvent *e) {
        auto ev=(const SN76489::UpdateEvent *)e->event;

        m_output->s(m_time_prefix);

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

        for(size_t i=0;i<4;++i) {
            auto ch=&ev->state.channels[i];

            m_output->f("ch%zu: freq=%u ($%04x) vol=$%x counter=%u mask=$%02x\n",
                        i,
                        ch->values.freq,ch->values.freq,
                        ch->values.vol,
                        ch->counter,
                        ch->mask);
        }
    }

    void HandleSN76489WriteEvent(const TraceEvent *e) {
        auto ev=(const SN76489::WriteEvent *)e->event;

        m_output->s(m_time_prefix);
        LogIndenter indent(m_output.get());

        m_output->f("SN76489 - $%02x (%d; %%%s) - write ",
                    ev->write_value,
                    ev->write_value,
                    BINARY_BYTE_STRINGS[ev->write_value]);

        if(ev->reg&1) {
            m_output->f("%s volume: %u",GetSoundChannelName(ev->reg),ev->reg_value);
        } else {
            switch(ev->reg>>1) {
                case 2:
                    m_sound_channel2_value=ev->reg_value;
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
                    if(ev->reg_value&4) {
                        m_output->s("white noise");
                    } else {
                        m_output->s("periodic noise");
                    }

                    m_output->f(", %u (",ev->reg_value&3);

                    switch(ev->reg_value&3) {
                        case 0:
                        case 1:
                        case 2:
                            m_output->f("%.1fHz",GetSoundHz(0x10<<(ev->reg_value&3)));
                            break;

                        case 3:
                            if(m_sound_channel2_value<0) {
                                m_output->s("unknown");
                            } else {
                                ASSERT(m_sound_channel2_value<65536);
                                m_output->f("%.1fHz",GetSoundHz((uint16_t)m_sound_channel2_value));
                            }
                            break;
                    }
                    break;

                    m_output->f(")");
            }
        }

        m_output->EnsureBOL();
    }

    void HandleBlankLine(const TraceEvent *e) {
        (void)e;

        static const char BLANK_LINE_CHAR='\n';

        (*m_save_data_fn)(&BLANK_LINE_CHAR,1,m_save_data_context);
    }

    void HandleInstruction(const TraceEvent *e) {
        auto ev=(const BBCMicro::InstructionTraceEvent *)e->event;

        m_last_instruction_time=e->time;

        const M6502DisassemblyInfo *i=&m_type->m6502_config->disassembly_info[ev->opcode];

        // This buffer size has been carefully selected to be Big
        // Enough(tm).
        char line[1000],*c=line;

        if(m_time_prefix_len>0) {
            memcpy(c,m_time_prefix,m_time_prefix_len);
            c+=m_time_prefix_len;
        }

        c=AddAddress(c,"",0,ev->pc,":");

        *c++=i->undocumented?'*':' ';

        char *mnemonic_begin=c;

        memcpy(c,i->mnemonic,sizeof i->mnemonic-1);
        c+=sizeof i->mnemonic-1;

        *c++=' ';

        // This logic is a bit gnarly, and probably wants hiding away
        // somewhere closer to the 6502 code.
        switch(i->mode) {
            default:
                ASSERT(0);
                // fall through
            case M6502AddrMode_IMP:
                break;

            case M6502AddrMode_REL:
            {
                uint16_t tmp;

                if(!ev->data) {
                    tmp=(uint16_t)(ev->pc+2+(uint16_t)(int16_t)(int8_t)ev->ad);
                } else {
                    tmp=ev->ad;
                }

                c=AddWord(c,"$",tmp,"");
                //c+=sprintf(c,"$%04X",tmp);
            }
                break;

            case M6502AddrMode_IMM:
                c=AddByte(c,"#$",ev->data,"");
                break;

            case M6502AddrMode_ZPG:
                c=AddByte(c,"$",(uint8_t)ev->ad,"");
                c=AddAddress(c," [",ev->pc,(uint8_t)ev->ad,"]");
                break;

            case M6502AddrMode_ZPX:
                c=AddByte(c,"$",(uint8_t)ev->ad,",X");
                c=AddAddress(c," [",ev->pc,(uint8_t)(ev->ad+ev->x),"]");
                break;

            case M6502AddrMode_ZPY:
                c=AddByte(c,"$",(uint8_t)ev->ad,",Y");
                c=AddAddress(c," [",ev->pc,(uint8_t)(ev->ad+ev->y),"]");
                break;

            case M6502AddrMode_ABS:
                c=AddWord(c,"$",ev->ad,"");
                if(i->branch) {
                    // don't add the address for JSR/JMP - for
                    // consistency with Bxx and JMP indirect. The addresses
                    // aren't useful anyway, since the next line shows where
                    // execution ended up.
                } else {
                    c=AddAddress(c," [",ev->pc,ev->ad,"]");
                }
                break;

            case M6502AddrMode_ABX:
                c=AddWord(c,"$",ev->ad,",X");
                c=AddAddress(c," [",ev->pc,(uint16_t)(ev->ad+ev->x),"]");
                break;

            case M6502AddrMode_ABY:
                c=AddWord(c,"$",ev->ad,",Y");
                c=AddAddress(c," [",ev->pc,(uint16_t)(ev->ad+ev->y),"]");
                break;

            case M6502AddrMode_INX:
                c=AddByte(c,"($",(uint8_t)ev->ia,",X)");
                c=AddAddress(c," [",ev->pc,ev->ad,"]");
                break;

            case M6502AddrMode_INY:
                c=AddByte(c,"($",(uint8_t)ev->ia,"),Y");
                c=AddAddress(c," [",ev->pc,(uint16_t)(ev->ad+ev->y),"]");
                break;

            case M6502AddrMode_IND:
                c=AddWord(c,"($",ev->ia,")");
                // the effective address isn't stored anywhere - it's
                // loaded straight into the program counter. But it's not
                // really a problem... a JMP is easy to follow.
                break;

            case M6502AddrMode_ACC:
                *c++='A';
                break;

            case M6502AddrMode_INZ:
            {
                c=AddByte(c,"($",(uint8_t)ev->ia,")");
                c=AddAddress(c," [",ev->pc,ev->ad,"]");
            }
                break;

            case M6502AddrMode_INDX:
                c=AddWord(c,"($",ev->ia,",X)");
                // the effective address isn't stored anywhere - it's
                // loaded straight into the program counter. But it's not
                // really a problem... a JMP is easy to follow.
                break;
        }

        M6502P p;
        p.value=ev->p;

        // 0         1         2
        // 0123456789012345678901234
        // xxx ($xx),Y [$xxxx]

        while(c-mnemonic_begin<25) {
            *c++=' ';
        }

        c=AddByte(c,"A=",ev->a," ");
        c=AddByte(c,"X=",ev->x," ");
        c=AddByte(c,"Y=",ev->y," ");
        c=AddByte(c,"S=",ev->s," P=");
        *c++="nN"[p.bits.n];
        *c++="vV"[p.bits.v];
        *c++="dD"[p.bits.d];
        *c++="iI"[p.bits.i];
        *c++="zZ"[p.bits.z];
        *c++="cC"[p.bits.c];
        c=AddByte(c," (D=",ev->data,"");

        // Add some BBC-specific annotations
        if(ev->pc==0xffee||ev->pc==0xffe3) {
            c+=sprintf(c,"; %d",ev->a);

            if(isprint(ev->a)) {
                c+=sprintf(c,"; '%c'",ev->a);
            }
        }

        *c++=')';

        *c++='\n';
        *c=0;

        size_t num_chars=(size_t)(c-line);
        ASSERT(num_chars<sizeof line);

        (*m_save_data_fn)(line,num_chars,m_save_data_context);
        //m_output.s(line);

        if(m_progress) {
            m_progress->num_bytes_written+=num_chars;
        }
    }

    void HandleWriteROMSEL(const TraceEvent *e) {
        auto ev=(const Trace::WriteROMSELEvent *)e->event;

        m_romsel=ev->romsel;
        m_paging_dirty=true;
    }

    void HandleWriteACCCON(const TraceEvent *e) {
        auto ev=(const Trace::WriteACCCONEvent *)e->event;

        m_acccon=ev->acccon;
        m_paging_dirty=true;
    }

    static bool PrintTrace(Trace *t,const TraceEvent *e,void *context) {
        (void)t;

        auto this_=(TraceSaver *)context;

        {
            char *c=this_->m_time_prefix;

            if(this_->m_cycles_output!=TraceCyclesOutput_None) {

                uint64_t time=e->time;
                if(this_->m_cycles_output==TraceCyclesOutput_Relative) {
                    if(!this_->m_got_first_event_time) {
                        this_->m_got_first_event_time=true;
                        this_->m_first_event_time=e->time;
                    }

                    time-=this_->m_first_event_time;
                }

                char zero=' ';

                for(uint64_t value=this_->m_time_initial_value;value!=0;value/=10) {
                    uint64_t digit=time/value%10;

                    if(digit!=0) {
                        *c++=(char)('0'+digit);
                        zero='0';
                    } else {
                        *c++=zero;
                    }
                }

                if(time==0) {
                    c[-1]='0';
                }

                *c++=' ';
                *c++=' ';
            }

            this_->m_time_prefix_len=(size_t)(c-this_->m_time_prefix);

            *c++=0;
            ASSERT(c<=this_->m_time_prefix+sizeof this_->m_time_prefix);
        }

        MFn mfn=this_->m_mfns[e->type->type_id];
        if(mfn) {
            (this_->*mfn)(e);
        } else {
            this_->m_output->f("EVENT: type=%s; size=%zu\n",e->type->GetName().c_str(),e->size);
        }

        this_->m_output->Flush();

        if(this_->m_was_canceled_fn) {
            if((*this_->m_was_canceled_fn)(this_->m_was_canceled_context)) {
                return false;
            }
        }

        if(this_->m_progress) {
            ++this_->m_progress->num_events_handled;
        }

        return true;
    }

    void SetMFn(const TraceEventType &type,MFn print_mfn) {
        ASSERT(!m_mfns[type.type_id]);
        m_mfns[type.type_id]=print_mfn;
    }
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool SaveTrace(std::shared_ptr<Trace> trace,
               TraceCyclesOutput cycles_output,
               SaveTraceSaveDataFn save_data_fn,
               void *save_data_context,
               SaveTraceWasCanceledFn was_canceled_fn,
               void *was_canceled_context,
               SaveTraceProgress *progress)
{
    TraceSaver saver(std::move(trace),
                     cycles_output,
                     save_data_fn,save_data_context,
                     was_canceled_fn,was_canceled_context,
                     progress);

    bool canceled=saver.Execute();
    return canceled;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#endif
