#include <shared/system.h>
#include <shared/debug.h>
#include "BeebLinkUI.h"
#include "SettingsUI.h"
#include <vector>
#include <string>
#include "BeebLinkHTTPHandler.h"
#include "dear_imgui.h"
#include <IconsFontAwesome5.h>

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

class BeebLinkUI:
    public SettingsUI
{
public:
    BeebLinkUI() {
    }

    void DoImGui(CommandContextStack *cc_stack) override {
        (void)cc_stack;

        std::vector<std::string> urls=BeebLinkHTTPHandler::GetServerURLs();

        ASSERT(urls.size()<=INT_MAX);
        ASSERT(urls.size()<=PTRDIFF_MAX);

        ImGui::Text("Default URL: %s",BeebLinkHTTPHandler::DEFAULT_URL.c_str());

        bool changed=false;

        size_t i=0;
        while(i<urls.size()) {
            ImGuiIDPusher id_pusher((int)i);

            bool remove=ImGui::Button("-");

            bool up,down;
            {
                ImGuiStyleColourPusher style_pusher;

                bool up_disabled=i==0;
                style_pusher.PushDisabledButtonColours(up_disabled);
                ImGui::SameLine();
                up=ImGui::Button(ICON_FA_ARROW_UP)&&!up_disabled;

                bool down_disabled=i==urls.size()-1;
                style_pusher.PushDisabledButtonColours(down_disabled);
                ImGui::SameLine();
                down=ImGui::Button(ICON_FA_ARROW_DOWN)&&!down_disabled;
            }

            ImGui::SameLine();
            ImGui::TextUnformatted(urls[i].c_str());

            if(remove) {
                urls.erase(urls.begin()+(ptrdiff_t)i);
                changed=true;
            } else {
                if(up) {
                    ASSERT(i>0);
                    std::swap(urls[i-1],urls[i]);
                    changed=true;
                } else if(down) {
                    ASSERT(i<urls.size()-1);
                    std::swap(urls[i],urls[i+1]);
                    changed=true;
                }

                ++i;
            }
        }

        bool add_new_url=false;
        if(ImGui::InputText("",m_new_url,sizeof m_new_url,ImGuiInputTextFlags_AutoSelectAll|ImGuiInputTextFlags_EnterReturnsTrue)) {
            add_new_url=true;
        }

        ImGui::SameLine();
        {
            ImGuiStyleColourPusher style_pusher;

            if(m_new_url[strspn(m_new_url," \t")]==0) {
                style_pusher.PushDisabledButtonColours();
            }

            if(ImGui::Button("Add URL")) {
                add_new_url=true;
            }
        }

        if(add_new_url) {
            changed=true;
            urls.push_back(m_new_url);
            strcpy(m_new_url,"");
        }

        if(changed) {
            BeebLinkHTTPHandler::SetServerURLs(std::move(urls));
            m_changed=true;
        }
    }

    bool OnClose() override {
        return m_changed;
    }
protected:
private:
    bool m_changed=false;
    char m_new_url[1000]={};
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

std::unique_ptr<SettingsUI> CreateBeebLinkUI(BeebWindow *beeb_window) {
    (void)beeb_window;

    return std::make_unique<BeebLinkUI>();
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
