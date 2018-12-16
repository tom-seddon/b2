#include <shared/system.h>
#include "TimelineUI.h"
#include "dear_imgui.h"
#include "misc.h"
#include "GenerateThumbnailJob.h"
#include "BeebState.h"
#include "BeebWindows.h"
#include "BeebThread.h"
#include "SettingsUI.h"
#include <IconsFontAwesome5.h>
#include "ThumbnailsUI.h"

////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////

class TimelineUI:
public SettingsUI
{
public:
    explicit TimelineUI(BeebWindow *beeb_window,
                        SDL_Renderer *renderer,
                        const SDL_PixelFormat *pixel_format):
    m_beeb_window(beeb_window),
    m_thumbnails(renderer,pixel_format)
    {
    }

    void DoImGui(CommandContextStack *cc_stack) override {
        auto beeb_thread=m_beeb_window->GetBeebThread();

        BeebThread::TimelineState timeline_state;
        beeb_thread->GetTimelineState(&timeline_state);

        ASSERT(timeline_state.end_2MHz_cycles>=timeline_state.begin_2MHz_cycles);
        const uint64_t timeline_duration=timeline_state.end_2MHz_cycles-timeline_state.begin_2MHz_cycles;

        ImGui::Text("Timeline State: %s",GetBeebThreadTimelineStateEnumName(timeline_state.state));

        ImGui::Checkbox("Follow new events",&m_follow);

        // Back, Stop, Play/Pause, Forward
        //
        // https://fontawesome.com/icons?d=gallery

        switch(timeline_state.state) {
            default:
                ASSERT(false);
                // fall through
            case BeebThreadTimelineState_None:
            {
                ASSERT(timeline_state.end_2MHz_cycles>=timeline_state.begin_2MHz_cycles);
                uint64_t duration=timeline_state.end_2MHz_cycles-timeline_state.begin_2MHz_cycles;

                // Record
                if(timeline_state.can_record) {
                    if(ImGuiConfirmButton("Record",timeline_duration>0)) {
                        beeb_thread->Send(std::make_shared<BeebThread::StartRecordingMessage>());
                    }
                }

                if(duration>0) {
                    // Clear
                    ImGui::SameLine();
                    if(ImGuiConfirmButton("Delete")) {
                        beeb_thread->Send(std::make_shared<BeebThread::ClearRecordingMessage>());
                    }
                }

                if(!timeline_state.can_record) {
                    std::string message="Recording disabled due to non-cloneable drives:";
                    for(int i=0;i<NUM_DRIVES;++i) {
                        if(timeline_state.non_cloneable_drives&1<<i) {
                            message+=strprintf(" %d",i);
                        }
                    }

                    ImGui::TextUnformatted(message.c_str());
                }
            }
                break;

            case BeebThreadTimelineState_Replay:
            {
                // Stop
                if(ImGui::Button("Stop")) {
                }

                // Pause
                ImGui::SameLine();
                ImGui::Button("Pause");
            }
                break;

            case BeebThreadTimelineState_Record:
            {
                // Stop
                if(ImGui::Button("Stop")) {
                    beeb_thread->Send(std::make_shared<BeebThread::StopRecordingMessage>());
                }
            }
                break;
        }

        switch(timeline_state.state) {
            default:
                ASSERT(false);
                // fall through
            case BeebThreadTimelineState_None:
                // fall through
            case BeebThreadTimelineState_Record:
            {
                if(timeline_duration==0) {
                    ImGui::Text("No recording.");
                } else {
                    ImGui::Text("Recorded: %zu events, %s",
                                timeline_state.num_events,
                                Get2MHzCyclesString(timeline_duration).c_str());
                }
            }
                break;

            case BeebThreadTimelineState_Replay:
                break;
        }

        const ImVec2 THUMBNAIL_SIZE=m_thumbnails.GetThumbnailSize();
        const float GAP_HEIGHT=20;
        const float CELL_HEIGHT=THUMBNAIL_SIZE.y+3*ImGui::GetTextLineHeight()+GAP_HEIGHT;

        //std::vector<BeebThread::TimelineBeebStateEvent> beeb_state_events;


//        ASSERT(timeline_state.end_2MHz_cycles>=timeline_state.begin_2MHz_cycles);
//        int num_seconds=(timeline_state.end_2MHz_cycles-timeline_state.begin_2MHz_cycles+1999999)/2000000;
//
//        int num_columns=(int)ceil(ImGui::GetContentRegionAvailWidth()/(cell_size.x));
//        int num_rows=(int)ceil((double)num_seconds/num_columns);
//
//        ImGui::Text("num_seconds=%d num_columns=%d num_rows=%d",num_seconds,num_columns,num_rows);
//
//        char cycles_str[MAX_UINT64_THOUSANDS_LEN];
//
//        GetThousandsString(cycles_str,timeline_state.begin_2MHz_cycles);
//        ImGui::Text("begin: %s (%s)",cycles_str,Get2MHzCyclesString(timeline_state.begin_2MHz_cycles).c_str());
//
//        GetThousandsString(cycles_str,timeline_state.end_2MHz_cycles);
//        ImGui::Text("end: %s (%s)",cycles_str,Get2MHzCyclesString(timeline_state.end_2MHz_cycles).c_str());

        ImGui::Text("%zu thumbnails; %zu textures",
                    m_thumbnails.GetNumThumbnails(),
                    m_thumbnails.GetNumTextures());

        ImGui::BeginChild("timeline_container",
                          ImVec2(0,0),
                          true,//border
                          ImGuiWindowFlags_AlwaysVerticalScrollbar);

        ImVec2 timeline_size(0,timeline_state.num_beeb_state_events*CELL_HEIGHT);

        if(m_follow) {
            if(timeline_state.state==BeebThreadTimelineState_Record) {
                ImGui::SetScrollY(timeline_size.y);
            }
        }

        float scroll_y=ImGui::GetScrollY();

        {
            ImGui::BeginChild("timeline",timeline_size,false,0);//false = no border

            // not too concerned about overflow here...
            int display_row_begin,display_row_end;
            ImGui::CalcListClipping((int)timeline_state.num_beeb_state_events,
                                    CELL_HEIGHT,
                                    &display_row_begin,
                                    &display_row_end);

            std::vector<BeebThread::TimelineBeebStateEvent> beeb_state_events=
            beeb_thread->GetTimelineBeebStateEvents((size_t)display_row_begin,
                                                    (size_t)display_row_end);

            for(size_t i=0;i<beeb_state_events.size();++i) {
                const BeebThread::TimelineBeebStateEvent *e=&beeb_state_events[i];
                ImGuiIDPusher pusher(e->message.get());
                const std::shared_ptr<const BeebState> state=e->message->GetBeebState();
                int row=display_row_begin+(int)i;

                ImGui::SetCursorPos(ImVec2(0.f,row*CELL_HEIGHT));
//                ImGui::Text("Row %d",row);

                char cycles_str[MAX_UINT64_THOUSANDS_LEN];
                GetThousandsString(cycles_str,e->time_2MHz_cycles);

                if(ImGuiConfirmButton("Load")) {
                    beeb_thread->Send(std::make_shared<BeebThread::LoadTimelineStateMessage>(state,
                                                                                             true));
                }

                ImGui::SameLine();

                if(ImGuiConfirmButton("Delete")) {
                    beeb_thread->Send(std::make_shared<BeebThread::DeleteTimelineStateMessage>(state));
                }

                ImGui::SameLine();

                if(ImGui::Button("Replay")) {
                    beeb_thread->Send(std::make_shared<BeebThread::StartReplayMessage>(state));
                }

                ImGui::Text("%s (%s)",cycles_str,Get2MHzCyclesString(e->time_2MHz_cycles).c_str());

                const std::string &name=state->GetName();
                if(name.empty()) {
                    ImGui::TextUnformatted("(no name)");
                } else {
                    ImGui::Text("Name: %s",name.c_str());
                }

                m_thumbnails.Thumbnail(e->message->GetBeebState());
            }
            ImGui::EndChild();
        }
        ImGui::EndChild();

        m_thumbnails.Update();
    }

    bool OnClose() override {
        return false;
    }
protected:
private:
    BeebWindow *m_beeb_window=nullptr;
    ThumbnailsUI m_thumbnails;
    bool m_follow=true;
};

////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////

std::unique_ptr<SettingsUI> CreateTimelineUI(BeebWindow *beeb_window,
                                             SDL_Renderer *renderer,
                                             const SDL_PixelFormat *pixel_format)
{
    return std::make_unique<TimelineUI>(beeb_window,renderer,pixel_format);
}
