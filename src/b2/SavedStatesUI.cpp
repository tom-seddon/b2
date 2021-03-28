#include <shared/system.h>
#include "SavedStatesUI.h"
#include "dear_imgui.h"
#include "SettingsUI.h"
#include "BeebWindows.h"
#include "ThumbnailsUI.h"
#include "BeebState.h"
#include <beeb/DiscImage.h>
#include "BeebWindow.h"
#include "BeebThread.h"

////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////

class SavedStatesUI:
public SettingsUI
{
public:
    explicit SavedStatesUI(BeebWindow *beeb_window,
                           SDL_Renderer *renderer,
                           const SDL_PixelFormat *pixel_format):
    m_beeb_window(beeb_window),
    m_thumbnails(renderer,pixel_format)
    {
    }

    void DoImGui() override {
        const std::shared_ptr<BeebThread> &beeb_thread=m_beeb_window->GetBeebThread();

        BeebThreadTimelineState timeline_state;
        beeb_thread->GetTimelineState(&timeline_state);

        // not too concerned about overflow here.
        int num_states=(int)BeebWindows::GetNumSavedStates();

        bool can_load=timeline_state.mode==BeebThreadTimelineMode_None;

        // Buttons
        // Name
        // Drive 0:
        // Drive 1:
        // <<Image>>

        float row_height=(2+NUM_DRIVES)*ImGui::GetTextLineHeight()+m_thumbnails.GetThumbnailSize().y+25;

        ImGui::Text("%d saved states",num_states);

        ImGui::BeginChild("saved_states_container",ImVec2(0,0),true,ImGuiWindowFlags_AlwaysVerticalScrollbar);
        {
            ImGui::BeginChild("saved_states",ImVec2(0,num_states*row_height),false,0);
            {
                if(num_states>0) {
                    int row_begin,row_end;
                    ImGui::CalcListClipping(num_states,row_height,&row_begin,&row_end);
                    ASSERT(row_begin>=0);
                    ASSERT(row_end>=row_begin);

                    std::vector<std::shared_ptr<const BeebState>> saved_states=BeebWindows::GetSavedStates((size_t)row_begin,
                                                                                                           (size_t)row_end);
                    for(size_t i=0;i<saved_states.size();++i) {
                        const std::shared_ptr<const BeebState> &s=saved_states[i];
                        ImGuiIDPusher id_pusher(s.get());

                        int row=row_begin+(int)i;
                        ImGui::SetCursorPos(ImVec2(0.f,row*row_height));

                        // Buttons
                        if(ImGuiConfirmButton("Delete")) {
                            BeebWindows::DeleteSavedState(s);
                        }

                        ImGui::SameLine();

                        if(can_load) {
                            if(ImGuiConfirmButton("Load")) {
                                beeb_thread->Send(std::make_shared<BeebThread::LoadStateMessage>(s,true));
                            }
                        } else {
                            ImGuiStyleColourPusher colour_pusher;
                            colour_pusher.PushDisabledButtonColours();
                            ImGui::Button("Load");
                        }

                        // Name
                        const std::string &name=s->GetName();
                        if(name.empty()) {
                            ImGui::TextUnformatted("(no name)");
                        } else {
                            ImGui::Text("Name: %s",
                                        name.c_str());
                        }

                        // Drives
                        for(int drive=0;drive<NUM_DRIVES;++drive) {
                            const std::shared_ptr<const DiscImage> &di=s->GetDiscImageByDrive(drive);
                            if(!di) {
                                ImGui::Text("Drive %d: *empty*",
                                            drive);
                            } else {
                                ImGui::Text("Drive %d: %s (%s)",
                                            drive,
                                            di->GetName().c_str(),
                                            di->GetLoadMethod().c_str());
                            }
                        }

                        // Thumbnail
                        m_thumbnails.Thumbnail(s);
                    }
                }
            }
            ImGui::EndChild();
        }
        ImGui::EndChild();
    }

    bool OnClose() override {
        return false;
    }
protected:
private:
    BeebWindow *m_beeb_window=nullptr;
    ThumbnailsUI m_thumbnails;
};

////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////

std::unique_ptr<SettingsUI> CreateSavedStatesUI(BeebWindow *beeb_window,
                                                SDL_Renderer *renderer,
                                                const SDL_PixelFormat *pixel_format)
{
    return std::make_unique<SavedStatesUI>(beeb_window,renderer,pixel_format);
}
