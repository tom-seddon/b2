#include <shared/system.h>
#include "TraceUI.h"
#include <beeb/Trace.h>
#include "dear_imgui.h"
#include "BeebThread.h"
#include "JobQueue.h"
#include "BeebWindow.h"
#include "BeebWindows.h"
#include <beeb/BBCMicro.h>
#include <beeb/6502.h>
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

// It's a bit ugly having a single set of default settings, but compared to the
// old behaviour (per-instance settings, defaults overwritten when dialog
// closed) this arrangement makes more sense when using the docking UI. Since
// with the UI docked, it's much more rarely going to be closed.
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

    void DoImGui() override;
    bool OnClose() override;
protected:
private:
    class SaveTraceJob;
    struct Key;

    BeebWindow *m_beeb_window=nullptr;

    std::shared_ptr<SaveTraceJob> m_save_trace_job;
    std::vector<uint8_t> m_keys;

    char m_stop_num_cycles_str[100]={};
    char m_start_address_str[100]={};

    bool m_config_changed=false;

    int GetKeyIndex(uint8_t beeb_key) const;
    void ResetTextBoxes();

    static bool GetBeebKeyName(void *data,int idx,const char **out_text);
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
                          TraceCyclesOutput cycles_output):
    m_trace(std::move(trace)),
    m_file_name(std::move(file_name)),
    m_cycles_output(cycles_output),
    m_msgs(message_list)
    {
    }

    void ThreadExecute() {
        FILE *f=fopen(m_file_name.c_str(),"wt");
        if(!f) {
            int err=errno;
            m_msgs.e.f(
                       "failed to open trace output file: %s\n",
                       m_file_name.c_str());
            m_msgs.i.f(
                       "(fopen failed: %s)\n",
                       strerror(err));
            return;
        }

        setvbuf(f,NULL,_IOFBF,262144);

        uint64_t start_ticks=GetCurrentTickCount();

        if(SaveTrace(m_trace,
                     m_cycles_output,
                     &SaveData,f,
                     &WasCanceledThunk,this,
                     &m_progress))
        {
            m_msgs.i.f(
                       "trace output file saved: %s\n",
                       m_file_name.c_str());
        } else {
            m_msgs.w.f(
                       "trace output file canceled: %s\n",
                       m_file_name.c_str());
        }

        double secs=GetSecondsFromTicks(GetCurrentTickCount()-start_ticks);
        if(secs!=0.) {
            double mbytes=m_progress.num_bytes_written/1024./1024.;
            m_msgs.i.f("(%.2f MBytes/sec)\n",mbytes/secs);
        }

        fclose(f);
        f=NULL;
    }

    void GetProgress(uint64_t *num,uint64_t *total) {
        *num=m_progress.num_events_handled;
        *total=m_progress.num_events;
    }
protected:
private:
    std::shared_ptr<Trace> m_trace;
    std::string m_file_name;
    TraceCyclesOutput m_cycles_output=TraceCyclesOutput_Relative;
    Messages m_msgs;            // this is quite a big object
    SaveTraceProgress m_progress;

    static bool SaveData(const void *data,size_t num_bytes,void *context) {
        size_t num_bytes_written=fwrite(data,1,num_bytes,(FILE *)context);
        return num_bytes_written==num_bytes;
    }

    static bool WasCanceledThunk(void *context) {
        auto this_=(TraceUI::SaveTraceJob *)context;

        return this_->WasCanceled();
    }
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

