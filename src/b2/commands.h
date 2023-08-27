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

// The CommandTable holds a list of commands. Each command is just
// name/text/member function pointer, with the object address being
// supplied each time the command is invoked. (The commands aren't
// bound to an object; the table has to be a global so that the keymap
// editor can edit it...)

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

class Command {
  public:
    Command(std::string name, std::string text, bool must_confirm, std::vector<uint32_t> default_shortcuts);
    virtual ~Command() = 0;

    virtual void Execute(void *object) const = 0;
    virtual const bool *IsTicked(void *object) const = 0;
    virtual bool IsEnabled(void *object) const = 0;

    const std::string &GetName() const;
    const std::string &GetText() const;

    const std::vector<uint32_t> *GetDefaultShortcuts() const;

  protected:
  private:
    const std::string m_name;
    std::string m_text;
    uint32_t m_shortcut = 0;
    bool m_must_confirm = false;
    std::vector<uint32_t> m_default_shortcuts;

    friend class CommandTable;
    friend class CommandContext;
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

template <class T>
class ObjectCommand : public Command {
  public:
    ObjectCommand(std::string name, std::string text,
                  std::function<void(T *)> execute_fun,
                  std::function<bool(const T *)> ticked_fun,
                  std::function<bool(const T *)> enabled_fun,
                  bool must_confirm,
                  std::vector<uint32_t> default_shortcuts)
        : Command(std::move(name), std::move(text), must_confirm, std::move(default_shortcuts))
        , m_execute_fun(std::move(execute_fun))
        , m_ticked_fun(std::move(ticked_fun))
        , m_enabled_fun(std::move(enabled_fun)) {
        ASSERT(!!m_execute_fun);
    }

    void Execute(void *object_) const override {
        auto object = (T *)object_;

        m_execute_fun(object);
    }

    const bool *IsTicked(void *object_) const override {
        static const bool s_true = true, s_false = false;

        if (!m_ticked_fun) {
            return nullptr;
        } else {
            auto object = (T *)object_;
            bool ticked = m_ticked_fun(object);
            if (ticked) {
                return &s_true;
            } else {
                return &s_false;
            }
        }
    }

    bool IsEnabled(void *object_) const override {
        bool enabled = true;

        if (!!m_enabled_fun) {
            auto object = (T *)object_;
            enabled = m_enabled_fun(object);
        }

        return enabled;
    }

  private:
    std::function<void(T *)> m_execute_fun;
    std::function<bool(const T *)> m_ticked_fun;
    std::function<bool(const T *)> m_enabled_fun;
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

class CommandTable {
  public:
    typedef Command CommandType; //temporary measure

    static void ForEachCommandTable(std::function<void(CommandTable *)> fun);
    static CommandTable *FindCommandTableByName(const std::string &name);

    CommandTable(std::string name);
    ~CommandTable();

    Command *FindCommandByName(const char *name) const;
    Command *FindCommandByName(const std::string &str) const;

    const std::string &GetName() const;

    void ForEachCommand(std::function<void(Command *)> fun) const;

    // sets command to have its default shortcuts.
    void ResetDefaultMappingsByCommand(Command *command);

    // sets command to explicitly have no shortcuts at all.
    void ClearMappingsByCommand(Command *command);

    void AddMapping(uint32_t pc_key, Command *command);
    void RemoveMapping(uint32_t pc_key, Command *command);

    const std::vector<uint32_t> *GetPCKeysForCommand(bool *are_defaults, Command *command) const;

    bool ExecuteCommandsForPCKey(uint32_t keycode, void *object) const;

    //const std::vector<Command *> *GetCommandsForPCKey(uint32_t key) const;
  protected:
    Command *AddCommand(std::unique_ptr<Command> command);

  private:
    std::string m_name;

    struct StringLessThan {
        inline bool operator()(const char *a, const char *b) const {
            return strcmp(a, b) < 0;
        }
    };
    std::map<const char *, std::unique_ptr<Command>, StringLessThan> m_command_by_name;

    std::map<Command *, std::vector<uint32_t>> m_pc_keys_by_command;

    mutable std::map<uint32_t, std::vector<Command *>> m_commands_by_pc_key;
    mutable bool m_commands_by_pc_key_dirty = true;

    mutable std::vector<Command *> m_commands_sorted;
    mutable bool m_commands_sorted_dirty = true;

    void UpdateSortedCommands() const;
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

struct CommandDef {
    std::string name;
    std::string text;
    std::vector<uint32_t> shortcuts;
    bool must_confirm = false;

    CommandDef(std::string name, std::string text);

