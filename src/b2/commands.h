#ifndef HEADER_6958596828274A368C8F315251DB0613 // -*- mode:c++ -*-
#define HEADER_6958596828274A368C8F315251DB0613

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#include <string>
#include <functional>
#include <map>
#include <string.h>
#include <vector>

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
//
//
//
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

class Command2;

class CommandTable2 {
  public:
    typedef Command2 CommandType; //temporary measure

    explicit CommandTable2(std::string name, int default_command_visibility = 1);
    ~CommandTable2();

    CommandTable2(const CommandTable2 &) = delete;
    CommandTable2 &operator=(const CommandTable2 &) = delete;
    CommandTable2(CommandTable2 &&) = delete;
    CommandTable2 &operator=(CommandTable2 &&) = delete;

    const std::string &GetName() const;

    void ForEachCommand(std::function<void(Command2 *)> fun) const;

    // sets command to have its default shortcuts.
    void ResetDefaultMappingsByCommand(Command2 *command);

    // sets command to explicitly have no shortcuts at all.
    void ClearMappingsByCommand(Command2 *command);

    void AddMapping(uint32_t pc_key, Command2 *command);
    void RemoveMapping(uint32_t pc_key, Command2 *command);

    const std::vector<uint32_t> *GetPCKeysForCommand(bool *are_defaults, const Command2 *command) const;
    const std::vector<Command2 *> *GetCommandsForPCKey(uint32_t pc_key) const;

    Command2 *FindCommandByName(const char *name) const;
    Command2 *FindCommandByName(const std::string &name) const;

  protected:
  private:
    std::string m_name;
    bool m_default_command_visibility = true;

    std::map<const Command2 *, std::vector<uint32_t>> m_pc_keys_by_command;

    mutable std::map<uint32_t, std::vector<Command2 *>> m_commands_by_pc_key;
    mutable bool m_commands_by_pc_key_dirty = true;

    std::vector<Command2 *> m_commands_sorted;

    friend class Command2;
    friend class CommandStateTable;
    friend void LinkCommands();
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// Annoying 2-part class, so that the default constructors can be used for the
// data while still having extra code to look after a global list of these
// things.

class Command2Data {
  public:
    Command2Data(CommandTable2 *table, std::string name, std::string text);

  protected:
    CommandTable2 *m_table = nullptr;
    std::string m_name;
    std::string m_text;
    std::string m_extra_text;
    bool m_must_confirm = false;
    bool m_has_tick = false;
    std::vector<uint32_t> m_shortcuts;
    bool m_visible = true;
    size_t m_index = ~(size_t)0;
    bool m_always_prioritized = false;

    Command2Data(const Command2Data &) = default;
    Command2Data(Command2Data &&) = default;
    Command2Data &operator=(const Command2Data &) = default;
    Command2Data &operator=(Command2Data &&) = default;

  private:
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

class Command2 : private Command2Data {
  public:
    Command2();
    Command2(CommandTable2 *table, std::string id, std::string label);
    ~Command2();

    Command2(const Command2 &);
    Command2 &operator=(const Command2 &) = default;
    Command2(Command2 &&);
    Command2 &operator=(Command2 &&) = default;

    // TODO: terminology...
    const std::string &GetName() const;
    const std::string &GetText() const;
    const std::string &GetExtraText() const;
    bool IsAlwaysPrioritized() const;

    // Invisible commands refer to functionality that's compiled out of this
    // build, or otherwise hidden. They remain present, but aren't exposed in
    // the UI.
    bool IsVisible() const;

    //bool WasActioned() const;

    // fluent interface
    Command2 &MustConfirm();
    Command2 &WithTick();
    Command2 &WithShortcut(uint32_t shortcut);
    Command2 &VisibleIf(int flag); //invisible if any such flag is false
    Command2 &WithExtraText(std::string extra_text);
    Command2 &AlwaysPrioritized();

  protected:
  private:
    static void AddCommand(Command2 *command);
    static void RemoveCommand(Command2 *command);

    friend class CommandTable2;
    friend class CommandStateTable;
    friend void LinkCommands();
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// Holds per-window state for each command. Every state table has room for all
// possible commands, so mix and match as appropriate.

class CommandStateTable {
  public:
    CommandStateTable();
    ~CommandStateTable();

    bool GetEnabled(const Command2 &command) const;
    void SetEnabled(const Command2 &command, bool enabled);

    bool GetTicked(const Command2 &command) const;
    void SetTicked(const Command2 &command, bool ticked);

    void DoButton(const Command2 &command);
    void DoMenuItem(const Command2 &command);
    void DoToggleCheckbox(const Command2 &command);

    bool WasActioned(const Command2 &command);

    bool ActionCommand(Command2 *command);
    bool ActionCommands(const std::vector<Command2 *> *commands);

    bool ActionCommandsForPCKey(const CommandTable2 &table, uint32_t pc_key);

  protected:
  private:
    struct State {
        bool actioned : 1;
        bool ticked : 1;
        bool enabled : 1;
    };
    CHECK_SIZEOF(State, 1);

    void Action();

    std::vector<State> m_states;
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void ForEachCommand2(std::function<void(Command2 *)> fun);
void ForEachCommandTable2(std::function<void(CommandTable2 *)> fun);
CommandTable2 *FindCommandTable2ByName(const std::string &name);

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// Call once at start of main.
void LinkCommands();

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#endif
