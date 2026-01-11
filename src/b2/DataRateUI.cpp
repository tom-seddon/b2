#include <shared/system.h>
#include "DataRateUI.h"
#include "BeebWindow.h"
#include "BeebThread.h"
#include "dear_imgui.h"
#include <shared/debug.h>
#include "SettingsUI.h"
#include <inttypes.h>
#include "b2.h"
#include <shared/metrics.h>

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static uint64_t APPROX_STARTUP_TICKS = GetCurrentTickCount(); //doesn't need to be perfect...

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static void DoTimerDefImGui(const TimerDef *def) {
    if (ImGui::TreeNode(def->name.c_str())) {
        uint64_t total_num_ticks = def->GetTotalNumTicks();
        uint64_t num_samples = def->GetNumSamples();
        ImGui::Text("%.3f sec tot", GetSecondsFromTicks(total_num_ticks));
        ImGui::Text("%.3f " MICROSECONDS_UTF8 " avg", GetSecondsFromTicks((uint64_t)((double)total_num_ticks / num_samples)) * 1.e6);

        if (const TimerDef *parent = def->GetParent()) {
            ImGui::Text("%.3f%% of parent", (double)total_num_ticks / parent->GetTotalNumTicks() * 100.);
        }

        std::vector<const TimerDef *> children = def->GetChildren();
        if (!children.empty()) {
            uint64_t total_child_ticks = 0;
            for (const TimerDef *child_def : children) {
                DoTimerDefImGui(child_def);
                total_child_ticks += child_def->GetTotalNumTicks();
            }

            ImGui::Text("%.3f%% non-child time", ((double)total_num_ticks - total_child_ticks) / total_num_ticks * 100.);
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
    std::map<uint32_t, std::string> m_name_by_update_flags;

    void GetVBlankRecords(std::vector<BeebWindow::VBlankRecord> *vblank_records);
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

DataRateUI::DataRateUI(BeebWindow *beeb_window)
    : m_beeb_window(beeb_window) {
    this->SetDefaultSize(ImVec2(550, 450));
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

static void MutexMetadataUI(MutexMetadata *m, const MutexStats *stats) {
    ImGuiIDPusher pusher(m);

    uint64_t num_ticks = GetCurrentTickCount() - stats->start_ticks;

    ImGui::Spacing();
    ImGui::Text("Mutex Name: %s", stats->name.c_str());

    ImGui::TextUnformatted("Interesting:");
    {
        uint8_t events = m->GetInterestingEvents();

        ImGui::SameLine();
        ImGuiCheckboxFlags("Locks", &events, MutexInterestingEvent_Lock);
        ImGui::SameLine();
        ImGuiCheckboxFlags("Contended locks", &events, MutexInterestingEvent_ContendedLock);

        m->SetInterestingEvents(events);
    }

    ImGui::Text("Locks: %" PRIu64 " (~%.1f/sec)", stats->num_locks, num_ticks == 0 ? 0 : stats->num_locks / GetSecondsFromTicks(num_ticks));
    if (stats->num_locks > 0) {
        ImGui::Text("Contended Locks: %" PRIu64 " (%.3f%%)", stats->num_contended_locks, stats->num_locks == 0 ? 0. : (double)stats->num_contended_locks / stats->num_locks);

        ImGui::Text("Lock Wait Time: %.01f ms (~%.1f%% total)",
                    GetMillisecondsFromTicks(stats->total_lock_wait_ticks),
                    stats->total_lock_wait_ticks / (double)num_ticks * 100.);

        ImGui::Text("Lock Wait Stats: Min: %.01f ms; Max: %0.1f ms; Mean: %.01f ms",
                    GetMillisecondsFromTicks(stats->min_lock_wait_ticks),
                    GetMillisecondsFromTicks(stats->max_lock_wait_ticks),
                    GetMillisecondsFromTicks(stats->total_lock_wait_ticks) / stats->num_locks);

    } else {
        ImGui::TextUnformatted("Contended Locks: N/A");
        ImGui::TextUnformatted("Lock Wait Time: N/A");
        ImGui::TextUnformatted("Lock Wait Stats: N/A");
    }

    if (stats->num_try_locks > 0) {
        ImGui::Text("Successful Try Locks: %" PRIu64 "/%" PRIu64, stats->num_successful_try_locks, stats->num_try_locks);
    }

    if (stats->ever_locked) {
        if (ImGui::Button("Reset Stats")) {
            m->RequestReset();
        }
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

#if BBCMICRO_DEBUGGER
    if (ImGui::CollapsingHeader("Update MFn Stats")) {
        std::shared_ptr<const BBCMicro::UpdateMFnData> data = beeb_thread->GetUpdateMFnData();
        ImGui::Text("Update MFn changes: %" PRIu64, data->num_update_mfn_changes);
        ImGui::Text("UpdateCpuDataBusFn calls: %" PRIu64 " (~%.2f/sec)",
                    data->num_UpdateCpuDataBusFn_calls,
                    data->num_UpdateCpuDataBusFn_calls / ((double)beeb_thread->GetEmulatedCycles().n / CYCLES_PER_SECOND));
        ImGui::Text("UpdatePaging calls: %" PRIu64 " (~%.2f/sec)",
                    data->num_UpdatePaging_calls,
                    data->num_UpdatePaging_calls / ((double)beeb_thread->GetEmulatedCycles().n / CYCLES_PER_SECOND));

        for (uint32_t i = 0; i < NUM_BBCMICRO_UPDATE_MFNS; ++i) {
            if (data->update_mfn_cycle_count[i].n > 0) {
                char cycles_str[MAX_UINT64_THOUSANDS_SIZE];
                GetThousandsString(cycles_str, data->update_mfn_cycle_count[i].n);

                std::string *name = &m_name_by_update_flags[i];
                if (name->empty()) {
                    *name = BBCMicro::GetUpdateFlagExpr(i);
                }

                ImGui::Text("%s: %s", cycles_str, name->c_str());
            }
        }
    }
#endif

    std::vector<std::shared_ptr<MetricSet>> metric_sets = MetricSet::GetAll();
    if (metric_sets.empty()) {
        if (ImGui::CollapsingHeader("Metrics")) {
        }
    } else {
        for (const std::shared_ptr<MetricSet> &metric_set : metric_sets) {
            if (ImGui::CollapsingHeader(("Metrics: " + metric_set->GetName()).c_str())) {
                std::vector<const TimerDef *> roots = metric_set->GetRootTimerDefs();
                if (!roots.empty()) {
                    if (ImGui::Button("Reset Timers")) {
                        metric_set->ResetTimerDefs();
                    }

                    ImGui::Separator();
                    for (const TimerDef *root : roots) {
                        DoTimerDefImGui(root);
                    }
                }

                std::vector<const Value *> values = metric_set->GetValues();
                if (!values.empty()) {
                    if (ImGui::Button("Reset Counters")) {
                        metric_set->ResetCounters();
                    }

                    ImGui::Separator();
                    for (const Value *value : values) {
                        ImGui::Text("%s: %" PRIu64, value->name.c_str(), value->GetValue());
                    }
                }
            }
        }
    }

    //if (ImGui::CollapsingHeader("Timers")) {
    //    if (ImGui::Button("Reset all")) {
    //        ResetTimerDefs();
    //    }

    //    std::vector<const TimerDef *> roots = GetRootTimerDefs();
    //    if (!roots.empty()) {
    //        ImGui::Separator();
    //        for (const TimerDef *root : roots) {
    //            DoTimerDefImGui(root);
    //        }
    //    }
    //}

#if MUTEX_DEBUGGING

    ImGuiHeader("Mutexes");

    bool assume_free_uncontended_locks = Mutex::GetAssumeFreeUncontendedLocks();
    if (ImGui::Checkbox("Assume uncontended locks are free", &assume_free_uncontended_locks)) {
        Mutex::SetAssumeFreeUncontendedLocks(assume_free_uncontended_locks);
    }

    std::vector<std::shared_ptr<MutexMetadata>> metadata = Mutex::GetAllMetadata();

    std::vector<MutexStats> mutex_stats(metadata.size());
    for (size_t i = 0; i < metadata.size(); ++i) {
        metadata[i]->GetStats(&mutex_stats[i]);
    }

    uint64_t runtime_ticks = GetCurrentTickCount() - APPROX_STARTUP_TICKS;
    ImGui::Text("Total run time: ~%.3f sec (~%.1f ms)", GetSecondsFromTicks(runtime_ticks), GetMillisecondsFromTicks(runtime_ticks));

    uint64_t total_lock_wait_ticks = 0;
    for (const MutexStats &stats : mutex_stats) {
        total_lock_wait_ticks += stats.total_lock_wait_ticks;
    }
    ImGui::Text("Total lock wait time: ~%.3f sec (~%.1f ms) (%.3f%%)", GetSecondsFromTicks(total_lock_wait_ticks), GetMillisecondsFromTicks(total_lock_wait_ticks), total_lock_wait_ticks / (double)runtime_ticks * 100.);

    uint64_t name_overhead_ticks = Mutex::GetNameOverheadTicks();
    ImGui::Text("Mutex Name Overhead: ~%.3f sec (~%.1f ms) (%.3f%%)", GetSecondsFromTicks(name_overhead_ticks), GetMillisecondsFromTicks(name_overhead_ticks), name_overhead_ticks / (double)runtime_ticks * 100.);

    if (ImGui::Button("Reset all stats")) {
        for (const std::shared_ptr<MutexMetadata> &m : metadata) {
            m->RequestReset();
        }
    }

    size_t num_never_locked = 0;
    size_t num_0_locks = 0;

    for (size_t i = 0; i < metadata.size(); ++i) {
        MutexMetadata *m = metadata[i].get();
        const MutexStats *stats = &mutex_stats[i];

        if (stats->ever_locked) {
            if (stats->num_locks == 0) {
                ++num_0_locks;
            } else {
                ImGui::Separator();
                MutexMetadataUI(m, stats);
            }
        } else {
            ++num_never_locked;
        }
    }

    if (num_0_locks > 0) {
        if (ImGui::CollapsingHeader("0 locks since stats reset")) {
            for (size_t i = 0; i < metadata.size(); ++i) {
                MutexMetadata *m = metadata[i].get();
                const MutexStats *stats = &mutex_stats[i];

                if (stats->ever_locked && stats->num_locks == 0) {
                    ImGui::Separator();
                    MutexMetadataUI(m, stats);
                }
            }
        }
    }

    if (num_never_locked > 0) {
        if (ImGui::CollapsingHeader("Never locked ever")) {
            for (size_t i = 0; i < metadata.size(); ++i) {
                MutexMetadata *m = metadata[i].get();
                const MutexStats *stats = &mutex_stats[i];

                if (!stats->ever_locked) {
                    ImGui::Separator();
                    MutexMetadataUI(m, stats);
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

std::unique_ptr<SettingsUI> CreateDataRateUI(BeebWindow *beeb_window) {
    return std::make_unique<DataRateUI>(beeb_window);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
