#include <shared/system.h>
#include "CommandContextStackUI.h"
#include "BeebWindow.h"
#include "commands.h"

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

CommandContextStackUI::CommandContextStackUI(CommandContextStack *cc_stack):
    m_cc_stack(cc_stack)
{
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void CommandContextStackUI::DoImGui(CommandContextStack *cc_stack) {
    (void)cc_stack;

    ImGui::Text("Focus Window=%s",GImGui->NavWindow?GImGui->NavWindow->Name:"(none)");
    ImGui::Text("IsWindowFocused=%s",BOOL_STR(ImGui::IsWindowFocused()));
    ImGui::Text("IsWindowFocused(RootWindow)=%s",BOOL_STR(ImGui::IsWindowFocused(ImGuiFocusedFlags_RootWindow)));
    ImGui::Text("IsWindowFocused(RootAndChildWindows)=%s",BOOL_STR(ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows)));

    for(size_t i=0;i<m_cc_stack->GetNumCCs();++i) {
        const std::shared_ptr<CommandContext> &cc=m_cc_stack->GetCCByIndex(i);
        const CommandTable *table=cc->GetCommandTable();

        ImGui::Text("%zu. %s",i,table->GetName().c_str());
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool CommandContextStackUI::OnClose() {
    return false;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
