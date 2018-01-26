#include <shared/system.h>
#include "TraceUI.h"
#include <beeb/Trace.h>
#include "dear_imgui.h"
#include "BeebThread.h"
#include "JobQueue.h"
#include "BeebWindow.h"
#include "BeebWindows.h"
#include <beeb/BBCMicro.h>
#include <6502/6502.h>
#include <shared/debug.h>
#include <string.h>
#include "native_ui.h"
#include <inttypes.h>
#include "keys.h"
#include <math.h>
#include <atomic>
#include "SettingsUI.h"

#include <shared/enum_def.h>
#include "TraceUI.inl"
#include <shared/enum_end.h>

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static const std::string RECENT_PATHS_TRACES="traces";

static TraceUISettings g_default_settings;

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

TraceUISettings GetDefaultTraceUISettings() {
    return g_default_settings;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void SetDefaultTraceUISettings(const TraceUISettings &settings) {
    g_default_settings=settings;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if BBCMICRO_TRACE

class TraceUI:
    public SettingsUI
{
public:
#if BBCMICRO_TRACE
    TraceUI(BeebWindow *beeb_window);

    void DoImGui(CommandContextStack *cc_stack) override;
    bool OnClose() override;
protected:
private:
    class SaveTraceJob;
    struct Key;

    BeebWindow *m_beeb_window=nullptr;

    std::shared_ptr<SaveTraceJob> m_save_trace_job;
    std::vector<uint8_t> m_keys;

    TraceUISettings m_settings;
    bool m_config_changed=false;

    int GetKeyIndex(uint8_t beeb_key) const;
    static bool GetBeebKeyName(void *data,int idx,const char **out_text);
    void SaveButton(const char *label,const std::shared_ptr<Trace> &last_trace,bool cycles);
#endif
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

class TraceUI::SaveTraceJob:
    public JobQueue::Job
{
public:
    explicit SaveTraceJob(std::shared_ptr<Trace> trace,
        std::string file_name,
        std::shared_ptr<MessageList> message_list,
        bool cycles):
        m_trace(std::move(trace)),
        m_file_name(std::move(file_name)),
        m_cycles(cycles),
        m_msgs(message_list)
    {
    }

    void ThreadExecute() {
        this->SetMFn(BBCMicro::INSTRUCTION_EVENT,&SaveTraceJob::HandleInstruction);
        this->SetMFn(BBCMicro::INITIAL_EVENT,&SaveTraceJob::HandleInitial);
        this->SetMFn(Trace::STRING_EVENT,&SaveTraceJob::HandleString);

        {
            TraceStats stats;
            m_trace->GetStats(&stats);

            m_num_events=stats.num_events;

            m_time_initial_value=0;
            if(stats.max_time>0) {
                // fingers crossed this is actually accurate enough??
                double exp=floor(1.+log10(stats.max_time));
                m_time_initial_value=(uint64_t)pow(10.,exp-1.);
            }
        }

        if(!m_mfns_ok) {
            return;
        }

        m_f=fopen(m_file_name.c_str(),"wt");
        if(!m_f) {
            int err=errno;
            m_msgs.e.f(
                "failed to open trace output file: %s\n",
                m_file_name.c_str());
            m_msgs.i.f(
                "(fopen failed: %s)\n",
                strerror(err));
            return;
        }

        setvbuf(m_f,NULL,_IOFBF,262144);

        LogPrinterFILE printer(m_f);

        m_output=std::make_unique<Log>("",&printer);

        uint64_t start_ticks=GetCurrentTickCount();

        if(m_trace->ForEachEvent(&PrintTrace,this)) {
            m_msgs.i.f(
                "trace output file saved: %s\n",
                m_file_name.c_str());
        } else {
            m_output->f("(trace file output was canceled)\n");

            m_msgs.w.f(
                "trace output file canceled: %s\n",
                m_file_name.c_str());
        }

        double secs=GetSecondsFromTicks(GetCurrentTickCount()-start_ticks);
        if(secs!=0.) {
            double mbytes=m_num_bytes_written/1024./1024.;
            m_msgs.i.f("(%.2f MBytes/sec)\n",mbytes/secs);
        }

        fclose(m_f);
        m_f=NULL;

        m_output=nullptr;
    }

    void GetProgress(uint64_t *num,uint64_t *total) {
        *num=m_num_events_handled.load(std::memory_order_acquire);
        *total=m_num_events;
    }
protected:
private:
    typedef void (SaveTraceJob::*MFn)(const TraceEvent *);

    std::shared_ptr<Trace> m_trace;
    std::string m_file_name;
    std::shared_ptr<MessageList> m_message_list;
    bool m_cycles=true;
    MFn m_mfns[256]={};
    bool m_mfns_ok=true;
    std::unique_ptr<Log> m_output;
    Messages m_msgs;            // this is quite a big object
    const M6502Config *m_config=nullptr;
    std::atomic<uint64_t> m_num_events_handled{0};
    uint64_t m_num_events=0;
    size_t m_num_bytes_written=0;
    FILE *m_f=nullptr;

    // 0         1         2
    // 01234567890123456789012
    // 18446744073709551616  
    char m_time_prefix[23];
    size_t m_time_prefix_len=0;
    uint64_t m_time_initial_value=0;

    class LogPrinterFILE:
        public LogPrinter
    {
    public:
        explicit LogPrinterFILE(FILE *f):
            m_f(f)
        {
        }

        void Print(const char *str,size_t str_len) override {
            fwrite(str,1,str_len,m_f);
        }
    protected:
    private:
        FILE *m_f=nullptr;
    };

    // Since I stopped trying to minimize #includes, maybe this should
    // be part of Log?
    static void PrintToFILE(const char *str,size_t str_len,void *data) {
        auto this_=(SaveTraceJob *)data;

        fwrite(str,str_len,1,this_->m_f);
        this_->m_num_bytes_written+=str_len;
    }

    void HandleInitial(const TraceEvent *e) {
        auto ev=(const BBCMicro::InitialTraceEvent *)e->event;

        m_config=ev->config;
    }

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

    void HandleString(const TraceEvent *e) {
        m_output->s(m_time_prefix);
        LogIndenter indent(m_output.get());
        m_output->s((const char *)e->event);
        m_output->EnsureBOL();
    }

    void HandleInstruction(const TraceEvent *e) {
        auto ev=(const BBCMicro::InstructionTraceEvent *)e->event;

        if(!m_config) {
            m_config=&M6502_defined_config;
            m_output->f(
                "(No 6502 config set. Using default: %s)\n",
                m_config->name);
        }

        const M6502DisassemblyInfo *i=&m_config->disassembly_info[ev->opcode];

        // This buffer size has been carefully selected to be Big
        // Enough(tm).
        char line[1000],*c=line;

        if(m_time_prefix_len>0) {
            memcpy(c,m_time_prefix,m_time_prefix_len);
            c+=m_time_prefix_len;
        }

        c=AddWord(c,"",ev->pc,":");

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
            break;

        case M6502AddrMode_ZPX:
            c=AddByte(c,"$",(uint8_t)ev->ad,",X");
            c=AddByte(c," [$",(uint8_t)(ev->ad+ev->x),"]");
            break;

        case M6502AddrMode_ZPY:
            c=AddByte(c,"$",(uint8_t)ev->ad,",Y");
            c=AddByte(c," [$",(uint8_t)(ev->ad+ev->y),"]");
            break;

        case M6502AddrMode_ABS:
            c=AddWord(c,"$",ev->ad,"");
            break;

        case M6502AddrMode_ABX:
            c=AddWord(c,"$",ev->ad,",X");
            c=AddWord(c," [$",(uint16_t)(ev->ad+ev->x),"]");
            break;

        case M6502AddrMode_ABY:
            c=AddWord(c,"$",ev->ad,",Y");
            c=AddWord(c," [$",(uint16_t)(ev->ad+ev->y),"]");
            break;

        case M6502AddrMode_INX:
            c=AddByte(c,"($",(uint8_t)ev->ia,",X)");
            c=AddWord(c," [$",ev->ad,"]");
            break;

        case M6502AddrMode_INY:
            c=AddByte(c,"($",(uint8_t)ev->ia,"),Y");
            c=AddWord(c," [$",ev->ad+ev->y,"]");
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
                c=AddWord(c," [$",ev->ad,"]");
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

        fwrite(line,1,num_chars,m_f);
        //m_output.s(line);
        m_num_bytes_written+=num_chars;
    }

    static int PrintTrace(Trace *t,const TraceEvent *e,void *context) {
        (void)t;

        auto this_=(SaveTraceJob *)context;

        {
            char *c=this_->m_time_prefix;

            if(this_->m_cycles) {
                char zero=' ';

                for(uint64_t value=this_->m_time_initial_value;value!=0;value/=10) {
                    uint64_t digit=e->time/value%10;

                    if(digit!=0) {
                        *c++=(char)('0'+digit);
                        zero='0';
                    } else {
                        *c++=zero;
                    }
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

        if(this_->WasCanceled()) {
            return 0;
        }

        this_->m_num_events_handled.fetch_add(1,std::memory_order_acq_rel);

        return 1;
    }

    void SetMFn(const TraceEventType &type,MFn print_mfn) {
        ASSERT(!m_mfns[type.type_id]);
        m_mfns[type.type_id]=print_mfn;
    }
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

TraceUI::TraceUI(BeebWindow *beeb_window):
    m_beeb_window(beeb_window),
    m_settings(g_default_settings)
{
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

int TraceUI::GetKeyIndex(uint8_t beeb_key) const {
    for(size_t i=0;i<m_keys.size();++i) {
        if(m_keys[i]==beeb_key) {
            return (int)i;
        }
    }

    return -1;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static void DoTraceStatsImGui(const volatile TraceStats *stats) {
    ImGui::Separator();

    ImGui::Columns(2);

    ImGui::TextUnformatted("Events");
    ImGui::NextColumn();
    ImGui::Text("%" PRIthou "zu",stats->num_events);
    ImGui::NextColumn();

    ImGui::TextUnformatted("Bytes Used");
    ImGui::NextColumn();
    ImGui::Text("%" PRIthou ".3f MB",stats->num_used_bytes/1024./1024.);
    ImGui::NextColumn();

    ImGui::TextUnformatted("Bytes Allocated");
    ImGui::NextColumn();
    ImGui::Text("%" PRIthou ".3f MB",stats->num_allocated_bytes/1024./1024.);
    ImGui::NextColumn();

    ImGui::Columns(1);

    ImGui::Separator();
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool TraceUI::GetBeebKeyName(void *data,int idx,const char **out_text) {
    auto this_=(TraceUI *)data;

    if(idx>=0&&(size_t)idx<this_->m_keys.size()) {
        *out_text=::GetBeebKeyName((BeebKey)this_->m_keys[(size_t)idx]);
        ASSERT(*out_text);
        return true;
    } else {
        return false;
    }
}

void TraceUI::DoImGui(CommandContextStack *cc_stack) {
    (void)cc_stack;

    std::shared_ptr<BeebThread> beeb_thread=m_beeb_window->GetBeebThread();

    if(m_save_trace_job) {
        uint64_t n,t;
        m_save_trace_job->GetProgress(&n,&t);

        ImGui::Columns(2);

        ImGui::TextUnformatted("Saving");

        ImGui::NextColumn();

        ImGui::Text("%" PRIthou PRIu64 "/%" PRIthou PRIu64,n,t);

        ImGui::NextColumn();

        ImGui::Columns(1);

        float fraction=0.f;
        if(t>0) {
            fraction=(float)(n/(double)t);
        }

        ImGui::ProgressBar(fraction);

        if(m_save_trace_job->IsFinished()) {
            m_save_trace_job=nullptr;
        }

        if(ImGui::Button("Cancel")) {
            m_save_trace_job->Cancel();
            m_save_trace_job=nullptr;
        }

        return;
    }

    const volatile TraceStats *running_stats=beeb_thread->GetTraceStats();

    if(!running_stats) {
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted("Start:");
        ImGui::SameLine();
        ImGuiRadioButton("Immediate",&m_settings.start,TraceUIStartCondition_Now);
        ImGui::SameLine();
        ImGuiRadioButton("Return",&m_settings.start,TraceUIStartCondition_Return);

        ImGui::TextUnformatted("Stop:");
        ImGui::SameLine();
        ImGuiRadioButton("By request",&m_settings.stop,TraceUIStopCondition_ByRequest);
        ImGui::SameLine();
        ImGuiRadioButton("OSWORD 0",&m_settings.stop,TraceUIStopCondition_OSWORD0);

        ImGui::TextUnformatted("Flags");
        for(uint32_t i=1;i!=0;i<<=1) {
            const char *name=GetBBCMicroTraceFlagEnumName((int)i);
            if(name[0]=='?') {
                continue;
            }

            ImGui::CheckboxFlags(name,&m_settings.flags,i);
        }

        if(ImGui::Button("Start")) {
            TraceConditions c;

            switch(m_settings.start) {
            default:
                ASSERT(false);
                // fall through
            case TraceUIStartCondition_Now:
                c.start=BeebThreadStartTraceCondition_Immediate;
                break;

            case TraceUIStartCondition_Return:
                c.start=BeebThreadStartTraceCondition_NextKeypress;
                c.beeb_key=BeebKey_Return;
                break;
            }

            switch(m_settings.stop) {
            default:
                ASSERT(false);
                // fall through
            case TraceUIStopCondition_ByRequest:
                c.stop=BeebThreadStopTraceCondition_ByRequest;
                break;

            case TraceUIStopCondition_OSWORD0:
                c.stop=BeebThreadStopTraceCondition_OSWORD0;
                break;
            }

            c.trace_flags=m_settings.flags;

            beeb_thread->Send(std::make_unique<BeebThreadStartTraceMessage>(c));
        }

        std::shared_ptr<Trace> last_trace=beeb_thread->GetLastTrace();

        if(!!last_trace) {
            TraceStats stats;
            last_trace->GetStats(&stats);

            DoTraceStatsImGui(&stats);

            this->SaveButton("Save...",last_trace,true);
            ImGui::SameLine();

            this->SaveButton("Save (no cycles)...",last_trace,false);
            ImGui::SameLine();

            if(ImGui::Button("Clear")) {
                beeb_thread->ClearLastTrace();
            }
        }
    } else {
        if(ImGui::Button("Stop")) {
            beeb_thread->Send(std::make_unique<BeebThreadStopTraceMessage>());
        }

        DoTraceStatsImGui(running_stats);
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool TraceUI::OnClose() {
    SetDefaultTraceUISettings(m_settings);

    return true;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void TraceUI::SaveButton(const char *label,const std::shared_ptr<Trace> &last_trace,bool cycles) {
    if(ImGui::Button(label)) {
        SaveFileDialog fd(RECENT_PATHS_TRACES);

        fd.AddFilter("Text files","*.txt");
        fd.AddAllFilesFilter();

        std::string path;
        if(fd.Open(&path)) {
            fd.AddLastPathToRecentPaths();
            m_save_trace_job=std::make_shared<SaveTraceJob>(last_trace,path,m_beeb_window->GetMessageList(),cycles);
            BeebWindows::AddJob(m_save_trace_job);
        }
    }

}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

std::unique_ptr<SettingsUI> CreateTraceUI(BeebWindow *beeb_window) {
    return std::make_unique<TraceUI>(beeb_window);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#endif
