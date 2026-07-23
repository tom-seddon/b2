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
        double total_num_seconds = GetSecondsFromTicks(total_num_ticks);
        ImGui::Text("%.3f sec (%.3f " MICROSECONDS_UTF8 ") tot", total_num_seconds, total_num_seconds * 1000.);
        ImGui::Text("%.3f " MICROSECONDS_UTF8 " mean", GetSecondsFromTicks((uint64_t)((double)total_num_ticks / num_samples)) * 1.e6);

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

static void MutexMetadataUI(MutexMetadata *m, const MutexDetails *details) {
    ImGuiIDPusher pusher(m);

    uint64_t num_ticks = GetCurrentTickCount() - details->stats.start_ticks;

    ImGui::Spacing();
    ImGui::Text("Mutex Name: %s", details->name.c_str());

    ImGui::TextUnformatted("Interesting:");
    {
        uint8_t events = m->GetInterestingEvents();

        ImGui::SameLine();
        ImGuiCheckboxFlags("Locks", &events, MutexInterestingEvent_Lock);
        ImGui::SameLine();
        ImGuiCheckboxFlags("Contended locks", &events, MutexInterestingEvent_ContendedLock);

        m->SetInterestingEvents(events);
    }

    ImGui::Text("Locks: %" PRIu64 " (~%.1f/sec)", details->stats.num_locks, num_ticks == 0 ? 0 : details->stats.num_locks / GetSecondsFromTicks(num_ticks));
    if (details->stats.num_locks > 0) {
        ImGui::Text("Contended Locks: %" PRIu64 " (%.3f%%)", details->stats.num_contended_locks, details->stats.num_locks == 0 ? 0. : (double)details->stats.num_contended_locks / details->stats.num_locks * 100.);

        ImGui::Text("Lock Wait Time: %.01f ms (~%.1f%% total)",
                    GetMillisecondsFromTicks(details->stats.total_lock_wait_ticks),
                    details->stats.total_lock_wait_ticks / (double)num_ticks * 100.);

        ImGui::Text("Lock Wait Stats: Min: %.01f ms; Max: %0.1f ms; Mean: %.01f ms",
                    GetMillisecondsFromTicks(details->stats.min_lock_wait_ticks),
                    GetMillisecondsFromTicks(details->stats.max_lock_wait_ticks),
                    GetMillisecondsFromTicks(details->stats.total_lock_wait_ticks) / details->stats.num_locks);

    } else {
        ImGui::TextUnformatted("Contended Locks: N/A");
        ImGui::TextUnformatted("Lock Wait Time: N/A");
        ImGui::TextUnformatted("Lock Wait Stats: N/A");
    }

    if (details->stats.num_try_locks > 0) {
        ImGui::Text("Successful Try Locks: %" PRIu64 "/%" PRIu64, details->stats.num_successful_try_locks, details->stats.num_try_locks);
    }

    if (details->stats.ever_locked) {
        if (ImGui::Button("Reset Stats")) {
            m->RequestReset();
        }
    }
}
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if MUTEX_DEBUGGING
struct MutexUIContext {
    uint64_t tick_count = 0;
    uint64_t total_lock_wait_ticks = 0;
};
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if MUTEX_DEBUGGING
static void DoCommonMutexMetadataUI(MutexUIContext *context, const std::vector<std::shared_ptr<MutexMetadata>> &mutex_metadata, const std::vector<MutexDetails> &mutex_details) {
    context->tick_count = GetCurrentTickCount();

    context->total_lock_wait_ticks = 0;
    for (const MutexDetails &details : mutex_details) {
        context->total_lock_wait_ticks += details.stats.total_lock_wait_ticks;
    }

    uint64_t runtime_ticks = context->tick_count - APPROX_STARTUP_TICKS;
    ImGui::Text("Total run time: ~%.3f sec (~%.1f ms)", GetSecondsFromTicks(runtime_ticks), GetMillisecondsFromTicks(runtime_ticks));

    ImGui::Text("Total lock wait time: ~%.3f sec (~%.1f ms) (%.3f%%)", GetSecondsFromTicks(context->total_lock_wait_ticks), GetMillisecondsFromTicks(context->total_lock_wait_ticks), context->total_lock_wait_ticks / (double)runtime_ticks * 100.);

    uint64_t name_overhead_ticks = Mutex::GetNameOverheadTicks();
    ImGui::Text("Mutex Name Overhead: ~%.3f sec (~%.1f ms) (%.3f%%)", GetSecondsFromTicks(name_overhead_ticks), GetMillisecondsFromTicks(name_overhead_ticks), name_overhead_ticks / (double)runtime_ticks * 100.);

    if (ImGui::Button("Reset all stats")) {
        for (const std::shared_ptr<MutexMetadata> &m : mutex_metadata) {
            m->RequestReset();
        }
    }
}
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

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
                ImGuiIDPusher id_pusher(metric_set.get());

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

    if (ImGui::CollapsingHeader("Mutexes")) {
        bool assume_free_uncontended_locks = Mutex::GetAssumeFreeUncontendedLocks();
        if (ImGui::Checkbox("Assume uncontended locks are free", &assume_free_uncontended_locks)) {
            Mutex::SetAssumeFreeUncontendedLocks(assume_free_uncontended_locks);
        }

        std::vector<std::shared_ptr<MutexMetadata>> metadata = Mutex::GetAllMetadata();

        std::vector<MutexDetails> mutex_details(metadata.size());
        for (size_t i = 0; i < metadata.size(); ++i) {
            metadata[i]->GetDetails(&mutex_details[i]);
        }

        MutexUIContext context;
        DoCommonMutexMetadataUI(&context, metadata, mutex_details);

        size_t num_never_locked = 0;
        size_t num_0_locks = 0;

        for (size_t i = 0; i < metadata.size(); ++i) {
            MutexMetadata *m = metadata[i].get();
            const MutexDetails *details = &mutex_details[i];

            if (details->stats.ever_locked) {
                if (details->stats.num_locks == 0) {
                    ++num_0_locks;
                } else {
                    ImGui::Separator();
                    MutexMetadataUI(m, details);
                }
            } else {
                ++num_never_locked;
            }
        }

        if (num_0_locks > 0) {
            if (ImGui::CollapsingHeader("0 locks since stats reset")) {
                for (size_t i = 0; i < metadata.size(); ++i) {
                    MutexMetadata *m = metadata[i].get();
                    const MutexDetails *details = &mutex_details[i];

                    if (details->stats.ever_locked && details->stats.num_locks == 0) {
                        ImGui::Separator();
                        MutexMetadataUI(m, details);
                    }
                }
            }
        }

        if (num_never_locked > 0) {
            if (ImGui::CollapsingHeader("Never locked ever")) {
                for (size_t i = 0; i < metadata.size(); ++i) {
                    MutexMetadata *m = metadata[i].get();
                    const MutexDetails *details = &mutex_details[i];

                    if (!details->stats.ever_locked) {
                        ImGui::Separator();
                        MutexMetadataUI(m, details);
                    }
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

// When initialising the table, set each column's ColumnUserID to a unique value
// of a specific type (presumably an enum).
//
// If the sort specs is dirty, initialise one of these, pointing it at the order
// table and the data table.
//
// Call SortColumn once for each column in turn, passing in the spec, the unique
// value, and the less than callable. SortColumn will do the if, and handle the
// ascending/descending aspect.

template <class ValueType>
class ColumnSorter {
  public:
    ColumnSorter(std::vector<size_t> *order_table, const std::vector<ValueType> *cont)
        : m_order_table(order_table)
        , m_cont(cont) {
    }

    template <class ColumnType, class LessThanType>
    void SortColumn(const ImGuiTableColumnSortSpecs *spec, ColumnType column, const LessThanType &less_than) {
        if (static_cast<ColumnType>(spec->ColumnUserID) == column) {
            if (spec->SortDirection == ImGuiSortDirection_Ascending) {
                std::stable_sort(m_order_table->begin(),
                                 m_order_table->end(),
                                 [this, spec, &less_than](size_t index_a, size_t index_b) -> bool {
                                     ASSERT(index_a < m_cont->size());
                                     ASSERT(index_b < m_cont->size());

                                     return less_than((*m_cont)[index_a], (*m_cont)[index_b]);
                                 });
            } else if (spec->SortDirection == ImGuiSortDirection_Descending) {
                std::stable_sort(m_order_table->begin(),
                                 m_order_table->end(),
                                 [this, spec, &less_than](size_t index_a, size_t index_b) -> bool {
                                     ASSERT(index_a < m_cont->size());
                                     ASSERT(index_b < m_cont->size());

                                     return less_than((*m_cont)[index_b], (*m_cont)[index_a]);
                                 });
            }
        }
    }

  protected:
  private:
    std::vector<size_t> *m_order_table = nullptr;
    const std::vector<ValueType> *m_cont = nullptr;
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if MUTEX_DEBUGGING

class MutexStatsUI : public SettingsUI {
  public:
    explicit MutexStatsUI();

    void DoImGui() override;

    bool OnClose() override;

  protected:
  private:
    uint64_t m_last_mutex_metadata_change_counter = 0;
    std::vector<std::shared_ptr<MutexMetadata>> m_mutex_metadata;
    std::vector<size_t> m_mutex_metadata_order_table;
};

enum class MutexTableColumn : ImGuiID {
    Name,
    LockCount,
    LockFrequency,
    ContendedLockCount,
    ContendedLockFrequency,
    LockWaitTime,
    EverLocked,

    Count //must be last
};

//ImGui::Text("Locks: %" PRIu64 " (~%.1f/sec)", stats->num_locks, num_ticks == 0 ? 0 : stats->num_locks / GetSecondsFromTicks(num_ticks));

static double GetMutexDetailsLockFrequency(const MutexDetails &details, uint64_t current_tick_count) {
    uint64_t num_ticks = current_tick_count - details.stats.start_ticks;
    if (num_ticks == 0) {
        return 0.;
    } else {
        return details.stats.num_locks / GetSecondsFromTicks(num_ticks);
    }
}

static double GetMutexDetailsContendedLockFrequency(const MutexDetails &details, uint64_t current_tick_count) {
    uint64_t num_ticks = current_tick_count - details.stats.start_ticks;
    if (num_ticks == 0) {
        return 0.;
    } else {
        return details.stats.num_contended_locks / GetSecondsFromTicks(num_ticks);
    }
}

static void DoLockFrequencyColumnImGui(double hz) {
    ImGui::Text("%.3f", hz);
    if (ImGui::IsItemHovered()) {
        ImGui::BeginTooltip();
        ImGui::Text("%.3f/sec", hz);
        ImGui::Text("%.3f/ms", hz / 1000.);
        ImGui::EndTooltip();
    }
}

MutexStatsUI::MutexStatsUI() {
    this->SetDefaultSize(ImVec2(550, 450));
}

void MutexStatsUI::DoImGui() {
    bool table_updated = false;
    uint64_t change_counter = Mutex::GetMetadataChangeCounter();
    if (change_counter != m_last_mutex_metadata_change_counter) {
        m_last_mutex_metadata_change_counter = change_counter;
        m_mutex_metadata = Mutex::GetAllMetadata();

        m_mutex_metadata_order_table.resize(m_mutex_metadata.size());
        for (size_t i = 0; i < m_mutex_metadata.size(); ++i) {
            m_mutex_metadata_order_table[i] = i;
        }
        table_updated = true;
    }

    std::vector<MutexDetails> mutex_details(m_mutex_metadata.size());
    for (size_t i = 0; i < m_mutex_metadata.size(); ++i) {
        m_mutex_metadata[i]->GetDetails(&mutex_details[i]);
    }

    MutexUIContext context;
    DoCommonMutexMetadataUI(&context, m_mutex_metadata, mutex_details);

    const uint32_t table_flags = ImGuiTableFlags_Resizable | ImGuiTableFlags_Reorderable | ImGuiTableFlags_Hideable | ImGuiTableFlags_ScrollY | ImGuiTableFlags_Sortable | ImGuiTableFlags_SortMulti;
    if (ImGui::BeginTable("mutexes", (int)MutexTableColumn::Count, table_flags)) {
        ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthFixed, 0.f, (ImGuiID)MutexTableColumn::Name);
        ImGui::TableSetupColumn("Locks", ImGuiTableColumnFlags_WidthFixed, 0.f, (ImGuiID)MutexTableColumn::LockCount);
        ImGui::TableSetupColumn("Locks/sec", ImGuiTableColumnFlags_WidthFixed, 0.f, (ImGuiID)MutexTableColumn::LockFrequency);
        ImGui::TableSetupColumn("C'locks", ImGuiTableColumnFlags_WidthFixed, 0.f, (ImGuiID)MutexTableColumn::ContendedLockCount);
        ImGui::TableSetupColumn("C'locks/sec", ImGuiTableColumnFlags_WidthFixed, 0.f, (ImGuiID)MutexTableColumn::ContendedLockFrequency);
        ImGui::TableSetupColumn("Wait Time (ms)", ImGuiTableColumnFlags_WidthFixed, 0.f, (ImGuiID)MutexTableColumn::LockWaitTime);
        ImGui::TableSetupColumn("Ever", ImGuiTableColumnFlags_WidthFixed, 0.f, (ImGuiID)MutexTableColumn::EverLocked);

        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableHeadersRow();

        if (ImGuiTableSortSpecs *specs = ImGui::TableGetSortSpecs()) {
            if (specs->SpecsDirty || table_updated) {
                specs->SpecsDirty = false;

                ColumnSorter<MutexDetails> sorter(&m_mutex_metadata_order_table, &mutex_details);

                for (int spec_index = 0; spec_index < specs->SpecsCount; ++spec_index) {
                    const ImGuiTableColumnSortSpecs *spec = &specs->Specs[spec_index];

                    sorter.SortColumn(spec,
                                      MutexTableColumn::Name,
                                      [](const MutexDetails &a, const MutexDetails &b) -> bool {
                                          return a.name < b.name;
                                      });

                    sorter.SortColumn(spec,
                                      MutexTableColumn::LockCount,
                                      [](const MutexDetails &a, const MutexDetails &b) -> bool {
                                          return a.stats.num_locks < b.stats.num_locks;
                                      });

                    sorter.SortColumn(spec,
                                      MutexTableColumn::LockFrequency,
                                      [tick_count = context.tick_count](const MutexDetails &a, const MutexDetails &b) -> bool {
                                          double fa = GetMutexDetailsLockFrequency(a, tick_count);
                                          double fb = GetMutexDetailsLockFrequency(b, tick_count);
                                          return fa < fb;
                                      });

                    sorter.SortColumn(spec,
                                      MutexTableColumn::ContendedLockCount,
                                      [](const MutexDetails &a, const MutexDetails &b) -> bool {
                                          return a.stats.num_contended_locks < b.stats.num_contended_locks;
                                      });

                    sorter.SortColumn(spec,
                                      MutexTableColumn::ContendedLockFrequency,
                                      [tick_count = context.tick_count](const MutexDetails &a, const MutexDetails &b) -> bool {
                                          double fa = GetMutexDetailsContendedLockFrequency(a, tick_count);
                                          double fb = GetMutexDetailsContendedLockFrequency(b, tick_count);
                                          return fa < fb;
                                      });

                    sorter.SortColumn(spec,
                                      MutexTableColumn::LockWaitTime,
                                      [](const MutexDetails &a, const MutexDetails &b) -> bool {
                                          return a.stats.total_lock_wait_ticks < b.stats.total_lock_wait_ticks;
                                      });

                    sorter.SortColumn(spec,
                                      MutexTableColumn::EverLocked,
                                      [](const MutexDetails &a, const MutexDetails &b) -> bool {
                                          if (!a.stats.ever_locked && b.stats.ever_locked) {
                                              return true;
                                          } else {
                                              return false;
                                          }
                                      });
                }
            }
        }

        for (size_t i = 0; i < m_mutex_metadata_order_table.size(); ++i) {
            static const char POPUP_NAME[] = "mutex_context_popup";

            size_t mutex_index = m_mutex_metadata_order_table[i];

            const std::shared_ptr<MutexMetadata> &metadata = m_mutex_metadata[mutex_index];
            ImGuiIDPusher pusher(metadata.get());

            const MutexDetails *details = &mutex_details[mutex_index];

            char str[MAX_UINT64_THOUSANDS_SIZE];

            ImGui::TableNextRow();

            ImGui::TableNextColumn();
            ImGui::TextUnformatted(details->name.c_str());
            if (ImGui::IsItemHovered() && ImGui::IsMouseReleased(ImGuiMouseButton_Right)) {
                ImGui::OpenPopup(POPUP_NAME);
            }

            ImGui::TableNextColumn();
            GetThousandsString(str, details->stats.num_locks);
            ImGui::TextUnformatted(str);

            ImGui::TableNextColumn();
            DoLockFrequencyColumnImGui(GetMutexDetailsLockFrequency(*details, context.tick_count));

            ImGui::TableNextColumn();
            GetThousandsString(str, details->stats.num_contended_locks);
            ImGui::TextUnformatted(str);
            if (details->stats.num_locks > 0) {
                if (ImGui::IsItemHovered()) {
                    ImGui::BeginTooltip();
                    ImGui::Text("~%.3f%% of total", (double)details->stats.num_contended_locks / details->stats.num_locks * 100.);
                    ImGui::EndTooltip();
                }
            }

            ImGui::TableNextColumn();
            DoLockFrequencyColumnImGui(GetMutexDetailsContendedLockFrequency(*details, context.tick_count));

            ImGui::TableNextColumn();
            ImGui::Text("%.3f", GetMillisecondsFromTicks(details->stats.total_lock_wait_ticks));
            if (ImGui::IsItemHovered()) {
                ImGui::BeginTooltip();
                ImGui::Text("%.3f ms", GetMillisecondsFromTicks(details->stats.total_lock_wait_ticks));
                ImGui::Text("%.3f " MICROSECONDS_UTF8, GetMicrosecondsFromTicks(details->stats.total_lock_wait_ticks));
                if (context.total_lock_wait_ticks > 0) {
                    ImGui::Text("~%.3f%% of total wait", (double)details->stats.total_lock_wait_ticks / context.total_lock_wait_ticks * 100.);
                }
                ImGui::Separator();
                ImGui::Text("Min Wait: %.3f " MICROSECONDS_UTF8, GetMicrosecondsFromTicks(details->stats.min_lock_wait_ticks));
                ImGui::Text("Max Wait: %.3f " MICROSECONDS_UTF8, GetMicrosecondsFromTicks(details->stats.max_lock_wait_ticks));
                ImGui::Text("Mean Wait: %.3f " MICROSECONDS_UTF8, details->stats.num_locks == 0 ? 0. : details->stats.total_lock_wait_ticks / details->stats.num_locks);
                ImGui::EndTooltip();
            }

            ImGui::TableNextColumn();
            ImGui::TextUnformatted(BOOL_STR(details->stats.ever_locked));

            if (ImGui::BeginPopup(POPUP_NAME)) {
                ImGuiHeader("Interesting Events");
                uint8_t events = metadata->GetInterestingEvents();
                ImGuiCheckboxFlags("Locks", &events, MutexInterestingEvent_Lock);
                ImGuiCheckboxFlags("Contended Locks", &events, MutexInterestingEvent_ContendedLock);
                metadata->SetInterestingEvents(events);

                ImGui::EndPopup();
            }
        }

        ImGui::EndTable();
    }
}

bool MutexStatsUI::OnClose() {
    return false;
}

#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

std::unique_ptr<SettingsUI> CreateMutexStatsUI(BeebWindow *beeb_window) {
    (void)beeb_window;
#if MUTEX_DEBUGGING
    return std::make_unique<MutexStatsUI>();
#else
    return nullptr;
#endif
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

class EnumsUI : public SettingsUI {
  public:
    EnumsUI();

    void DoImGui() override;
    bool OnClose() override;

  protected:
  private:
    struct Enum {
        const EnumTraitsBase *traits = nullptr;
        std::vector<const EnumValue *> values;
        std::vector<size_t> values_order;
    };
    std::vector<Enum> m_enums;
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

enum class EnumTableColumn : ImGuiID {
    Name,
    Value,
    ValueHex,

    Count //must be last
};

enum class BitfieldTableColumn : ImGuiID {
    Name,
    Shift,
    Width,
    Mask,
    Enum,

    Count //must be last
};

static bool EnumValueLessThanByName(const EnumValue *lhs, const EnumValue *rhs) {
    return strcmp(lhs->name, rhs->name) < 0;
}

static bool EnumValueLessThanBySignedValue(const EnumValue *lhs, const EnumValue *rhs) {
    return (int64_t)lhs->value < (int64_t)rhs->value;
}

static bool EnumValueLessThanByUnsignedValue(const EnumValue *lhs, const EnumValue *rhs) {
    return lhs->value < rhs->value;
}

EnumsUI::EnumsUI() {
    this->SetDefaultSize({200.f, 100.f});

    for (const EnumTraitsBase *traits = EnumTraitsBase::GetFirst(); traits; traits = traits->next) {
        Enum e;

        e.traits = traits;

        for (const EnumValue *value = traits->first_value; value; value = value->next) {
            e.values.push_back(value);
            e.values_order.push_back(e.values_order.size());
        }

        m_enums.push_back(std::move(e));
    }

    std::sort(m_enums.begin(),
              m_enums.end(),
              [](const Enum &lhs, const Enum &rhs) -> bool {
                  return strcasecmp(lhs.traits->name, rhs.traits->name) < 0;
              });
}

void EnumsUI::DoImGui() {
    for (Enum &e : m_enums) {
        if (ImGui::CollapsingHeader(e.traits->name)) {
            ImGui::BulletText("Size: %zu bits (%f bytes)", e.traits->size_bits, e.traits->size_bits / 8.);
            ImGui::BulletText("Signed: %s", BOOL_STR(e.traits->is_signed));
            ImGui::BulletText("Bitfield: %s", BOOL_STR(e.traits->is_bitfield));

            if (e.traits->is_bitfield) {
                const uint32_t table_flags = ImGuiTableFlags_Resizable | ImGuiTableFlags_Reorderable | ImGuiTableFlags_Hideable | ImGuiTableFlags_Sortable | ImGuiTableFlags_SortMulti;

                if (ImGui::BeginTable("fields", (int)BitfieldTableColumn::Count, table_flags)) {
                    ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthFixed, 0.f, (ImGuiID)BitfieldTableColumn::Name);
                    ImGui::TableSetupColumn("Shift", ImGuiTableColumnFlags_WidthFixed, 0.f, (ImGuiID)BitfieldTableColumn::Shift);
                    ImGui::TableSetupColumn("Width", ImGuiTableColumnFlags_WidthFixed, 0.f, (ImGuiID)BitfieldTableColumn::Width);
                    ImGui::TableSetupColumn("Mask", ImGuiTableColumnFlags_WidthFixed, 0.f, (ImGuiID)BitfieldTableColumn::Mask);
                    ImGui::TableSetupColumn("Enum", ImGuiTableColumnFlags_WidthFixed, 0.f, (ImGuiID)BitfieldTableColumn::Enum);

                    ImGui::TableSetupScrollFreeze(0, 1);
                    ImGui::TableHeadersRow();
                    if (ImGuiTableSortSpecs *specs = ImGui::TableGetSortSpecs()) {
                        if (specs->SpecsDirty) {
                            specs->SpecsDirty = false;

                            ColumnSorter<const EnumValue *> sorter(&e.values_order, &e.values);

                            for (int spec_index = 0; spec_index < specs->SpecsCount; ++spec_index) {
                                const ImGuiTableColumnSortSpecs *spec = &specs->Specs[spec_index];

                                sorter.SortColumn(spec, BitfieldTableColumn::Name, &EnumValueLessThanByName);

                                sorter.SortColumn(spec,
                                                  BitfieldTableColumn::Shift,
                                                  [](const EnumValue *lhs, const EnumValue *rhs) -> bool {
                                                      return lhs->bit_shift < rhs->bit_shift;
                                                  });

                                sorter.SortColumn(spec,
                                                  BitfieldTableColumn::Width,
                                                  [](const EnumValue *lhs, const EnumValue *rhs) -> bool {
                                                      return lhs->bit_width < rhs->bit_width;
                                                  });

                                sorter.SortColumn(spec, BitfieldTableColumn::Mask, &EnumValueLessThanByUnsignedValue);

                                sorter.SortColumn(spec,
                                                  BitfieldTableColumn::Enum,
                                                  [](const EnumValue *lhs, const EnumValue *rhs) -> bool {
                                                      if (lhs->bit_enum && rhs->bit_enum) {
                                                          return strcmp(lhs->bit_enum->name, rhs->bit_enum->name) < 0;
                                                      } else if (rhs->bit_enum) {
                                                          return true;
                                                      } else {
                                                          return false;
                                                      }
                                                  });
                            }
                        }
                    }

                    for (size_t value_index : e.values_order) {
                        const EnumValue *value = e.values[value_index];

                        ImGui::TableNextRow();

                        ImGui::TableNextColumn();
                        ImGui::TextUnformatted(value->name);

                        ImGui::TableNextColumn();
                        ImGui::Text("%" PRId8, value->bit_shift);

                        ImGui::TableNextColumn();
                        ImGui::Text("%" PRIu8, value->bit_width);

                        ImGui::TableNextColumn();
                        ImGui::Text("%0.*" PRIx64, e.traits->width_xdigits, value->value);

                        ImGui::TableNextColumn();
                        if (value->bit_enum) {
                            ImGui::TextUnformatted(value->bit_enum->name);
                        }
                    }

                    ImGui::EndTable();
                }
            } else {
                const uint32_t table_flags = ImGuiTableFlags_Resizable | ImGuiTableFlags_Reorderable | ImGuiTableFlags_Hideable | ImGuiTableFlags_Sortable | ImGuiTableFlags_SortMulti;
                if (ImGui::BeginTable("values", (int)EnumTableColumn::Count, table_flags)) {
                    ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthFixed, 0.f, (ImGuiID)EnumTableColumn::Name);
                    ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthFixed, 0.f, (ImGuiID)EnumTableColumn::Value);
                    ImGui::TableSetupColumn("Value (hex)", ImGuiTableColumnFlags_WidthFixed, 0.f, (ImGuiID)EnumTableColumn::ValueHex);

                    ImGui::TableSetupScrollFreeze(0, 1);
                    ImGui::TableHeadersRow();

                    if (ImGuiTableSortSpecs *specs = ImGui::TableGetSortSpecs()) {
                        if (specs->SpecsDirty) {
                            specs->SpecsDirty = false;

                            ColumnSorter<const EnumValue *> sorter(&e.values_order, &e.values);

                            for (int spec_index = 0; spec_index < specs->SpecsCount; ++spec_index) {
                                const ImGuiTableColumnSortSpecs *spec = &specs->Specs[spec_index];

                                sorter.SortColumn(spec, EnumTableColumn::Name, &EnumValueLessThanByName);
                                sorter.SortColumn(spec, EnumTableColumn::Value, e.traits->is_signed ? &EnumValueLessThanBySignedValue : &EnumValueLessThanByUnsignedValue);
                                sorter.SortColumn(spec, EnumTableColumn::ValueHex, &EnumValueLessThanByUnsignedValue);
                            }
                        }
                    }

                    for (size_t value_index : e.values_order) {
                        const EnumValue *value = e.values[value_index];

                        ImGui::TableNextRow();

                        ImGui::TableNextColumn();
                        ImGui::TextUnformatted(value->name);

                        ImGui::TableNextColumn();
                        if (e.traits->is_signed) {
                            ImGui::Text("%" PRId64, (int64_t)value->value);
                        } else {
                            ImGui::Text("%" PRIu64, value->value);
                        }

                        ImGui::TableNextColumn();
                        ImGui::Text("%0*" PRIx64, e.traits->width_xdigits, value->value);
                    }

                    ImGui::EndTable();
                }
            }
        }
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool EnumsUI::OnClose() {
    return false;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

std::unique_ptr<SettingsUI> CreateEnumsUI(BeebWindow *beeb_window) {
    (void)beeb_window;
    return std::make_unique<EnumsUI>();
}
