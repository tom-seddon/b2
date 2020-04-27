#include <shared/system.h>
#include "CommandKeymapsUI.h"
#include "commands.h"
#include "dear_imgui.h"
#include "SettingsUI.h"
#include <algorithm>

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

    void DoImGui() override {
        m_wants_keyboard_focus=false;

        float extra=(GImGui->Style.FramePadding.x+GImGui->Style.FrameRounding)*2.f;

        CommandTable::ForEachCommandTable([&](CommandTable *table) {
            ImGuiIDPusher id_pusher(table);
            std::string title=table->GetName()+" shortcuts";
            if(ImGui::CollapsingHeader(title.c_str(),ImGuiTreeNodeFlags_DefaultOpen)) {
                table->ForEachCommand([&](Command *command) {
                    ImGuiIDPusher command_id_pusher(command);

                    bool default_shortcuts;
                    const std::vector<uint32_t> *pc_keys=table->GetPCKeysForCommand(&default_shortcuts,command);

                    float left=ImGui::GetCursorPosX();

                    if(ImGui::Button(command->GetText().c_str())) {
                        ImGui::OpenPopup(SHORTCUT_KEYCODES_POPUP);
                    }

                    if (ImGui::BeginPopupContextItem("command context item"))
                    {
                        if(ImGui::Selectable("Clear shortcuts")) {
                            table->ClearMappingsByCommand(command);
                        }

                        if(!default_shortcuts) {
                            if(ImGui::Selectable("Reset to default")) {
                                table->ResetDefaultMappingsByCommand(command);
                            }
                        }

                        ImGui::EndPopup();
                    }

                    if(ImGui::BeginPopup(SHORTCUT_KEYCODES_POPUP)) {
                        ImGui::TextUnformatted("(press key to add)");

                        uint32_t keycode=ImGuiConsumePressedKeycode();
                        if(keycode!=0) {
                            table->AddMapping(keycode,command);
                            m_edited=true;
                            ImGui::CloseCurrentPopup();
                        }

                        m_wants_keyboard_focus=true;
                        ImGui::EndPopup();
                    }

                    if(pc_keys) {
                        ImGuiStyleColourPusher pusher;

                        for(size_t i=0;i<pc_keys->size();++i) {
                            uint32_t pc_key=(*pc_keys)[i];
                            ImGuiIDPusher pc_key_id_pusher((int)pc_key);

                            if(i>0) {
                                ImGui::NewLine();
                            }

                            ImGui::SameLine(left+m_max_command_text_width+extra+20.f);

                            if(!default_shortcuts) {
                                if(ImGui::Button("x")) {
                                    table->RemoveMapping(pc_key,command);
                                    break;
                                }

                                ImGui::SameLine();
                            }


                            ImGui::Text("%s",GetKeycodeName(pc_key).c_str());
                        }
                    }
                });
            }
        });
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
