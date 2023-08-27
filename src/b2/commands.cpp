#include <shared/system.h>
#include <shared/debug.h>
#include "commands.h"
#include "dear_imgui.h"
#include "keys.h"
#include <algorithm>
#include <set>

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static bool g_linked = false;

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static std::map<std::string, CommandTable *> *g_all_command_tables;

static void InitAllCommandTables() {
    if (!g_all_command_tables) {
        static std::map<std::string, CommandTable *> s_all_command_tables;

        g_all_command_tables = &s_all_command_tables;
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

Command::Command(std::string name, std::string text, bool must_confirm, std::vector<uint32_t> default_shortcuts)
    : m_name(std::move(name))
    , m_text(std::move(text))
    , m_must_confirm(must_confirm)
    , m_default_shortcuts(std::move(default_shortcuts)) {
    if (!m_default_shortcuts.empty()) {
        m_shortcut = m_default_shortcuts[0];
    }
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

    for (auto &&it : *g_all_command_tables) {
        fun(it.second);
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

CommandTable *CommandTable::FindCommandTableByName(const std::string &name) {
    InitAllCommandTables();

    auto &&it = g_all_command_tables->find(name);
    if (it == g_all_command_tables->end()) {
        return nullptr;
    } else {
        return it->second;
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

CommandTable::CommandTable(std::string name)
    : m_name(std::move(name)) {
    InitAllCommandTables();

    ASSERT(g_all_command_tables->count(this->GetName()) == 0);
    (*g_all_command_tables)[this->GetName()] = this;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

CommandTable::~CommandTable() {
    ASSERT((*g_all_command_tables)[this->GetName()] == this);
    g_all_command_tables->erase(this->GetName());
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

Command *CommandTable::FindCommandByName(const char *name) const {
    auto &&it = m_command_by_name.find(name);
    if (it != m_command_by_name.end()) {
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

    for (size_t i = 0; i < m_commands_sorted.size(); ++i) {
        fun(m_commands_sorted[i]);
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void CommandTable::ResetDefaultMappingsByCommand(Command *command) {
    m_pc_keys_by_command.erase(command);

    m_commands_by_pc_key_dirty = true;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void CommandTable::ClearMappingsByCommand(Command *command) {
    m_pc_keys_by_command[command].clear();

    m_commands_by_pc_key_dirty = true;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void CommandTable::AddMapping(uint32_t pc_key, Command *command) {
    ASSERT(this->FindCommandByName(command->m_name));

    std::vector<uint32_t> *pc_keys = &m_pc_keys_by_command[command];

    if (std::find(pc_keys->begin(), pc_keys->end(), pc_key) == pc_keys->end()) {
        pc_keys->push_back(pc_key);

        if (command->m_shortcut == 0) {
            command->m_shortcut = pc_key;
        }

        m_commands_by_pc_key_dirty = true;
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void CommandTable::RemoveMapping(uint32_t pc_key, Command *command) {
    ASSERT(this->FindCommandByName(command->m_name));

    auto &&command_it = m_pc_keys_by_command.find(command);
    if (command_it == m_pc_keys_by_command.end()) {
        return;
    }

    auto &&key_it = std::find(command_it->second.begin(), command_it->second.end(), pc_key);
    if (key_it == command_it->second.end()) {
        return;
    }

    command_it->second.erase(key_it);

    if (command_it->second.empty()) {
        command->m_shortcut = 0;

        // and leave the empty mapping in place.
    } else {
        command->m_shortcut = command_it->second[0];
    }

    m_commands_by_pc_key_dirty = true;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

const std::vector<uint32_t> *CommandTable::GetPCKeysForCommand(bool *are_defaults, Command *command) const {
    auto &&it = m_pc_keys_by_command.find(command);
    if (it == m_pc_keys_by_command.end()) {
        if (are_defaults) {
            *are_defaults = true;
        }
        return command->GetDefaultShortcuts();
    } else {
        if (are_defaults) {
            *are_defaults = false;
        }
        return &it->second;
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool CommandTable::ExecuteCommandsForPCKey(uint32_t key, void *object) const {
    if (!object) {
        return false;
    }

    if (m_commands_by_pc_key_dirty) {
        m_commands_by_pc_key.clear();

        this->ForEachCommand([this](Command *command) {
            const std::vector<uint32_t> *pc_keys = this->GetPCKeysForCommand(nullptr, command);
            for (uint32_t pc_key : *pc_keys) {
                m_commands_by_pc_key[pc_key].push_back(command);
            }
        });

        m_commands_by_pc_key_dirty = false;
    }

    auto &&it = m_commands_by_pc_key.find(key);
    if (it == m_commands_by_pc_key.end()) {
        return false;
    }

    for (Command *command : it->second) {
        command->Execute(object);
    }

    return true;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

Command *CommandTable::AddCommand(std::unique_ptr<Command> command) {
    ASSERT(!this->FindCommandByName(command->m_name));

    Command *result = command.get();

    m_command_by_name[command->m_name.c_str()] = std::move(command);
    m_commands_sorted_dirty = true;

    return result;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void CommandTable::UpdateSortedCommands() const {
    if (m_commands_sorted_dirty) {
        m_commands_sorted.clear();
        m_commands_sorted.reserve(m_command_by_name.size());
        for (auto &&it : m_command_by_name) {
            m_commands_sorted.push_back(it.second.get());
        }

        std::sort(m_commands_sorted.begin(), m_commands_sorted.end(), [](auto a, auto b) {
            return a->GetText() < b->GetText();
        });

        m_commands_sorted_dirty = false;
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

CommandDef::CommandDef(std::string name_, std::string text_)
    : name(std::move(name_))
    , text(std::move(text_)) {
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
    this->must_confirm = true;

    return *this;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

CommandContext::CommandContext(void *object, const CommandTable *table)
    : CommandContext(object, table, nullptr) {
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

CommandContext::CommandContext(const CommandTable2 *table2)
    : CommandContext(nullptr, nullptr, table2) {
}

CommandContext::CommandContext(void *object, const CommandTable *table, const CommandTable2 *table2)
    : m_object(object)
    , m_table(table)
    , m_table2(table2) {
    ASSERT(m_table && m_object || m_table2);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void CommandContext::DoButton(const char *name) const {
    ASSERT(m_table);
    Command *command = m_table->FindCommandByName(name);
    if (!command) {
        return;
    }

    if (!m_object) {
        return;
    }

    if (command->m_must_confirm) {
        // TODO - use ImGuiConfirmButton.
    } else if (const bool *ticked = command->IsTicked(m_object)) {
        if (ImGui::RadioButton(command->m_text.c_str(), *ticked)) {
            command->Execute(m_object);
        }
    } else {
        bool enabled = command->IsEnabled(m_object);
        if (ImGuiButton(command->m_text.c_str(), enabled)) {
            command->Execute(m_object);
        }
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void CommandContext::DoMenuItemUI(const char *name) const {
    ASSERT(m_table);
    Command *command = m_table->FindCommandByName(name);
    if (!command) {
        char dummy[100];
        snprintf(dummy, sizeof dummy, "?? - %s", name);
        ImGui::MenuItem(dummy);
        return;
    }

    if (!m_object) {
        return;
    }

    std::string shortcut;
    if (command->m_shortcut != 0) {
        shortcut = GetKeycodeName(command->m_shortcut);
    }

    bool enabled = command->IsEnabled(m_object);

    if (command->m_must_confirm) {
        if (ImGui::BeginMenu(command->m_text.c_str(), enabled)) {
            if (ImGui::MenuItem("Confirm")) {
                command->Execute(m_object);
            }
            ImGui::EndMenu();
        }
    } else {
        const bool *ticked = command->IsTicked(m_object);

        bool selected = false;
        if (ticked && *ticked) {
            selected = true;
        }

        if (ImGui::MenuItem(command->m_text.c_str(), shortcut.empty() ? nullptr : shortcut.c_str(), &selected, enabled)) {
            command->Execute(m_object);
        }
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void CommandContext::DoToggleCheckboxUI(const char *name) const {
    ASSERT(m_table);
    Command *command = m_table->FindCommandByName(name);
    if (!command) {
        return;
    }

    if (!m_object) {
        return;
    }

    bool value = *command->IsTicked(m_object);

    if (ImGui::Checkbox(command->GetText().c_str(), &value)) {
        command->Execute(m_object);
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool CommandContext::ExecuteCommandsForPCKey(uint32_t keycode) const {
    if (m_table) {
        if (m_table->ExecuteCommandsForPCKey(keycode, m_object)) {
            return true;
        }
    }

    if (m_table2) {
        if (m_table2->ActionCommandsForPCKey(keycode)) {
            return true;
        }
    }

    return false;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// This isn't efficient, and nor does it pretend to be.
static std::vector<Command2 *> *g_all_command2s;
static std::vector<CommandTable2 *> *g_all_command_table2s;

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

template <class ContType>
static bool Contains(const ContType &cont, const typename ContType::value_type &thing) {
    auto &&it = std::find(cont.begin(), cont.end(), thing);
    if (it != cont.end()) {
        return true;
    } else {
        return false;
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static std::vector<Command2 *> *GetCommand2sList() {
    static std::vector<Command2 *> s_all_command2s;
    return g_all_command2s = &s_all_command2s;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static std::vector<CommandTable2 *> *GetCommandTable2sList() {
    static std::vector<CommandTable2 *> s_all_command_table2s;
    return g_all_command_table2s = &s_all_command_table2s;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

CommandTable2::CommandTable2(std::string name)
    : m_name(std::move(name)) {
    std::vector<CommandTable2 *> *const list = GetCommandTable2sList();
    ASSERT(!Contains(*list, this));
    list->push_back(this);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

CommandTable2::~CommandTable2() {
    std::vector<CommandTable2 *> *const list = GetCommandTable2sList();
    ASSERT(Contains(*list, this));
    list->erase(std::find(list->begin(), list->end(), this));
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

const std::string &CommandTable2::GetName() const {
    return m_name;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void CommandTable2::ForEachCommand(std::function<void(Command2 *)> fun) const {
    for (Command2 *command : m_commands_sorted) {
        fun(command);
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void CommandTable2::ResetDefaultMappingsByCommand(Command2 *command) {
    m_pc_keys_by_command.erase(command);
    m_commands_by_pc_key_dirty = true;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void CommandTable2::ClearMappingsByCommand(Command2 *command) {
    m_pc_keys_by_command[command].clear();
    m_commands_by_pc_key_dirty = true;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void CommandTable2::AddMapping(uint32_t pc_key, Command2 *command) {
    ASSERT(Contains(m_commands_sorted, command));

    std::vector<uint32_t> *pc_keys = &m_pc_keys_by_command[command];
    if (std::find(pc_keys->begin(), pc_keys->end(), pc_key) == pc_keys->end()) {
        pc_keys->push_back(pc_key);
        m_commands_by_pc_key_dirty = true;
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void CommandTable2::RemoveMapping(uint32_t pc_key, Command2 *command) {
    auto &&command_and_pc_keys = m_pc_keys_by_command.find(command);
    if (command_and_pc_keys == m_pc_keys_by_command.end()) {
        return;
    }

    std::vector<uint32_t> *pc_keys = &command_and_pc_keys->second;

    auto &&pc_key_it = std::find(pc_keys->begin(), pc_keys->end(), pc_key);
    if (pc_key_it != pc_keys->end()) {
        return;
    }

    pc_keys->erase(pc_key_it);

    if (pc_keys->empty()) {
    } else {
    }

    m_commands_by_pc_key_dirty = true;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

const std::vector<uint32_t> *CommandTable2::GetPCKeysForCommand(bool *are_defaults, Command2 *command) const {
    auto &&command_and_pc_keys = m_pc_keys_by_command.find(command);
    if (command_and_pc_keys == m_pc_keys_by_command.end()) {
        if (are_defaults) {
            *are_defaults = true;
        }
        return &command->m_shortcuts;
    } else {
        if (are_defaults) {
            *are_defaults = false;
        }
        return &command_and_pc_keys->second;
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool CommandTable2::ActionCommandsForPCKey(uint32_t pc_key) const {
    if (m_commands_by_pc_key_dirty) {
        m_commands_by_pc_key.clear();

        this->ForEachCommand([this](Command2 *command) {
            const std::vector<uint32_t> *pc_keys = this->GetPCKeysForCommand(nullptr, command);
            for (uint32_t pc_key : *pc_keys) {
                m_commands_by_pc_key[pc_key].push_back(command);
            }
        });
        m_commands_by_pc_key_dirty = false;
    }

    auto &&pc_key_and_commands = m_commands_by_pc_key.find(pc_key);
    if (pc_key_and_commands == m_commands_by_pc_key.end()) {
        return false;
    }

    for (Command2 *command : pc_key_and_commands->second) {
        command->Action();
    }

    return true;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

Command2 *CommandTable2::FindCommandByName(const char *name) const {
    for (Command2 *command : m_commands_sorted) {
        if (command->GetName() == name) {
            return command;
        }
    }

    return nullptr;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

Command2 *CommandTable2::FindCommandByName(const std::string &name) const {
    for (Command2 *command : m_commands_sorted) {
        if (command->GetName() == name) {
            return command;
        }
    }

    return nullptr;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static void AddCommand(std::vector<Command2 *> *commands, Command2 *command) {
    ASSERT(!Contains(*commands, command));
    commands->push_back(command);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static void RemoveCommand(std::vector<Command2 *> *commands, Command2 *command) {
    auto it = std::find(commands->begin(), commands->end(), command);
    ASSERT(it != commands->end());
    commands->erase(it);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void Command2::AddCommand(Command2 *command) {
    ::AddCommand(GetCommand2sList(), command);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void Command2::RemoveCommand(Command2 *command) {
    ::RemoveCommand(GetCommand2sList(), command);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

Command2Data::Command2Data(CommandTable2 *table, std::string name, std::string text)
    : m_table(table)
    , m_name(std::move(name))
    , m_text(std::move(text)) {
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

Command2::Command2(CommandTable2 *table, std::string name, std::string text)
    : Command2Data(table, std::move(name), std::move(text)) {
    AddCommand(this);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

Command2::Command2(const Command2 &src)
    : Command2Data(src) {
    AddCommand(this);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

Command2::Command2(Command2 &&src)
    : Command2Data(src) {
    AddCommand(this);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

Command2::~Command2() {
    RemoveCommand(this);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

const std::string &Command2::GetName() const {
    return m_name;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

const std::string &Command2::GetText() const {
    return m_text;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void Command2::DoButton() {
    if (m_has_tick) {
        if (ImGui::Checkbox(m_text.c_str(), &this->ticked)) {
            this->Action();
        }
    } else {
        if (ImGuiButton(m_text.c_str(), &this->enabled)) {
            this->Action();
        }
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void Command2::DoMenuItem() {
    std::string shortcut_str;
    //if (this->shortcut != 0) {
    //    shortcut_str = GetKeycodeName(this->shortcut);
    //}

    if (m_must_confirm) {
        if (ImGui::BeginMenu(m_text.c_str(), this->enabled)) {
            if (ImGui::MenuItem("Confirm")) {
                this->Action();
            }
            ImGui::EndMenu();
        }
    } else {
        bool t = m_has_tick && ticked;

        if (ImGui::MenuItem(m_text.c_str(),
                            shortcut_str.empty() ? nullptr : shortcut_str.c_str(),
                            &t,
                            this->enabled)) {
            this->Action();
        }
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void Command2::DoToggleCheckbox() {
    if (ImGui::Checkbox(m_text.c_str(), &this->ticked)) {
        this->Action();
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool Command2::WasActioned() const {
    ASSERT(g_linked);

    if (m_frame_counter != 0) {
        uint64_t f = GetImGuiFrameCounter();
        if (m_frame_counter == f - 1) {
            m_frame_counter = 0;
            return true;
        }
    }

    return false;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

Command2 Command2::MustConfirm() const {
    Command2 c = *this;

    c.m_must_confirm = true;

    return c;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

Command2 Command2::WithTick() const {
    Command2 c = *this;

    c.m_has_tick = true;

    return c;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

Command2 Command2::WithShortcut(uint32_t shortcut) const {
    Command2 c = *this;

    c.m_shortcuts.push_back(shortcut);

    return c;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void Command2::Action() {
    m_frame_counter = GetImGuiFrameCounter();
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void ForEachCommand2(std::function<void(Command2 *)> fun) {
    ASSERT(g_linked);

    for (Command2 *command : *GetCommand2sList()) {
        fun(command);
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void ForEachCommandTable2(std::function<void(CommandTable2 *)> fun) {
    ASSERT(g_linked);

    for (CommandTable2 *table : *GetCommandTable2sList()) {
        fun(table);
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

CommandTable2 *FindCommandTable2ByName(const std::string &name) {
    for (CommandTable2 *table : *GetCommandTable2sList()) {
        if (table->GetName() == name) {
            return table;
        }
    }

    return nullptr;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void LinkCommands() {
    ASSERT(!g_linked);

    std::set<std::string> names;
    for (Command2 *command : *GetCommand2sList()) {
        ASSERT(names.find(command->GetName()) == names.end());
        names.insert(command->GetName());

        if (!command->m_table) {
            continue;
        }

        ASSERT(Contains(*GetCommand2sList(), command));
        ASSERT(!Contains(command->m_table->m_commands_sorted, command));
        command->m_table->m_commands_sorted.push_back(command);
    }

    for (CommandTable2 *table : *GetCommandTable2sList()) {
        std::sort(table->m_commands_sorted.begin(),
                  table->m_commands_sorted.end(),
                  [](const Command2 *a, const Command2 *b) {
                      return a->GetText() < b->GetText();
                  });
    }

    g_linked = true;
}
