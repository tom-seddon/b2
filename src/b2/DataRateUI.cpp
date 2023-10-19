#include <shared/system.h>
#include "DataRateUI.h"
#include "BeebWindow.h"
#include "BeebThread.h"
#include "dear_imgui.h"
#include <shared/debug.h>
#include "SettingsUI.h"
#include <inttypes.h>

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static std::unique_ptr<std::vector<TimerDef *>> g_all_root_timer_defs;
static uint64_t APPROX_STARTUP_TICKS = GetCurrentTickCount(); //doesn't need to be perfect...

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

TimerDef::TimerDef(std::string name_, TimerDef *parent)
    : name(std::move(name_))
    , m_parent(parent) {
    if (m_parent) {
        m_parent->m_children.push_back(this);
    } else {
        if (!g_all_root_timer_defs) {
            g_all_root_timer_defs = std::make_unique<std::vector<TimerDef *>>();
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
    m_total_num_ticks = 0;
    m_num_samples = 0;

    for (TimerDef *child_def : m_children) {
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
    m_total_num_ticks += num_ticks;
    ++m_num_samples;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void TimerDef::DoImGui() {
    if (ImGui::TreeNode(this->name.c_str())) {
        ImGui::Text("%.3f sec tot", GetSecondsFromTicks(m_total_num_ticks));
        ImGui::Text("%.3f " MICROSECONDS_UTF8 " avg", GetSecondsFromTicks((uint64_t)((double)m_total_num_ticks / m_num_samples)) * 1.e6);

        if (m_parent) {
            ImGui::Text("%.3f%% of parent", (double)m_total_num_ticks / m_parent->m_total_num_ticks * 100.);
        }

        if (!m_children.empty()) {
            uint64_t total_child_ticks = 0;
            for (TimerDef *def : m_children) {
                def->DoImGui();
                total_child_ticks += def->m_total_num_ticks;
            }

            ImGui::Text("%.3f%% non-child time", ((double)m_total_num_ticks - total_child_ticks) / m_total_num_ticks * 100.);
        }

        ImGui::TreePop();
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

class DataRateUI : public SettingsUI {
  public:
    explicit DataRateUI(BeebWindow *beeb_window);

    void DoImGui() override;

    bool OnClose() override;

  protected:
  private:
    BeebWindow *m_beeb_window;

    void GetVBlankRecords(std::vector<BeebWindow::VBlankRecord> *vblank_records);
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

DataRateUI::DataRateUI(BeebWindow *beeb_window)
    : m_beeb_window(beeb_window) {
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static float GetAudioCallbackRecordPercentage(void *data_, int idx) {
    auto data = (const std::vector<BeebThread::AudioCallbackRecord> *)data_;

    ASSERT(idx >= 0 && (size_t)idx < data->size());
    const BeebThread::AudioCallbackRecord *record = &(*data)[(size_t)idx];

    if (record->needed == 0) {
        return 0.f;
    } else {
        return (float)((double)record->available / record->needed * 100.);
    }
}

static float GetFrameTime(void *data_, int idx) {
    auto data = (const std::vector<BeebWindow::VBlankRecord> *)data_;

    ASSERT(idx >= 0 && (size_t)idx < data->size());
    const BeebWindow::VBlankRecord *record = &(*data)[(size_t)idx];

    return (float)(GetSecondsFromTicks(record->num_ticks) * 1000.);
}

static float GetNumUnits(void *data_, int idx) {
    auto data = (const std::vector<BeebWindow::VBlankRecord> *)data_;

    ASSERT(idx >= 0 && (size_t)idx < data->size());
    const BeebWindow::VBlankRecord *record = &(*data)[(size_t)idx];

    return (float)record->num_video_units;
}

static float GetPercentage(void *data_, int idx) {
    auto data = (const std::vector<BeebWindow::VBlankRecord> *)data_;

    ASSERT(idx >= 0 && (size_t)idx < data->size());
    const BeebWindow::VBlankRecord *record = &(*data)[(size_t)idx];

    double real_us = GetSecondsFromTicks(record->num_ticks) * 1e6;
    double emu_us = (double)record->num_video_units;

    return (float)(emu_us / real_us * 100.);
}

#if MUTEX_DEBUGGING
static void MutexMetadataUI(MutexMetadata *m) {
    ImGuiIDPusher pusher(m);

    uint64_t num_ticks = GetCurrentTickCount() - m->stats.start_ticks;

    ImGui::Spacing();
    ImGui::Text("Mutex Name: %s", m->name.c_str());
    ImGui::Text("Locks: %" PRIu64 " (~%.1f/sec)", m->stats.num_locks, num_ticks == 0 ? 0 : m->stats.num_locks / GetSecondsFromTicks(num_ticks));
    if (m->stats.num_locks > 0) {
        ImGui::Text("Contended Locks: %" PRIu64 " (%.3f%%)", m->stats.num_contended_locks, m->stats.num_locks == 0 ? 0. : (double)m->stats.num_contended_locks / m->stats.num_locks);

        ImGui::Text("Lock Wait Time: %.01f ms (~%.1f%% total)",
                    GetMillisecondsFromTicks(m->stats.total_lock_wait_ticks),
                    m->stats.total_lock_wait_ticks / (double)num_ticks * 100.);

        ImGui::Text("Lock Wait Stats: Min: %.01f ms; Max: %0.1f ms; Mean: %.01f ms",
                    GetMillisecondsFromTicks(m->stats.min_lock_wait_ticks),
                    GetMillisecondsFromTicks(m->stats.max_lock_wait_ticks),
                    GetMillisecondsFromTicks(m->stats.total_lock_wait_ticks) / m->stats.num_locks);

    } else {
        ImGui::TextUnformatted("Contended Locks: N/A");
        ImGui::TextUnformatted("Lock Wait Time: N/A");
        ImGui::TextUnformatted("Lock Wait Stats: N/A");
    }

    if (m->ever_locked) {
        if (ImGui::Button("Reset Stats")) {
            m->RequestReset();
        }
    }

    uint64_t num_try_locks = m->num_try_locks.load(std::memory_order_acquire);
    if (num_try_locks > 0) {
        ImGui::Text("Successful Try Locks: %" PRIu64 "/%" PRIu64, m->stats.num_successful_try_locks, num_try_locks);
    }
}
#endif

void DataRateUI::DoImGui() {
    std::shared_ptr<BeebThread> beeb_thread = m_beeb_window->GetBeebThread();

    ImGui::Separator();

    std::vector<BeebWindow::VBlankRecord> vblank_records;

    if (ImGui::CollapsingHeader("Audio Data Availability (mark=25%)")) {
        std::vector<BeebThread::AudioCallbackRecord> audio_records = beeb_thread->GetAudioCallbackRecords();
        ImGuiPlotLines("", &GetAudioCallbackRecordPercentage, &audio_records, (int)audio_records.size(), 0, nullptr, 0.f, 250., ImVec2(0, 100), ImVec2(0, 25));
    }

    if (ImGui::CollapsingHeader("PC VBlank Time (mark=1/60 sec)")) {
        this->GetVBlankRecords(&vblank_records);
        ImGuiPlotLines("", &GetFrameTime, &vblank_records, (int)vblank_records.size(), 0, nullptr, 0.f, 100.f, ImVec2(0, 100), ImVec2(0, 1000.f / 60));
    }

    if (ImGui::CollapsingHeader("Video Data Consumed per PC VBlank (mark=1/60 sec)")) {
        this->GetVBlankRecords(&vblank_records);
        ImGuiPlotLines("", &GetNumUnits, &vblank_records, (int)vblank_records.size(), 0, nullptr, 0.f, 2e6f / 15, ImVec2(0, 100), ImVec2(0, 2e6f / 60));
    }

    if (ImGui::CollapsingHeader("Video Data Availability (mark=50%)")) {
        this->GetVBlankRecords(&vblank_records);
        ImGuiPlotLines("", &GetPercentage, &vblank_records, (int)vblank_records.size(), 0, nullptr, 0.f, 200.f, ImVec2(0, 100), ImVec2(0, 50));
    }

    if (ImGui::CollapsingHeader("BeebThread Timing Stats")) {
        BeebThread::TimingStats stats = beeb_thread->GetTimingStats();

        ImGui::Text("MQ polls: %zu", stats.num_mq_polls);
        ImGui::Text("MQ waits: %zu", stats.num_mq_waits);
    }

    if (!!g_all_root_timer_defs && !g_all_root_timer_defs->empty()) {
        ImGui::Separator();
        for (TimerDef *def : *g_all_root_timer_defs) {
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

#if MUTEX_DEBUGGING

    ImGui::Separator();

    std::vector<std::shared_ptr<MutexMetadata>> metadata = Mutex::GetAllMetadata();

    size_t num_skipped = 0;

    uint64_t runtime_ticks = GetCurrentTickCount() - APPROX_STARTUP_TICKS;
    ImGui::Text("Total run time: ~%.3f sec (~%.1f ms)", GetSecondsFromTicks(runtime_ticks), GetMillisecondsFromTicks(runtime_ticks));

    for (size_t i = 0; i < metadata.size(); ++i) {
        MutexMetadata *m = metadata[i].get();

        if (m->ever_locked) {
            MutexMetadataUI(m);
        } else {
            ++num_skipped;
        }
    }

    if (num_skipped > 0) {
        if (ImGui::CollapsingHeader("Never Locked")) {
            for (size_t i = 0; i < metadata.size(); ++i) {
                MutexMetadata *m = metadata[i].get();

                if (!m->ever_locked) {
                    MutexMetadataUI(m);
                }
            }
        }
    }

#endif
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool DataRateUI::OnClose() {
    return false;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void DataRateUI::GetVBlankRecords(std::vector<BeebWindow::VBlankRecord> *vblank_records) {
    if (vblank_records->empty()) {
        *vblank_records = m_beeb_window->GetVBlankRecords();
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void ResetTimerDefs() {
    if (!!g_all_root_timer_defs) {
        for (TimerDef *def : *g_all_root_timer_defs) {
            def->Reset();
        }
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

std::unique_ptr<SettingsUI> CreateDataRateUI(BeebWindow *beeb_window) {
    return std::make_unique<DataRateUI>(beeb_window);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
