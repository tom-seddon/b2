#include <shared/system.h>
#include <shared/path.h>
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
#include "VideoWriter.h"

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static const std::string RECENT_PATHS_VIDEO("video");

static const char VIDEO_FORMATS_POPUP[] = "video_formats_popup";

////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////

class TimelineUI : public SettingsUI {
  public:
    explicit TimelineUI(BeebWindow *beeb_window,
                        SDL_Renderer *renderer)
        : m_beeb_window(beeb_window)
        , m_thumbnails(renderer) {
        this->SetDefaultSize(ImVec2(350, 400));
    }

    void DoImGui() override {
        auto beeb_thread = m_beeb_window->GetBeebThread();

        BeebThreadTimelineState timeline_state;
        beeb_thread->GetTimelineState(&timeline_state);

        ASSERT(timeline_state.end_cycles.n >= timeline_state.begin_cycles.n);
        const CycleCount timeline_duration = {timeline_state.end_cycles.n - timeline_state.begin_cycles.n};

        ImGui::Text("Timeline Mode: %s", GetBeebThreadTimelineModeEnumName(timeline_state.mode));

        ImGui::Checkbox("Follow new events", &m_follow);

        // Back, Stop, Play/Pause, Forward
        //
        // https://fontawesome.com/icons?d=gallery

        switch (timeline_state.mode) {
        default:
            ASSERT(false);
            // fall through
        case BeebThreadTimelineMode_None:
            {
                ASSERT(timeline_state.end_cycles.n >= timeline_state.begin_cycles.n);
                CycleCount duration = {timeline_state.end_cycles.n - timeline_state.begin_cycles.n};

                // Record
                if (timeline_state.can_record) {
                    if (ImGuiConfirmButton("Record", timeline_duration.n > 0)) {
                        beeb_thread->Send(std::make_shared<BeebThread::StartRecordingMessage>());
                    }
                }

                if (duration.n > 0) {
                    // Clear
                    ImGui::SameLine();
                    if (ImGuiConfirmButton("Delete")) {
                        beeb_thread->Send(std::make_shared<BeebThread::ClearRecordingMessage>());
                    }
                }

                if (!timeline_state.can_record) {
                    ImGui::Text("Recording disabled due to: %s", GetCloneImpedimentsDescription(timeline_state.clone_impediments).c_str());
                }
            }
            break;

        case BeebThreadTimelineMode_Replay:
            {
                // Stop
                if (ImGui::Button("Stop")) {
                }

                // Pause
                ImGui::SameLine();
                ImGui::Button("Pause");
            }
            break;

        case BeebThreadTimelineMode_Record:
            {
                // Stop
                if (ImGui::Button("Stop")) {
                    beeb_thread->Send(std::make_shared<BeebThread::StopRecordingMessage>());
                }
            }
            break;
        }

        switch (timeline_state.mode) {
        default:
            ASSERT(false);
            // fall through
        case BeebThreadTimelineMode_None:
            // fall through
        case BeebThreadTimelineMode_Record:
            {
                if (timeline_duration.n == 0) {
                    ImGui::Text("No recording.");
                } else {
                    ImGui::Text("Recorded: %zu events, %s",
                                timeline_state.num_events,
                                GetCycleCountString(timeline_duration).c_str());
                }
            }
            break;

        case BeebThreadTimelineMode_Replay:
            break;
        }

        const ImVec2 THUMBNAIL_SIZE = m_thumbnails.GetThumbnailSize();
        const float GAP_HEIGHT = 20;

        // The +3 is to accommodate the separator. Should probably calculate
        // the size perfectly, but I'm not sure how...
        const float CELL_HEIGHT = THUMBNAIL_SIZE.y + 3 * ImGui::GetTextLineHeight() + GAP_HEIGHT + 3;

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
                          ImVec2(0, 0),
                          true, //border
                          ImGuiWindowFlags_AlwaysVerticalScrollbar);

        ImVec2 timeline_size(0, timeline_state.num_beeb_state_events * CELL_HEIGHT);

        if (m_follow) {
            if (timeline_state.mode == BeebThreadTimelineMode_Record) {
                size_t num_beeb_state_events = beeb_thread->GetNumTimelineBeebStateEvents();
                if (m_old_num_beeb_state_events != num_beeb_state_events) {
                    ImGui::SetScrollY(timeline_size.y);

                    m_old_num_beeb_state_events = num_beeb_state_events;
                }
            }
        }

