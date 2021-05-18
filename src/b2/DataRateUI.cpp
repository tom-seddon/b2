#include <shared/system.h>
#include "DataRateUI.h"
#include "SDLBeebWindow.h"
#include "BeebThread.h"
#include "dear_imgui.h"
#include <shared/debug.h>
#include "SettingsUI.h"
#include <inttypes.h>
#include "b2.h"

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static std::unique_ptr<std::vector<TimerDef *>> g_all_root_timer_defs;

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

TimerDef::TimerDef(std::string name_,TimerDef *parent):
name(std::move(name_)),
m_parent(parent)
{
    if(m_parent) {
        m_parent->m_children.push_back(this);
    } else {
        if(!g_all_root_timer_defs) {
            g_all_root_timer_defs=std::make_unique<std::vector<TimerDef *>>();
        }

        g_all_root_timer_defs->push_back(this);
    }

}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

TimerDef::~TimerDef() {
    // this can probably be arranged using (for example) a shared_ptr, with
    // each TimerDef having its own reference, but this hardly seems worth the
    // bother...

//    std::vector<TimerDef *> *list;
//    if(m_parent) {
//        list=&m_parent->m_children;
//    } else {
//        list=g_all_root_timer_defs.get();
//    }
//
//    auto it=std::find(list->begin(),list->end(),this);
//    ASSERT(it!=list->end());
//    list->erase(it);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void TimerDef::Reset() {
    m_total_num_ticks=0;
    m_num_samples=0;

    for(TimerDef *child_def:m_children) {
        child_def->Reset();
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

uint64_t TimerDef::GetTotalNumTicks() const {
    return m_total_num_ticks;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

uint64_t TimerDef::GetNumSamples() const {
    return m_num_samples;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void TimerDef::AddTicks(uint64_t num_ticks) {
    m_total_num_ticks+=num_ticks;
    ++m_num_samples;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void TimerDef::DoImGui() {
    if(ImGui::TreeNode(this->name.c_str())) {
        ImGui::Text("%.3f sec tot",GetSecondsFromTicks(m_total_num_ticks));
        ImGui::Text("%.3f sec avg",GetSecondsFromTicks((uint64_t)((double)m_total_num_ticks/m_num_samples)));

        if(m_parent) {
            ImGui::Text("%.3f%% of parent",(double)m_total_num_ticks/m_parent->m_total_num_ticks*100.);
        }

        if(!m_children.empty()) {
            uint64_t total_child_ticks=0;
            for(TimerDef *def:m_children) {
                def->DoImGui();
                total_child_ticks+=def->m_total_num_ticks;
            }

            ImGui::Text("%.3f%% non-child time",((double)m_total_num_ticks-total_child_ticks)/m_total_num_ticks*100.);
        }

        ImGui::TreePop();
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

enum GraphType {
    GraphType_Production,
    GraphType_Consumption,
};

//struct DisplayState {
//    uint64_t last_seen_update_counter=0;
//    DisplayStateGraphType graph_type=DisplayStateGraphType_Production;
//};

class DataRateUI:
    public SettingsUI
{
public:
    explicit DataRateUI(SDLBeebWindow *beeb_window);

    void DoImGui() override;

    bool OnClose() override;
protected:
private:
    GraphType m_graph_type=GraphType_Consumption;
    SDLBeebWindow *m_beeb_window=nullptr;
};


//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

DataRateUI::DataRateUI(SDLBeebWindow *beeb_window):
    m_beeb_window(beeb_window)
{
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static float GetAudioCallbackRecordPercentage(void *data_,int idx) {
    auto data=(const std::vector<BeebThread::AudioCallbackRecord> *)data_;

    ASSERT(idx>=0&&(size_t)idx<data->size());
    const BeebThread::AudioCallbackRecord *record=&(*data)[(size_t)idx];

    if(record->needed==0) {
        return 0.f;
    } else {
        return (float)((double)record->available/record->needed*100.);
    }
}

template<class T>
static const T *GetStats(const std::vector<T> &stats,size_t stats_head,int idx) {
    ASSERT(idx>=0&&(size_t)idx<stats.size());
    size_t stats_index=(stats_head+(size_t)idx)%stats.size();

    const T *result=&stats[stats_index];
    return result;
}

//static const GlobalStats::VBlankStats *GetVBlankStats(const GlobalStats::DisplayStats *dstats,int idx) {
//    ASSERT(idx>=0&&(size_t)idx<dstats->vblank_stats.size());
//    size_t vstats_index=(dstats->vblank_stats_head+(size_t)idx)%dstats->vblank_stats.size();
//
//    const GlobalStats::VBlankStats *vstats=&dstats->vblank_stats[vstats_index];
//    return vstats;
//}

struct GetFrameTimeData {
    const GlobalStats::DisplayStats *dstats=nullptr;
    GraphType graph_type=GraphType_Consumption;
};

static float GetFrameTime(void *data_,int idx) {
    auto data=(const GetFrameTimeData *)data_;
    
    const GlobalStats::VBlankStats *vstats=GetStats(data->dstats->vblank_stats,data->dstats->vblank_stats_head,idx);
    
    uint64_t ticks;
    switch(data->graph_type) {
    default:
        ASSERT(false);
    case GraphType_Production:
        ticks=vstats->production_delta_ticks;
        break;

    case GraphType_Consumption:
        ticks=vstats->consumption_delta_ticks;
        break;
    }
    
    return (float)GetSecondsFromTicks(ticks)*1000.f;
    
//    auto data=(const std::vector<BeebWindow::VBlankRecord> *)data_;
//
//    ASSERT(idx>=0&&(size_t)idx<data->size());
//    const BeebWindow::VBlankRecord *record=&(*data)[(size_t)idx];
//
//    return (float)(GetSecondsFromTicks(record->num_ticks)*1000.);
}

static float GetUpdateTime(void *data,int idx) {
    auto gstats=(const GlobalStats *)data;
    const GlobalStats::UpdateStats *ustats=GetStats(gstats->update_stats,gstats->update_stats_head,idx);

    return (float)GetSecondsFromTicks(ustats->update_ticks)*1000.f;
}

static float GetNumUnits(void *data_,int idx) {
    auto data=(const std::vector<BeebWindow::VBlankRecord> *)data_;

    ASSERT(idx>=0&&(size_t)idx<data->size());
    const BeebWindow::VBlankRecord *record=&(*data)[(size_t)idx];

    return (float)record->num_video_units;
}

static float GetPercentage(void *data_,int idx) {
    auto data=(const std::vector<BeebWindow::VBlankRecord> *)data_;

    ASSERT(idx>=0&&(size_t)idx<data->size());
    const BeebWindow::VBlankRecord *record=&(*data)[(size_t)idx];

    double real_us=GetSecondsFromTicks(record->num_ticks)*1e6;
    double emu_us=(double)record->num_video_units;

    return (float)(emu_us/real_us*100.);
}

#if MUTEX_DEBUGGING
static bool EverLocked(const MutexMetadata *m) {
    if(m->num_locks>0) {
        return true;
    }

    if(m->num_try_locks>0) {
        return true;
    }

    return false;
}
#endif

#if MUTEX_DEBUGGING
static void MutexMetadataUI(const MutexMetadata *m) {
    ImGui::Spacing();
    ImGui::Text("Mutex Name: %s",m->name.c_str());
    ImGui::Text("Locks: %" PRIu64,m->num_locks);
    ImGui::Text("Contended Locks: %" PRIu64 " (%.3f%%)",m->num_contended_locks,m->num_locks==0?0.:(double)m->num_contended_locks/m->num_locks);
    ImGui::Text("Lock Wait Time: %.05f sec",GetSecondsFromTicks(m->total_lock_wait_ticks));
    ImGui::Text("Successful Try Locks: %" PRIu64 "/%" PRIu64,m->num_successful_try_locks,m->num_try_locks.load(std::memory_order_acquire));
}
#endif

static void GlobalEventStatsUI(const char *caption,const GlobalStats::EventStats &stats) {
    ImGui::TextUnformatted(caption);
    ImGui::Text("VBlank messages: %" PRIu64,stats.num_vblank_messages);
    ImGui::Text("Mouse motion events: %" PRIu64,stats.num_mouse_motion_events);
    ImGui::Text("Mouse wheel events: %" PRIu64,stats.num_mouse_wheel_events);
    ImGui::Text("Text input events: %" PRIu64,stats.num_text_input_events);
    ImGui::Text("Key up/down events: %" PRIu64,stats.num_key_events);
}

void DataRateUI::DoImGui() {
    const GlobalStats *const stats=GetGlobalStats();
//    std::shared_ptr<BeebThread> beeb_thread=m_beeb_window->GetBeebThread();

    ImGui::Separator();

//    ImGui::TextUnformatted("Audio Data Availability (mark=25%)");
//    std::vector<BeebThread::AudioCallbackRecord> audio_records=beeb_thread->GetAudioCallbackRecords();
//    ImGuiPlotLines("",&GetAudioCallbackRecordPercentage,&audio_records,(int)audio_records.size(),0,nullptr,0.f,120.,ImVec2(0,100),ImVec2(0,25));
//
    ImGui::Text("Wait for messages: %.3f sec",GetSecondsFromTicks(stats->wait_for_message_ticks));

    ImGui::Separator();

    ImGuiRadioButton(&m_graph_type,GraphType_Production,"Production");
    ImGui::SameLine();
    ImGuiRadioButton(&m_graph_type,GraphType_Consumption,"Consumption");
    
    for(const GlobalStats::DisplayStats &dstats:stats->display_stats) {
        ImGuiIDPusher pusher(dstats.display_id);
        
        ImGui::Text("VBlank Times for display %u (mark=1/60 sec)",dstats.display_id);
        if(dstats.w>0&&dstats.h>0) {
            ImGui::Text("(Display rect: (%d,%d) + %d x %d)",dstats.x,dstats.y,dstats.w,dstats.h);
        }
        

        //ImGui::TextUnformatted("PC VBlank Time (mark=1/60 sec)");
    //std::vector<BeebWindow::VBlankRecord> vblank_records=m_beeb_window->GetVBlankRecords();
        
        GetFrameTimeData data;
        data.dstats=&dstats;
        data.graph_type=m_graph_type;
        
        ImGuiPlotLines("",
                       &GetFrameTime,
                       &data,
                       (int)dstats.vblank_stats.size(),
                       0,
                       nullptr,
                       0.f,100.f,
                       ImVec2(0,100),
                       ImVec2(0,1000.f/60));
    }

    ImGui::Text("UpdateBeeb Times");
    ImGuiPlotLines("",
                   &GetUpdateTime,
                   (void *)stats,
                   (int)stats->update_stats.size(),
                   0,
                   nullptr,
                   0.f,100.f,
                   ImVec2(0,100),
                   ImVec2(0,1000.f/60));

    //
//    ImGui::TextUnformatted("Video Data Consumed per PC VBlank (mark=1/60 sec)");
//    ImGuiPlotLines("",&GetNumUnits,&vblank_records,(int)vblank_records.size(),0,nullptr,0.f,2e6f/15,ImVec2(0,100),ImVec2(0,2e6f/60));
//
//    ImGui::TextUnformatted("Video Data Availability (mark=50%)");
//    ImGuiPlotLines("",&GetPercentage,&vblank_records,(int)vblank_records.size(),0,nullptr,0.f,200.f,ImVec2(0,100),ImVec2(0,50));

    if(!!g_all_root_timer_defs&&!g_all_root_timer_defs->empty()) {
        ImGui::Separator();
        for(TimerDef *def:*g_all_root_timer_defs) {
            def->DoImGui();
        }
    }
//    if(!!g_all_timer_defs&&!g_all_timer_defs->empty()) {
//        ImGui::Separator();
//
//        for(const TimerDef *def:*g_all_timer_defs) {
//            uint64_t ticks=def->GetTotalNumTicks();
//            uint64_t count=def->GetNumSamples();
//
//            ImGui::TextUnformatted(def->name.c_str());
//            ImGui::Text("%.3f sec tot",GetSecondsFromTicks(ticks));
//            ImGui::Text("%.3f sec avg",GetSecondsFromTicks((double)ticks/count));
//        }
//    }
    
    ImGui::Separator();
    
    GlobalEventStatsUI("Total",stats->total);
    GlobalEventStatsUI("Window",stats->window);

#if MUTEX_DEBUGGING

    ImGui::Separator();

    std::vector<std::shared_ptr<const MutexMetadata>> metadata=Mutex::GetAllMetadata();

    size_t num_skipped=0;

    for(size_t i=0;i<metadata.size();++i) {
        const MutexMetadata *m=metadata[i].get();

        if(EverLocked(m)) {
            MutexMetadataUI(m);
        } else {
            ++num_skipped;
        }
    }

    if(num_skipped>0) {
        if(ImGui::CollapsingHeader("Never Locked")) {
            for(size_t i=0;i<metadata.size();++i) {
                const MutexMetadata *m=metadata[i].get();

                if(!EverLocked(m)) {
                    MutexMetadataUI(m);
                }
            }
        }
    }

#endif
                                      
//    // clear out state display states.
//    {
//        auto it=m_display_states.begin();
//        while(it!=m_display_states.end()) {
//            if(it->second.last_seen_update_counter!=m_update_counter) {
//                auto next_it=it;
//                ++next_it;
//                m_display_states.erase(it);
//                it=next_it;
//            } else {
//                ++it;
//            }
//        }
//    }
//
//    ++m_update_counter;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool DataRateUI::OnClose() {
    return false;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void ResetTimerDefs() {
    if(!!g_all_root_timer_defs) {
        for(TimerDef *def:*g_all_root_timer_defs) {
            def->Reset();
        }
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

std::unique_ptr<SettingsUI> CreateDataRateUI(BeebWindow *beeb_window) {
    return nullptr;
}

std::unique_ptr<SettingsUI> CreateDataRateUI(SDLBeebWindow *beeb_window) {
    return std::make_unique<DataRateUI>(beeb_window);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
