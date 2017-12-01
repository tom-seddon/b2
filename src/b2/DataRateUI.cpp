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

class DataRateUI:
    public SettingsUI
{
public:
    explicit DataRateUI(BeebWindow *beeb_window);

    void DoImGui(CommandContextStack *cc_stack) override;

    bool OnClose() override;
protected:
private:
    BeebWindow *m_beeb_window;
};


//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

DataRateUI::DataRateUI(BeebWindow *beeb_window):
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

static float GetFrameTime(void *data_,int idx) {
    auto data=(const std::vector<BeebWindow::VBlankRecord> *)data_;

    ASSERT(idx>=0&&(size_t)idx<data->size());
    const BeebWindow::VBlankRecord *record=&(*data)[(size_t)idx];

    return (float)(GetSecondsFromTicks(record->num_ticks)*1000.);
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

static bool EverLocked(const MutexMetadata *m) {
    if(m->num_locks>0) {
        return true;
    }

    if(m->num_try_locks>0) {
        return true;
    }

    return false;
}

static void MutexMetadataUI(const MutexMetadata *m) {
    ImGui::Spacing();
    ImGui::Text("Mutex Name: %s",m->name.c_str());
    ImGui::Text("Locks: %" PRIu64,m->num_locks);
    ImGui::Text("Lock Wait Time: %.05f sec",GetSecondsFromTicks(m->total_lock_ticks));
    ImGui::Text("Successful Try Locks: %" PRIu64 "/%" PRIu64,m->num_successful_try_locks,m->num_try_locks);
}

void DataRateUI::DoImGui(CommandContextStack *cc_stack) {
    (void)cc_stack;

    std::shared_ptr<BeebThread> beeb_thread=m_beeb_window->GetBeebThread();

    ImGui::Separator();

    ImGui::TextUnformatted("Audio Data Availability (mark=25%)");
    std::vector<BeebThread::AudioCallbackRecord> audio_records=beeb_thread->GetAudioCallbackRecords();
    ImGuiPlotLines("",&GetAudioCallbackRecordPercentage,&audio_records,(int)audio_records.size(),0,nullptr,0.f,120.,ImVec2(0,100),ImVec2(0,25));

    ImGui::Separator();

    ImGui::TextUnformatted("PC VBlank Time (mark=1/60 sec)");
    std::vector<BeebWindow::VBlankRecord> vblank_records=m_beeb_window->GetVBlankRecords();
    ImGuiPlotLines("",&GetFrameTime,&vblank_records,(int)vblank_records.size(),0,nullptr,0.f,100.f,ImVec2(0,100),ImVec2(0,1000.f/60));

    ImGui::TextUnformatted("Video Data Consumed per PC VBlank (mark=1/60 sec)");
    ImGuiPlotLines("",&GetNumUnits,&vblank_records,(int)vblank_records.size(),0,nullptr,0.f,2e6f/15,ImVec2(0,100),ImVec2(0,2e6f/60));

    ImGui::TextUnformatted("Video Data Availability (mark=50%)");
    ImGuiPlotLines("",&GetPercentage,&vblank_records,(int)vblank_records.size(),0,nullptr,0.f,200.f,ImVec2(0,100),ImVec2(0,50));

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
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool DataRateUI::OnClose() {
    return false;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

std::unique_ptr<SettingsUI> CreateDataRateUI(BeebWindow *beeb_window) {
    return std::make_unique<DataRateUI>(beeb_window);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
