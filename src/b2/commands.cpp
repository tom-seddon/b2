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

CommandTable2::CommandTable2(std::string name, int default_command_visibility)
    : m_name(std::move(name))
    , m_default_command_visibility(!!default_command_visibility) {
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

Command2::Command2()
    : Command2Data(nullptr, std::string(), std::string()) {
    AddCommand(this);
}

Command2::Command2(CommandTable2 *table, std::string name, std::string text)
    : Command2Data(table, std::move(name), std::move(text)) {
    AddCommand(this);
    if (table) {
        m_visible = table->m_default_command_visibility;
    }
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

bool Command2::IsVisible() const {
    return m_visible;
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

    uint64_t frame_counter = m_frame_counter;
    m_frame_counter = 0;

    if (this->enabled) {
        if (frame_counter != 0) {
            uint64_t f = GetImGuiFrameCounter();
            if (frame_counter == f - 1) {
                return true;
            }
        }
    }

    return false;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

Command2 &Command2::MustConfirm() {
    m_must_confirm = true;

    return *this;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

Command2 &Command2::WithTick() {
    m_has_tick = true;

    return *this;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

Command2 &Command2::WithShortcut(uint32_t shortcut) {
    m_shortcuts.push_back(shortcut);

    return *this;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

Command2 &Command2::VisibleIf(int flag) {
    if (!flag) {
        m_visible = false;
    }

    return *this;
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

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
