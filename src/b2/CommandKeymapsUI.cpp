#include <shared/system.h>
#include "CommandKeymapsUI.h"
#include "commands.h"
#include "dear_imgui.h"
#include "SettingsUI.h"

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static const char SHORTCUT_KEYCODES_POPUP[]="CommandKeymapsUIShortcutKeycodesPopup";

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

class CommandKeymapsUI:
    public SettingsUI
{
public:
    CommandKeymapsUI() {
        CommandTable::ForEachCommandTable([&](CommandTable *table) {
            table->ForEachCommand([&](Command *command) {
                ImVec2 size=ImGui::CalcTextSize(command->GetText().c_str());

                m_max_command_text_width=std::max(m_max_command_text_width,size.x);
            });
        });
    }

    void DoImGui(CommandContextStack *cc_stack) override {
        (void)cc_stack;

        float extra=(GImGui->Style.FramePadding.x+GImGui->Style.FrameRounding)*2.f;

        CommandTable::ForEachCommandTable([&](CommandTable *table) {
            ImGuiIDPusher id_pusher(table);
            std::string title=table->GetName()+" shortcuts";
            if(ImGui::CollapsingHeader(title.c_str(),ImGuiTreeNodeFlags_DefaultOpen)) {
                table->ForEachCommand([&](Command *command) {
                    ImGuiIDPusher command_id_pusher(command);

                    float left=ImGui::GetCursorPosX();

                    if(ImGui::Button(command->GetText().c_str())) {
                        ImGui::OpenPopup(SHORTCUT_KEYCODES_POPUP);
                    }

                    if(ImGui::BeginPopup(SHORTCUT_KEYCODES_POPUP)) {
                        ImGui::TextUnformatted("(press key to add)");

                        uint32_t keycode=ImGuiGetPressedKeycode();
                        if(keycode!=0) {
                            table->SetMapping(keycode,command,true);
                            m_edited=true;
                            ImGui::CloseCurrentPopup();
                        }

                        m_wants_keyboard_focus=true;
                        ImGui::EndPopup();
                    }

                    if(const uint32_t *pc_keys=table->GetPCKeysForValue(command)) {
                        for(const uint32_t *pc_key=pc_keys;*pc_key!=0;++pc_key) {
                            ImGuiIDPusher pc_key_id_pusher(pc_key);

                            if(pc_key!=pc_keys) {
                                ImGui::NewLine();
                            }

                            ImGui::SameLine(left+m_max_command_text_width+extra+20.f);

                            if(ImGui::Button("x")) {
                                table->SetMapping(*pc_key,command,false);
                            }

                            ImGui::SameLine();

                            ImGui::Text("%s",GetKeycodeName(*pc_key).c_str());
                        }
                    }
                });
            }
        });
    }

    bool WantsKeyboardFocus() const override {
        return m_wants_keyboard_focus;
    }

    bool OnClose() override {
        return m_edited;
    }
protected:
private:
    bool m_edited=false;
    bool m_wants_keyboard_focus=false;
    float m_max_command_text_width=0.f;
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

std::unique_ptr<SettingsUI> CreateCommandKeymapsUI(BeebWindow *beeb_window) {
    (void)beeb_window;
    return std::make_unique<CommandKeymapsUI>();
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