TraceUI::TraceUI(BeebWindow *beeb_window):
    m_beeb_window(beeb_window)
{
    this->ResetTextBoxes();
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

void TraceUI::DoImGui() {
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
        ImGui::TextUnformatted("Start condition");
        ImGuiRadioButton("Immediate",&g_default_settings.start,TraceUIStartCondition_Now);
        ImGuiRadioButton("Return",&g_default_settings.start,TraceUIStartCondition_Return);
        ImGuiRadioButton("Instruction",&g_default_settings.start,TraceUIStartCondition_Instruction);
        if(g_default_settings.start==TraceUIStartCondition_Instruction) {
            if(ImGui::InputText("Address (hex)",
                                m_start_address_str,
                                sizeof m_start_address_str,
                                ImGuiInputTextFlags_CharsHexadecimal|ImGuiInputTextFlags_EnterReturnsTrue))
            {
                if(!GetUInt16FromString(&g_default_settings.start_address,
                                        m_start_address_str,
                                        16))
                {
                    this->ResetTextBoxes();
                }
            }
        }

        ImGui::Spacing();

        ImGui::TextUnformatted("Stop condition");
        ImGuiRadioButton("By request",&g_default_settings.stop,TraceUIStopCondition_ByRequest);
        ImGuiRadioButton("OSWORD 0",&g_default_settings.stop,TraceUIStopCondition_OSWORD0);
        ImGuiRadioButton("Cycle count",&g_default_settings.stop,TraceUIStopCondition_NumCycles);
        if(g_default_settings.stop==TraceUIStopCondition_NumCycles) {
            if(ImGui::InputText("Cycles",
                                m_stop_num_cycles_str,
                                sizeof m_stop_num_cycles_str,
                                ImGuiInputTextFlags_EnterReturnsTrue))
            {
                if(!GetUInt64FromString(&g_default_settings.stop_num_cycles,
                                        m_stop_num_cycles_str))
                {
                    this->ResetTextBoxes();
                }
            }
        }

        ImGui::Spacing();

        ImGui::TextUnformatted("Other traces");
        for(uint32_t i=1;i!=0;i<<=1) {
            const char *name=GetBBCMicroTraceFlagEnumName((int)i);
            if(name[0]=='?') {
                continue;
            }

            ImGui::CheckboxFlags(name,&g_default_settings.flags,i);
        }

        ImGui::Spacing();

        ImGui::TextUnformatted("Other settings");
        ImGui::Checkbox("Unlimited recording", &g_default_settings.unlimited);

        if(ImGui::Button("Start")) {
            TraceConditions c;

            switch(g_default_settings.start) {
            default:
                ASSERT(false);
                // fall through
            case TraceUIStartCondition_Now:
                c.start=BeebThreadStartTraceCondition_Immediate;
                break;

            case TraceUIStartCondition_Return:
                c.start=BeebThreadStartTraceCondition_NextKeypress;
                c.start_key=BeebKey_Return;
                break;

                case TraceUIStartCondition_Instruction:
                    c.start=BeebThreadStartTraceCondition_Instruction;
                    c.start_address=g_default_settings.start_address;
                    break;
            }

            switch(g_default_settings.stop) {
            default:
                ASSERT(false);
                // fall through
            case TraceUIStopCondition_ByRequest:
                c.stop=BeebThreadStopTraceCondition_ByRequest;
                break;

            case TraceUIStopCondition_OSWORD0:
                c.stop=BeebThreadStopTraceCondition_OSWORD0;
                break;

                case TraceUIStopCondition_NumCycles:
                    c.stop=BeebThreadStopTraceCondition_NumCycles;
                    c.stop_num_cycles=g_default_settings.stop_num_cycles;
                    break;
            }

            c.trace_flags=g_default_settings.flags;

            size_t max_num_bytes;
            if(g_default_settings.unlimited) {
                max_num_bytes=SIZE_MAX;
            } else {
                // 64MBytes = ~12m cycles, or ~6 sec, with all the flags on,
                // recorded sitting at the BASIC prompt, producing a ~270MByte
                // text file.
                //
                // 256MBytes, then, ought to be ~25 seconds, and a ~1GByte
                // text file. This ought to be enough to be getting on with,
                // and the buffer size is not excessive even for 32-bit systems.
                max_num_bytes=256*1024*1024;
            }

            beeb_thread->Send(std::make_shared<BeebThread::StartTraceMessage>(c,max_num_bytes));
        }

        std::shared_ptr<Trace> last_trace=beeb_thread->GetLastTrace();

        if(!!last_trace) {
            TraceStats stats;
            last_trace->GetStats(&stats);

            DoTraceStatsImGui(&stats);

            ImGui::TextUnformatted("Cycles output:");
            ImGuiRadioButton("Absolute",&g_default_settings.cycles_output,TraceCyclesOutput_Absolute);
            ImGuiRadioButton("Relative",&g_default_settings.cycles_output,TraceCyclesOutput_Relative);
            ImGuiRadioButton("None",&g_default_settings.cycles_output,TraceCyclesOutput_None);

            if(ImGui::Button("Save...")) {
                SaveFileDialog fd(RECENT_PATHS_TRACES);

                fd.AddFilter("Text files",{".txt"});
                fd.AddAllFilesFilter();

                std::string path;
                if(fd.Open(&path)) {
                    fd.AddLastPathToRecentPaths();
                    m_save_trace_job=std::make_shared<SaveTraceJob>(last_trace,
                                                                    path,
                                                                    m_beeb_window->GetMessageList(),
                                                                    g_default_settings.cycles_output);
                    BeebWindows::AddJob(m_save_trace_job);
                }
            }

            ImGui::SameLine();

            if(ImGui::Button("Clear")) {
                beeb_thread->ClearLastTrace();
            }
        }
    } else {
        if(ImGui::Button("Stop")) {
            beeb_thread->Send(std::make_shared<BeebThread::StopTraceMessage>());
        }

        DoTraceStatsImGui(running_stats);
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool TraceUI::OnClose() {
    return true;
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

void TraceUI::ResetTextBoxes() {
    snprintf(m_start_address_str,
             sizeof m_start_address_str,
             "%x",
             g_default_settings.start_address);

    snprintf(m_stop_num_cycles_str,
             sizeof m_stop_num_cycles_str,
             "%" PRIu64,
             g_default_settings.stop_num_cycles);
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

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

std::unique_ptr<SettingsUI> CreateTraceUI(BeebWindow *beeb_window) {
    return std::make_unique<TraceUI>(beeb_window);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#endif
