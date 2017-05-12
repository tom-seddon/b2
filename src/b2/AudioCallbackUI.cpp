#include <shared/system.h>
#include "AudioCallbackUI.h"
#include "BeebWindow.h"
#include "BeebThread.h"
#include "dear_imgui.h"
#include <shared/debug.h>

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

AudioCallbackUI::AudioCallbackUI(BeebWindow *beeb_window):
    m_beeb_window(beeb_window)
{
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static float GetAudioCallbackRecordPercentage(void *data_,int idx) {
    auto data=(const std::vector<AudioCallbackRecord> *)data_;

    ASSERT(idx>=0&&(size_t)idx<data->size());
    const AudioCallbackRecord *record=&(*data)[(size_t)idx];

    if(record->needed==0) {
        return 0.f;
    } else {
        return (float)((double)record->available/record->needed*100.);
    }
}

void AudioCallbackUI::DoImGui() {
    std::shared_ptr<BeebThread> beeb_thread=m_beeb_window->GetBeebThread();

    std::vector<AudioCallbackRecord> records=beeb_thread->GetAudioCallbackRecords();
    ImGui::PlotLines("Data Availability",&GetAudioCallbackRecordPercentage,&records,(int)records.size(),0,nullptr,0.f,120.,ImVec2(0,100));
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
