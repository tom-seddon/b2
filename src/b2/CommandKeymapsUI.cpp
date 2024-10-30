#include <shared/system.h>
#include "CommandKeymapsUI.h"
#include "commands.h"
#include "dear_imgui.h"
#include "SettingsUI.h"
#include <algorithm>

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static const char SHORTCUT_KEYCODES_POPUP[] = "CommandKeymapsUIShortcutKeycodesPopup";

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

class CommandKeymapsUI : public SettingsUI {
  public:
    CommandKeymapsUI(ImGuiStuff *imgui_stuff)
        : m_imgui_stuff(imgui_stuff) {
        this->SetDefaultSize(ImVec2(300, 300));

        ForEachCommandTable2([this](CommandTable2 *table) {
            table->ForEachCommand([this](Command2 *command) {
                std::string text = command->GetText();
                const std::string &extra_text = command->GetExtraText();
                if (!extra_text.empty()) {
                    text += " (" + extra_text + ")";
                }
                ImVec2 size = ImGui::CalcTextSize(text.c_str());
                m_max_command_text_width = std::max(m_max_command_text_width, size.x);
            });
        });
    }

    void DoImGui() override {
        m_wants_keyboard_focus = false;

        ForEachCommandTable2([this](CommandTable2 *table) {
            ImGuiIDPusher id_pusher(table->GetName().c_str());
            bool header_shown = false;
            bool table_visible = true;

            table->ForEachCommand([this, table, &table_visible, &header_shown](Command2 *command) {
                if (command->IsVisible()) {
                    if (!header_shown) {
                        std::string title = table->GetName() + " shortcuts";
                        table_visible = ImGui::CollapsingHeader(title.c_str(), ImGuiTreeNodeFlags_DefaultOpen);
                        header_shown = true;
                    }

                    if (table_visible) {
                        this->DoCommandKeymapsRowUI(table, command);
                    }
                }
            });
        });
    }

    bool OnClose() override {
        return m_edited;
    }

  protected:
  private:
    ImGuiStuff *m_imgui_stuff = nullptr;
    bool m_edited = false;
    bool m_wants_keyboard_focus = false;
    float m_max_command_text_width = 0.f;

    void DoCommandKeymapsRowUI(CommandTable2 *table, Command2 *command) {
        ImGuiIDPusher command_id_pusher(command);

        bool default_shortcuts;
        const std::vector<uint32_t> *pc_keys = table->GetPCKeysForCommand(&default_shortcuts, command);

        float left = ImGui::GetCursorPosX();

        if (ImGui::Button(command->GetText().c_str())) {
            ImGui::OpenPopup(SHORTCUT_KEYCODES_POPUP);
        }

        const std::string &extra_text = command->GetExtraText();
        if (!extra_text.empty()) {
            ImGui::SameLine();
            ImGui::Text(" (%s)", extra_text.c_str());
        }

        if (ImGui::BeginPopupContextItem("command context item")) {
            if (ImGui::Selectable("Clear shortcuts")) {
                table->ClearMappingsByCommand(command);
            }

            if (!default_shortcuts) {
                if (ImGui::Selectable("Reset to default")) {
                    table->ResetDefaultMappingsByCommand(command);
                }
            }

            ImGui::EndPopup();
        }

        if (ImGui::BeginPopup(SHORTCUT_KEYCODES_POPUP)) {
            ImGui::TextUnformatted("(press key to add)");

            uint32_t keycode = m_imgui_stuff->ConsumePressedKeycode();
            if (keycode != 0) {
                table->AddMapping(keycode, command);
                m_edited = true;
                ImGui::CloseCurrentPopup();
            }

            m_wants_keyboard_focus = true;
            ImGui::EndPopup();
        }

        if (pc_keys) {
            ImGuiStyleColourPusher pusher;

            for (size_t i = 0; i < pc_keys->size(); ++i) {
                uint32_t pc_key = (*pc_keys)[i];
                ImGuiIDPusher pc_key_id_pusher((int)pc_key);

                if (i > 0) {
                    ImGui::NewLine();
                }

                float extra = (GImGui->Style.FramePadding.x + GImGui->Style.FrameRounding) * 2.f;
                ImGui::SameLine(left + m_max_command_text_width + extra + 20.f);

                if (!default_shortcuts) {
                    if (ImGui::Button("x")) {
                        table->RemoveMapping(pc_key, command);
                        break;
                    }

                    ImGui::SameLine();
                }

                ImGui::Text("%s", GetKeycodeName(pc_key).c_str());
            }
        }
    }
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

std::unique_ptr<SettingsUI> CreateCommandKeymapsUI(BeebWindow *beeb_window, ImGuiStuff *imgui_stuff) {
    (void)beeb_window;
    return std::make_unique<CommandKeymapsUI>(imgui_stuff);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
