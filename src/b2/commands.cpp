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
    if (pc_key_it == pc_keys->end()) {
        return;
    }

    pc_keys->erase(pc_key_it);

    if (pc_keys->empty()) {
        m_pc_keys_by_command.erase(command);
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

const std::vector<Command2 *> *CommandTable2::GetCommandsForPCKey(uint32_t pc_key) const {
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
        return nullptr;
    }

    return &pc_key_and_commands->second;
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
    ASSERT(!g_linked);
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

Command2 &Command2::MustConfirm() {
    ASSERT(!m_has_tick); //Dear ImGui submenus can't have ticks :(

    m_must_confirm = true;

    return *this;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

Command2 &Command2::WithTick() {
    ASSERT(!m_must_confirm); //Dear ImGui submenus can't have ticks :(

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

CommandStateTable::CommandStateTable() {
    ASSERT(g_linked);

    State initial_state = {};
    initial_state.enabled = 1;

    m_states.resize(GetCommand2sList()->size(), initial_state);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

CommandStateTable::~CommandStateTable() {
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool CommandStateTable::GetEnabled(const Command2 &command) const {
    ASSERT(command.m_index < m_states.size());
    const State *state = &m_states[command.m_index];

    return state->enabled;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void CommandStateTable::SetEnabled(const Command2 &command, bool enabled) {
    ASSERT(command.m_index < m_states.size());
    State *state = &m_states[command.m_index];

    state->enabled = enabled;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool CommandStateTable::GetTicked(const Command2 &command) const {
    ASSERT(command.m_index < m_states.size());
    const State *state = &m_states[command.m_index];

    return state->ticked;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void CommandStateTable::SetTicked(const Command2 &command, bool ticked) {
    ASSERT(command.m_index < m_states.size());
    State *state = &m_states[command.m_index];

    state->ticked = ticked;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void CommandStateTable::DoButton(const Command2 &command) {
    ASSERT(command.m_index < m_states.size());
    State *state = &m_states[command.m_index];

    if (command.m_has_tick) {
        bool ticked = state->ticked;
        if (ImGui::Checkbox(command.m_text.c_str(), &ticked)) {
            state->ticked = ticked;
            state->actioned = 1;
        }
    } else {
        if (ImGuiButton(command.m_text.c_str(), state->enabled)) {
            state->actioned = 1;
        }
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void CommandStateTable::DoMenuItem(const Command2 &command) {
    ASSERT(command.m_index < m_states.size());
    State *state = &m_states[command.m_index];

    std::string shortcut_str;
    //if (this->shortcut != 0) {
    //    shortcut_str = GetKeycodeName(this->shortcut);
    //}

    if (command.m_must_confirm) {
        if (ImGui::BeginMenu(command.m_text.c_str(), state->enabled)) {
            if (ImGui::MenuItem("Confirm")) {
                state->actioned = 1;
            }
            ImGui::EndMenu();
        }
    } else {
        bool ticked = command.m_has_tick && state->ticked;

        if (ImGui::MenuItem(command.m_text.c_str(),
                            shortcut_str.empty() ? nullptr : shortcut_str.c_str(),
                            &ticked,
                            state->enabled)) {
            state->actioned = 1;
        }
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void CommandStateTable::DoToggleCheckbox(const Command2 &command) {
    ASSERT(command.m_index < m_states.size());
    State *state = &m_states[command.m_index];

    bool ticked = state->ticked;
    if (ImGui::Checkbox(command.m_text.c_str(), &ticked)) {
        if (state->enabled) {
            state->actioned = 1;
        }
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool CommandStateTable::WasActioned(const Command2 &command) {
    ASSERT(command.m_index < m_states.size());
    State *state = &m_states[command.m_index];

    bool actioned = state->actioned;
    state->actioned = 0;

    return actioned;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool CommandStateTable::ActionCommandsForPCKey(const CommandTable2 &table, uint32_t pc_key) {
    const std::vector<Command2 *> *commands = table.GetCommandsForPCKey(pc_key);
    if (!commands) {
        return false;
    }

    for (Command2 *command : *commands) {
        ASSERT(command->m_index < m_states.size());
        State *state = &m_states[command->m_index];

        if (state->enabled) {
            state->actioned = 1;
        }
    }

    return true;
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

    const std::vector<Command2 *> *list = GetCommand2sList();

    std::set<std::string> names;
    for (size_t i = 0; i < list->size(); ++i) {
        Command2 *command = (*list)[i];
        ASSERT(names.find(command->GetName()) == names.end());
        names.insert(command->GetName());

        ASSERT(command->m_index == ~(size_t)0);
        command->m_index = i;

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
