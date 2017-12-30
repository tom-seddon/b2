#include <shared/system.h>
#include <shared/debug.h>
#include "commands.h"
#include "dear_imgui.h"
#include "keys.h"
#include <algorithm>

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static std::map<std::string,CommandTable *> *g_all_command_tables;

static void InitAllCommandTables() {
    if(!g_all_command_tables) {
        static std::map<std::string,CommandTable *> s_all_command_tables;

        g_all_command_tables=&s_all_command_tables;
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

Command::Command(std::string name,std::string text,bool must_confirm,std::vector<uint32_t> default_shortcuts):
    m_name(std::move(name)),
    m_text(std::move(text)),
    m_must_confirm(must_confirm),
    m_default_shortcuts(std::move(default_shortcuts))
{
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

Command::~Command() {
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

const std::string &Command::GetName() const {
    return m_name;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

const std::string &Command::GetText() const {
    return m_text;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

const std::vector<uint32_t> *Command::GetDefaultShortcuts() const {
    return &m_default_shortcuts;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void CommandTable::ForEachCommandTable(std::function<void(CommandTable *)> fun) {
    InitAllCommandTables();

    for(auto &&it:*g_all_command_tables) {
        fun(it.second);
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

CommandTable *CommandTable::FindCommandTableByName(const std::string &name) {
    InitAllCommandTables();

    auto &&it=g_all_command_tables->find(name);
    if(it==g_all_command_tables->end()) {
        return nullptr;
    } else {
        return it->second;
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

CommandTable::CommandTable(std::string name):
    m_name(std::move(name))
{
    InitAllCommandTables();

    ASSERT(g_all_command_tables->count(this->GetName())==0);
    (*g_all_command_tables)[this->GetName()]=this;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

CommandTable::~CommandTable() {
    ASSERT((*g_all_command_tables)[this->GetName()]==this);
    g_all_command_tables->erase(this->GetName());
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

Command *CommandTable::FindCommandByName(const char *name) const {
    auto &&it=m_command_by_name.find(name);
    if(it!=m_command_by_name.end()) {
        return it->second.get();
    } else {
        return nullptr;
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

Command *CommandTable::FindCommandByName(const std::string &str) const {
    return this->FindCommandByName(str.c_str());
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

const std::string &CommandTable::GetName() const {
    return m_name;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void CommandTable::ForEachCommand(std::function<void(Command *)> fun) const {
    this->UpdateSortedCommands();

    for(size_t i=0;i<m_commands_sorted.size();++i) {
        fun(m_commands_sorted[i]);
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void CommandTable::ResetDefaultMappingsByCommand(Command *command) {
    m_pc_keys_by_command.erase(command);

    m_commands_by_pc_key_dirty=true;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void CommandTable::ClearMappingsByCommand(Command *command) {
    m_pc_keys_by_command[command].clear();

    m_commands_by_pc_key_dirty=true;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void CommandTable::AddMapping(uint32_t pc_key,Command *command) {
    ASSERT(this->FindCommandByName(command->m_name));

    std::vector<uint32_t> *pc_keys=&m_pc_keys_by_command[command];

    if(std::find(pc_keys->begin(),pc_keys->end(),pc_key)==pc_keys->end()) {
        pc_keys->push_back(pc_key);

        if(command->m_shortcut==0) {
            command->m_shortcut=pc_key;
        }

        m_commands_by_pc_key_dirty=true;
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void CommandTable::RemoveMapping(uint32_t pc_key,Command *command) {
    ASSERT(this->FindCommandByName(command->m_name));

    auto &&command_it=m_pc_keys_by_command.find(command);
    if(command_it==m_pc_keys_by_command.end()) {
        return;
    }

    auto &&key_it=std::find(command_it->second.begin(),command_it->second.end(),pc_key);
    if(key_it==command_it->second.end()) {
        return;
    }

    command_it->second.erase(key_it);

    if(command_it->second.empty()) {
        command->m_shortcut=0;

        // and leave the empty mapping in place.
    } else {
        command->m_shortcut=command_it->second[0];
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

const std::vector<uint32_t> *CommandTable::GetPCKeysForCommand(bool *are_defaults,Command *command) const {
    auto &&it=m_pc_keys_by_command.find(command);
    if(it==m_pc_keys_by_command.end()) {
        if(are_defaults) {
            *are_defaults=true;
        }
        return command->GetDefaultShortcuts();
    } else {
        if(are_defaults) {
            *are_defaults=false;
        }
        return &it->second;
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool CommandTable::ExecuteCommandsForPCKey(uint32_t key,void *object) const {
    if(!object) {
        return false;
    }

    if(m_commands_by_pc_key_dirty) {
        m_commands_by_pc_key.clear();

        this->ForEachCommand([this](Command *command) {
            const std::vector<uint32_t> *pc_keys=this->GetPCKeysForCommand(nullptr,command);
            for(uint32_t pc_key:*pc_keys) {
                m_commands_by_pc_key[pc_key].push_back(command);
            }
        });

        m_commands_by_pc_key_dirty=false;
    }

    auto &&it=m_commands_by_pc_key.find(key);
    if(it==m_commands_by_pc_key.end()) {
        return false;
    }

    for(Command *command:it->second) {
        command->Execute(object);
    }

    return true;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

Command *CommandTable::AddCommand(std::unique_ptr<Command> command) {
    ASSERT(!this->FindCommandByName(command->m_name));

    Command *result=command.get();

    m_command_by_name[command->m_name.c_str()]=std::move(command);
    m_commands_sorted_dirty=true;

    return result;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void CommandTable::UpdateSortedCommands() const {
    if(m_commands_sorted_dirty) {
        m_commands_sorted.clear();
        m_commands_sorted.reserve(m_command_by_name.size());
        for(auto &&it:m_command_by_name) {
            m_commands_sorted.push_back(it.second.get());
        }

        std::sort(m_commands_sorted.begin(),m_commands_sorted.end(),[](auto a,auto b) {
            return a->GetText()<b->GetText();
        });

        m_commands_sorted_dirty=false;
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

CommandDef::CommandDef(std::string name_,std::string text_):
    name(std::move(name_)),
    text(std::move(text_))
{
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

CommandDef &CommandDef::Shortcut(uint32_t keycode) {
    this->shortcuts.push_back(keycode);

    return *this;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

CommandDef &CommandDef::MustConfirm() {
    this->must_confirm=true;

    return *this;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

CommandContext::CommandContext(void *object,const CommandTable *table):
    m_object(object),
    m_table(table)
{
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void CommandContext::Reset() {
    m_object=nullptr;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void CommandContext::DoButton(const char *name) {
    Command *command=m_table->FindCommandByName(name);
    if(!command) {
        return;
    }

    if(!m_object) {
        return;
    }

    if(command->m_must_confirm) {
        // bleargh...
    } else if(const bool *ticked=command->IsTicked(m_object)) {
        if(ImGui::RadioButton(command->m_text.c_str(),*ticked)) {
            command->Execute(m_object);
        }
    } else {
        bool enabled=command->IsEnabled(m_object);
        if(ImGuiButton(command->m_text.c_str(),enabled)) {
            command->Execute(m_object);
        }
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void CommandContext::DoMenuItemUI(const char *name) {
    Command *command=m_table->FindCommandByName(name);
    if(!command) {
        return;
    }

    if(!m_object) {
        return;
    }

    std::string shortcut;
    if(command->m_shortcut!=0) {
        shortcut=GetKeycodeName(command->m_shortcut).c_str();
    }

    bool enabled=command->IsEnabled(m_object);

    if(command->m_must_confirm) {
        if(ImGui::BeginMenu(command->m_text.c_str(),enabled)) {
            if(ImGui::MenuItem("Confirm")) {
                command->Execute(m_object);
            }
            ImGui::EndMenu();
        }
    } else {
        const bool *ticked=command->IsTicked(m_object);

        bool selected=false;
        if(ticked&&*ticked) {
            selected=true;
        }

        if(ImGui::MenuItem(command->m_text.c_str(),shortcut.empty()?nullptr:shortcut.c_str(),&selected,enabled)) {
            command->Execute(m_object);
        }
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void CommandContext::DoToggleCheckboxUI(const char *name) {
    Command *command=m_table->FindCommandByName(name);
    if(!command) {
        return;
    }

    if(!m_object) {
        return;
    }

    bool value=*command->IsTicked(m_object);

    if(ImGui::Checkbox(command->GetText().c_str(),&value)) {
        command->Execute(m_object);
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool CommandContext::ExecuteCommandsForPCKey(uint32_t keycode) const {
    return m_table->ExecuteCommandsForPCKey(keycode,m_object);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

const CommandTable *CommandContext::GetCommandTable() const {
    return m_table;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void CommandContextStack::Push(const std::shared_ptr<CommandContext> &cc,bool force) {
    if(force||ImGui::IsWindowFocused()) {
        m_ccs.push_back(cc);
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void CommandContextStack::Reset() {
    m_ccs.clear();
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

size_t CommandContextStack::GetNumCCs() const {
    return m_ccs.size();
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

const std::shared_ptr<CommandContext> &CommandContextStack::GetCCByIndex(size_t index) const {
    ASSERT(index<m_ccs.size());
    return m_ccs[m_ccs.size()-1-index];
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool CommandContextStack::ExecuteCommandsForPCKey(uint32_t keycode) const {
    for(size_t i=0;i<this->GetNumCCs();++i) {
        const std::shared_ptr<CommandContext> &cc=this->GetCCByIndex(i);

        if(cc->ExecuteCommandsForPCKey(keycode)) {
            return true;
        }
    }

    return false;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
