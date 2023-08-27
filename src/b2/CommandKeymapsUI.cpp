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

// Temp measure until I get this stuff properly sorted out.
struct CommandTemp {
    Command *command = nullptr;
    Command2 *command2 = nullptr;
};

struct CommandTableTemp {
    CommandTable *table = nullptr;
    CommandTable2 *table2 = nullptr;
    std::vector<CommandTemp> commands;
};

class CommandKeymapsUI : public SettingsUI {
  public:
    CommandKeymapsUI() {
        CommandTable::ForEachCommandTable([this](CommandTable *table) {
            CommandTableTemp *t = &m_tables_by_name[table->GetName()];
            ASSERT(!t->table);
            t->table = table;
            table->ForEachCommand([this, t](Command *command) {
                t->commands.push_back({command, nullptr});
                this->UpdateTextWidth(command->GetText());
            });
        });

        ForEachCommandTable2([this](CommandTable2 *table) {
            CommandTableTemp *t = &m_tables_by_name[table->GetName()];
            ASSERT(!t->table2);
            t->table2 = table;
            table->ForEachCommand([this, t](Command2 *command) {
                t->commands.push_back({nullptr, command});
                this->UpdateTextWidth(command->GetText());
            });
        });

        for (auto &&name_and_table : m_tables_by_name) {
            std::sort(name_and_table.second.commands.begin(),
                      name_and_table.second.commands.end(),
                      [](const CommandTemp &a, const CommandTemp &b) {
                          ASSERT(a.command || a.command2);
                          ASSERT(b.command || b.command2);

                          const std::string &a_text = a.command ? a.command->GetText() : a.command2->GetText();
                          const std::string &b_text = b.command ? b.command->GetText() : b.command2->GetText();

                          return a_text < b_text;
                      });
        }
    }

    void DoImGui() override {
        m_wants_keyboard_focus = false;

        for (auto &&name_and_table : m_tables_by_name) {
            ImGuiIDPusher id_pusher(name_and_table.first.c_str());
            std::string title = name_and_table.first + " shortcuts";
            if (ImGui::CollapsingHeader(title.c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
                for (const CommandTemp &c : name_and_table.second.commands) {
                    if (c.command) {
                        this->DoCommandKeymapsRowUI(name_and_table.second.table, c.command);
                    } else {
                        if (c.command2->IsVisible()) {
                            this->DoCommandKeymapsRowUI(name_and_table.second.table2, c.command2);
                        }
                    }
                }
            }
        }
    }

    bool OnClose() override {
        return m_edited;
    }

  protected:
  private:
    std::map<std::string, CommandTableTemp> m_tables_by_name;
    bool m_edited = false;
    bool m_wants_keyboard_focus = false;
    float m_max_command_text_width = 0.f;

    void UpdateTextWidth(const std::string &text) {
        ImVec2 size = ImGui::CalcTextSize(text.c_str());

        m_max_command_text_width = std::max(m_max_command_text_width, size.x);
    }

    template <class TableType, class CommandType>
    void DoCommandKeymapsRowUI(TableType *table, CommandType *command) {
        ImGuiIDPusher command_id_pusher(command);

        bool default_shortcuts;
        const std::vector<uint32_t> *pc_keys = table->GetPCKeysForCommand(&default_shortcuts, command);

        float left = ImGui::GetCursorPosX();

        if (ImGui::Button(command->GetText().c_str())) {
            ImGui::OpenPopup(SHORTCUT_KEYCODES_POPUP);
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

            uint32_t keycode = ImGuiConsumePressedKeycode();
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

std::unique_ptr<SettingsUI> CreateCommandKeymapsUI(BeebWindow *beeb_window) {
    (void)beeb_window;
    return std::make_unique<CommandKeymapsUI>();
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