        {
            ImGui::BeginChild("timeline", timeline_size, false, 0); //false = no border

            // not too concerned about overflow here...
            ImGuiListClipper clipper;
            clipper.Begin((int)timeline_state.num_beeb_state_events, //not too concerned about overflow here!
                          CELL_HEIGHT);
            //            int display_row_begin,display_row_end;
            //            ImGui::CalcListClipping(
            //                                    CELL_HEIGHT,
            //                                    &display_row_begin,
            //                                    &display_row_end);

            while (clipper.Step()) {
                std::vector<BeebThread::TimelineBeebStateEvent> beeb_state_events =
                    beeb_thread->GetTimelineBeebStateEvents((size_t)clipper.DisplayStart,
                                                            (size_t)clipper.DisplayEnd);

                for (size_t i = 0; i < beeb_state_events.size(); ++i) {
                    const BeebThread::TimelineBeebStateEvent *e = &beeb_state_events[i];
                    ImGuiIDPusher pusher(e->message.get());
                    const std::shared_ptr<const BeebState> state = e->message->GetBeebState();
                    int row = clipper.DisplayStart + (int)i;

                    ImGui::SetCursorPos(ImVec2(0.f, row * CELL_HEIGHT));
                    //                ImGui::Text("Row %d",row);

                    if (i > 0) {
                        ImGui::Separator();
                    }

                    if (ImGuiConfirmButton("Load")) {
                        beeb_thread->Send(std::make_shared<BeebThread::LoadTimelineStateMessage>(state,
                                                                                                 true));
                    }

                    ImGui::SameLine();

                    if (ImGuiConfirmButton("Delete")) {
                        beeb_thread->Send(std::make_shared<BeebThread::DeleteTimelineStateMessage>(state));
                    }

                    ImGui::SameLine();

                    if (ImGui::Button("Replay")) {
                        beeb_thread->Send(std::make_shared<BeebThread::StartReplayMessage>(state));
                    }

                    if (CanCreateVideoWriter()) {
                        ImGui::SameLine();

                        if (ImGui::Button("Video")) {
                            ImGui::OpenPopup(VIDEO_FORMATS_POPUP);
                        }

                        if (ImGui::BeginPopup(VIDEO_FORMATS_POPUP)) {
                            for (size_t format_index = 0; format_index < GetNumVideoWriterFormats(); ++format_index) {
                                const VideoWriterFormat *format = GetVideoWriterFormatByIndex(format_index);
                                if (ImGui::Button(format->description.c_str())) {
                                    ImGui::CloseCurrentPopup();

                                    SaveFileDialog fd(RECENT_PATHS_VIDEO);
                                    fd.AddFilter(format->description, {format->extension});

                                    std::string path;
                                    if (fd.Open(&path)) {
                                        fd.AddLastPathToRecentPaths();

                                        if (PathGetExtension(path).empty()) {
                                            path += format->extension;
                                        }

                                        std::unique_ptr<VideoWriter> video_writer = CreateVideoWriter(m_beeb_window->GetMessageList(),
                                                                                                      std::move(path),
                                                                                                      format_index);
                                        auto message = std::make_shared<BeebThread::CreateTimelineVideoMessage>(state,
                                                                                                                std::move(video_writer));
                                        m_beeb_window->GetBeebThread()->Send(std::move(message));
                                    }
                                }
                            }

                            ImGui::EndPopup();
                        }
                    }

                    char cycles_str[MAX_UINT64_THOUSANDS_SIZE];
                    GetThousandsString(cycles_str, e->time_cycles.n);
                    ImGui::Text("%s (%s)", cycles_str, GetCycleCountString(e->time_cycles).c_str());

                    if (state->name.empty()) {
                        ImGui::TextUnformatted("(no name)");
                    } else {
                        ImGui::Text("Name: %s", state->name.c_str());
                    }

                    m_thumbnails.Thumbnail(e->message->GetBeebState());
                }
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
    BeebWindow *m_beeb_window = nullptr;
    ThumbnailsUI m_thumbnails;
    bool m_follow = true;
    size_t m_old_num_beeb_state_events = 0;
};

////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////

std::unique_ptr<SettingsUI> CreateTimelineUI(BeebWindow *beeb_window, SDL_Renderer *renderer) {
    return std::make_unique<TimelineUI>(beeb_window, renderer);
}
