#include <shared/system.h>
#include "CommandKeymapsUI.h"
#include "commands.h"
#include "dear_imgui.h"

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static const char SHORTCUT_KEYCODES_POPUP[]="CommandKeymapsUIShortcutKeycodesPopup";

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

CommandKeymapsUI::CommandKeymapsUI() {
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void CommandKeymapsUI::DoImGui() {
    CommandTable::ForEachCommandTable([&](CommandTable *table) {
        ImGuiIDPusher id_pusher(table);
        std::string title=table->GetName()+" shortcuts";
        if(ImGui::CollapsingHeader(title.c_str(),"header",true,true)) {
            table->ForEachCommand([&](Command *command) {
                ImGuiIDPusher id_pusher(command);

                if(ImGui::Button(command->GetText().c_str())) {
                    ImGui::OpenPopup(SHORTCUT_KEYCODES_POPUP);
                }

                if(ImGui::IsItemHovered()) {
                    ImGui::BeginTooltip();
                    ImGuiKeySymsList(&m_edited,table,(CommandTable *)nullptr,command);
                    m_wants_keyboard_focus=true;
                    ImGui::EndTooltip();
                }

                if(ImGui::BeginPopup(SHORTCUT_KEYCODES_POPUP)) {
                    ImGuiKeySymsList(&m_edited,table,table,command);
                    m_wants_keyboard_focus=true;
                    ImGui::EndPopup();
                }
            });
        }
    });
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool CommandKeymapsUI::WantsKeyboardFocus() const {
    return m_wants_keyboard_focus;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool CommandKeymapsUI::DidConfigChange() const {
    return m_edited;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

