#include <shared/system.h>
#include "SettingsUI.h"

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

SettingsUI::SettingsUI() {
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

SettingsUI::~SettingsUI() {
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

const std::string &SettingsUI::GetName() const {
    return m_name;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void SettingsUI::SetName(std::string name) {
    m_name = std::move(name);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

uint32_t SettingsUI::GetExtraImGuiWindowFlags() const {
    return 0;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool SettingsUI::ActionCommandsForPCKey(uint32_t pc_key) {
    (void)pc_key;

    return false;
}
