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

void CommandContextStackUI::DoImGui() {
    ImGui::Text("IsRootWindowFocused=%s",BOOL_STR(ImGui::IsRootWindowFocused()));
    ImGui::Text("IsWindowFocused=%s",BOOL_STR(ImGui::IsWindowFocused()));
    ImGui::Text("IsRootWindowOrAnyChildFocused=%s",BOOL_STR(ImGui::IsRootWindowOrAnyChildFocused()));

    for(size_t i=0;i<m_cc_stack->GetNumCCs();++i) {
        const std::shared_ptr<CommandContext> &cc=m_cc_stack->GetCCByIndex(i);
        const CommandTable *table=cc->GetCommandTable();

        ImGui::Text("%zu. %s",i,table->GetName().c_str());
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