    CommandDef &Shortcut(uint32_t keycode);
    CommandDef &MustConfirm();
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

template <class T>
class ObjectCommandTable : public CommandTable {
  public:
    struct Initializer {
        CommandDef def;
        void (T::*execute_mfn)() = nullptr;
        bool (T::*ticked_mfn)() const = nullptr;
        bool (T::*enabled_mfn)() const = nullptr;

        Initializer(CommandDef def_, void (T::*execute_mfn_)(), bool (T::*ticked_mfn_)() const = nullptr, bool (T::*enabled_mfn_)() const = nullptr)
            : def(std::move(def_))
            , execute_mfn(std::move(execute_mfn_))
            , ticked_mfn(ticked_mfn_)
            , enabled_mfn(enabled_mfn_) {
        }
    };

    ObjectCommandTable(std::string table_name, std::initializer_list<Initializer> inits)
        : CommandTable(std::move(table_name)) {
        for (const Initializer &init : inits) {
            std::function<void(T *)> execute_fun = [mfn = init.execute_mfn](T *p) {
                (p->*mfn)();
            };

            std::function<bool(const T *)> ticked_fun;
            if (init.ticked_mfn) {
                ticked_fun = [mfn = init.ticked_mfn](const T *p) {
                    return (p->*mfn)();
                };
            }

            std::function<bool(const T *)> enabled_fun;
            if (init.enabled_mfn) {
                enabled_fun = [mfn = init.enabled_mfn](const T *p) {
                    return (p->*mfn)();
                };
            }

            Command *command = this->AddCommand(std::make_unique<ObjectCommand<T>>(std::move(init.def.name),
                                                                                   std::move(init.def.text),
                                                                                   std::move(execute_fun),
                                                                                   std::move(ticked_fun),
                                                                                   std::move(enabled_fun),
                                                                                   init.def.must_confirm,
                                                                                   std::move(init.def.shortcuts)));
            (void)command;
        }
    }

  protected:
  private:
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

class CommandTable2;

// Links a CommandTable with its object's self pointer.
//
// (Not a great name for it - should really be BoundCommandTable, or something.)
class CommandContext {
  public:
    CommandContext() = default;
    explicit CommandContext(const CommandTable2 *table2);
    CommandContext(void *object, const CommandTable *table);
    CommandContext(void *object, const CommandTable *table, const CommandTable2 *table2);

    void DoButton(const char *name) const;
    void DoMenuItemUI(const char *name) const;

    // The check reflects the tick status, and the command is assumed to toggle that state.
    void DoToggleCheckboxUI(const char *name) const;

    // When the context isn't bound, does nothing and returns false.
    bool ExecuteCommandsForPCKey(uint32_t keycode) const;

  protected:
  private:
    void *m_object = nullptr;
    const CommandTable *m_table = nullptr;
    const CommandTable2 *m_table2 = nullptr;
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// New Command Table - see https://github.com/tom-seddon/b2/issues/262

class Command2;

class CommandTable2 {
  public:
    typedef Command2 CommandType; //temporary measure

    CommandTable2(std::string name);
    ~CommandTable2();

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

    std::map<Command2 *, std::vector<uint32_t>> m_pc_keys_by_command;

    mutable std::map<uint32_t, std::vector<Command2 *>> m_commands_by_pc_key;
    mutable bool m_commands_by_pc_key_dirty = true;

    std::vector<Command2 *> m_commands_sorted;

    friend class Command2;
    friend void LinkCommands();
};

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

    Command2Data(const Command2Data &) = default;
    Command2Data(Command2Data &&) = default;

  private:
    Command2Data &operator=(const Command2Data &) = delete;
    Command2Data &operator=(Command2Data &&) = delete;
};

class Command2 : private Command2Data {
  public:
    using Command2Data::enabled;
    using Command2Data::ticked;

    Command2(CommandTable2 *table, std::string id, std::string label);
    ~Command2();

    // TODO: terminology...
    const std::string &GetName() const;
    const std::string &GetText() const;

    void DoButton();
    void DoMenuItem();
    void DoToggleCheckbox();

    bool WasActioned() const;

    // fluent interface
    Command2 MustConfirm() const;
    Command2 WithTick() const;
    Command2 WithShortcut(uint32_t shortcut) const;

  protected:
  private:
    void Action();

    Command2(const Command2 &);
    Command2 &operator=(const Command2 &) = delete;
    Command2(Command2 &&);
    Command2 &operator=(Command2 &&) = delete;

    static void AddCommand(Command2 *command);
    static void RemoveCommand(Command2 *command);

    friend class CommandTable2;
    friend void LinkCommands();
};

void ForEachCommand2(std::function<void(Command2 *)> fun);
void ForEachCommandTable2(std::function<void(CommandTable2 *)> fun);
CommandTable2 *FindCommandTable2ByName(const std::string &name);

void LinkCommands();

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#endif
