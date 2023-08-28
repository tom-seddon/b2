#ifndef HEADER_6958596828274A368C8F315251DB0613 // -*- mode:c++ -*-
#define HEADER_6958596828274A368C8F315251DB0613

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#include <string>
#include <functional>
#include <memory>
#include <map>
#include <string.h>
#include <vector>
#include <shared/debug.h>

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
//
// To use a command:
//
// 0. start dear imgui frame
//
// 1. set enabled/ticked
//
// 2. check WasActioned - do the action if so
//
// 3. DoButton/DoMenuItem/etc. to show the UI where necessary
//
// Steps 1 and 2 should be performed on every frame, even if step 3 won't, so
// that keyboard shortcuts can work correctly.
//
// Having the command execute on the following frame isn't really ideal, but
// 1-frame delays are inevitable with the immediate mode approach...
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

    const std::vector<uint32_t> *GetPCKeysForCommand(bool *are_defaults, Command2 *command) const;

    bool ActionCommandsForPCKey(uint32_t keycode) const;

    Command2 *FindCommandByName(const char *name) const;
    Command2 *FindCommandByName(const std::string &name) const;

  protected:
  private:
    std::string m_name;
    bool m_default_command_visibility = true;

    std::map<Command2 *, std::vector<uint32_t>> m_pc_keys_by_command;

    mutable std::map<uint32_t, std::vector<Command2 *>> m_commands_by_pc_key;
    mutable bool m_commands_by_pc_key_dirty = true;

    std::vector<Command2 *> m_commands_sorted;

    friend class Command2;
    friend void LinkCommands();
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// Annoying 2-part class, so that the default constructors can be used for the
// data while still having extra code to look after a global list of these
// things.

class Command2Data {
  public:
    bool enabled = true;
    bool ticked = false;

    Command2Data(CommandTable2 *table, std::string name, std::string text);

  protected:
    CommandTable2 *m_table = nullptr;
    std::string m_name;
    std::string m_text;
    bool m_must_confirm = false;
    bool m_has_tick = false;
    mutable uint64_t m_frame_counter = 0;
    std::vector<uint32_t> m_shortcuts;
    bool m_visible = true;

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
    using Command2Data::enabled;
    using Command2Data::ticked;

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

    // Invisible commands refer to functionality that's compiled out of this
    // build, or otherwise hidden. They remain present, but aren't exposed in
    // the UI.
    bool IsVisible() const;

    void DoButton();
    void DoMenuItem();
    void DoToggleCheckbox();

    bool WasActioned() const;

    // fluent interface
    Command2 &MustConfirm();
    Command2 &WithTick();
    Command2 &WithShortcut(uint32_t shortcut);
    Command2 &VisibleIf(int flag); //invisible if any such flag is false

  protected:
  private:
    void Action();

    static void AddCommand(Command2 *command);
    static void RemoveCommand(Command2 *command);

    friend class CommandTable2;
    friend void LinkCommands();
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
