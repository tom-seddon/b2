#ifndef HEADER_6958596828274A368C8F315251DB0613// -*- mode:c++ -*-
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
    Command(std::string name,std::string text,bool must_confirm,std::vector<uint32_t> default_shortcuts);
    virtual ~Command()=0;

    virtual void Execute(void *object) const=0;
    virtual const bool *IsTicked(void *object) const=0;
    virtual bool IsEnabled(void *object) const=0;

    const std::string &GetName() const;
    const std::string &GetText() const;

    const std::vector<uint32_t> *GetDefaultShortcuts() const;
protected:
private:
    const std::string m_name;
    std::string m_text;
    uint32_t m_shortcut=0;
    bool m_must_confirm=false;
    std::vector<uint32_t> m_default_shortcuts;

    friend class CommandTable;
    friend class CommandContext;
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

template<class T>
class ObjectCommand:
    public Command
{
public:
    ObjectCommand(std::string name,std::string text,
                  std::function<void(T *)> execute_fun,
                  std::function<bool(const T *)> ticked_fun,
                  std::function<bool(const T *)> enabled_fun,
                  bool must_confirm,
                  std::vector<uint32_t> default_shortcuts):
        Command(std::move(name),std::move(text),must_confirm,std::move(default_shortcuts)),
        m_execute_fun(std::move(execute_fun)),
        m_ticked_fun(std::move(ticked_fun)),
        m_enabled_fun(std::move(enabled_fun))
    {
        ASSERT(!!m_execute_fun);
    }

    void Execute(void *object_) const override {
        auto object=(T *)object_;

        m_execute_fun(object);
    }

    const bool *IsTicked(void *object_) const override {
        static const bool s_true=true,s_false=false;

        if(!m_ticked_fun) {
            return nullptr;
        } else {
            auto object=(T *)object_;
            bool ticked=m_ticked_fun(object);
            if(ticked) {
                return &s_true;
            } else {
                return &s_false;
            }
        }
    }

    bool IsEnabled(void *object_) const override {
        bool enabled=true;

        if(!!m_enabled_fun) {
            auto object=(T *)object_;
            enabled=m_enabled_fun(object);
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

    void AddMapping(uint32_t pc_key,Command *command);
    void RemoveMapping(uint32_t pc_key,Command *command);

    const std::vector<uint32_t> *GetPCKeysForCommand(bool *are_defaults,Command *command) const;

    bool ExecuteCommandsForPCKey(uint32_t keycode,void *object) const;

    //const std::vector<Command *> *GetCommandsForPCKey(uint32_t key) const;
protected:
    Command *AddCommand(std::unique_ptr<Command> command);
private:
    std::string m_name;

    struct StringLessThan {
        inline bool operator()(const char *a,const char *b) const {
            return strcmp(a,b)<0;
        }
    };
    std::map<const char *,std::unique_ptr<Command>,StringLessThan> m_command_by_name;

    std::map<Command *,std::vector<uint32_t>> m_pc_keys_by_command;

    mutable std::map<uint32_t,std::vector<Command *>> m_commands_by_pc_key;
    mutable bool m_commands_by_pc_key_dirty=true;

    mutable std::vector<Command *> m_commands_sorted;
    mutable bool m_commands_sorted_dirty=true;

    void UpdateSortedCommands() const;
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

struct CommandDef {
    std::string name;
    std::string text;
    std::vector<uint32_t> shortcuts;
    bool must_confirm=false;

    CommandDef(std::string name,std::string text);

    CommandDef &Shortcut(uint32_t keycode);
    CommandDef &MustConfirm();
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

template<class T>
class ObjectCommandTable:
    public CommandTable
{
public:
    struct Initializer {
        CommandDef def;
        void (T::*execute_mfn)()=nullptr;
        bool (T::*ticked_mfn)() const=nullptr;
        bool (T::*enabled_mfn)() const=nullptr;

        Initializer(CommandDef def_,void (T::*execute_mfn_)(),bool (T::*ticked_mfn_)() const=nullptr,bool (T::*enabled_mfn_)() const=nullptr):
            def(std::move(def_)),
            execute_mfn(std::move(execute_mfn_)),
            ticked_mfn(ticked_mfn_),
            enabled_mfn(enabled_mfn_)
        {
        }
    };

    ObjectCommandTable(std::string table_name,std::initializer_list<Initializer> inits):
        CommandTable(std::move(table_name))
    {
        for(const Initializer &init:inits) {
            std::function<void(T *)> execute_fun=[mfn=init.execute_mfn](T *p) {
                (p->*mfn)();
            };

            std::function<bool(const T *)> ticked_fun;
            if(init.ticked_mfn) {
                ticked_fun=[mfn=init.ticked_mfn](const T *p) {
                    return (p->*mfn)();
                };
            }

            std::function<bool(const T *)> enabled_fun;
            if(init.enabled_mfn) {
                enabled_fun=[mfn=init.enabled_mfn](const T *p) {
                    return (p->*mfn)();
                };
            }

            Command *command=this->AddCommand(std::make_unique<ObjectCommand<T>>(std::move(init.def.name),
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

class CommandContext {
public:
    CommandContext(void *object,const CommandTable *table);

    CommandContext(const CommandContext &)=delete;
    CommandContext &operator=(const CommandContext &)=delete;

    CommandContext(CommandContext &&)=delete;
    CommandContext &operator=(CommandContext &&)=delete;

    void Reset();

    void DoButton(const char *name);
    void DoMenuItemUI(const char *name);

    // The check reflects the tick status, and the command is assumed to toggle that state.
    void DoToggleCheckboxUI(const char *name);
    bool ExecuteCommandsForPCKey(uint32_t keycode) const;

    const CommandTable *GetCommandTable() const;
protected:
private:
    void *m_object=nullptr;
    const CommandTable *m_table=nullptr;
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// This object does three things:
//
// 1. ensure a (non-templated) CommandContext is created in a
// type-safe fashion
//
// 2. creates a shareable CommandContext that can outlive the
// ObjectCommandContext. The context stack is only updated during the
// imgui step, but its contents have to stick around past that, so it
// can be handled by the SDL key event handler. This means that the
// CommandContext objects need to be able to outlive (if hopefully
// only briefly) the ObjectCommandContext they come from
//
// 3. calls Reset on its CommandContext on destruction, so that the
// CommandContext knows that its ObjectCommandContext has gone away
//
// (If point 2 weren't an issue, this could be simpler. Perhaps
// keyboard shortcuts could/should be handled by the dear imgui
// keyboard stuff instead?)

template<class T>
class ObjectCommandContext {
public:
    const std::shared_ptr<CommandContext> cc;

    ObjectCommandContext(T *object,const ObjectCommandTable<T> *table):
        cc(std::make_shared<CommandContext>(object,table))
    {
    }

    ~ObjectCommandContext() {
        this->cc->Reset();
    }

    void DoButton(const char *name) const {
        this->cc->DoButton(name);
    }

    void DoMenuItemUI(const char *name) const {
        this->cc->DoMenuItemUI(name);
    }

    void DoToggleCheckboxUI(const char *name) const {
        this->cc->DoToggleCheckboxUI(name);
    }
protected:
private:
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

class CommandContextStack {
public:
    template<class T>
    void Push(const ObjectCommandContext<T> &occ,bool force=false) {
        this->Push(occ.cc,force);
    }

    void Reset();

    void Push(const std::shared_ptr<CommandContext> &cc,bool force=false);

    size_t GetNumCCs() const;

    // index 0 is top of stack.
    const std::shared_ptr<CommandContext> &GetCCByIndex(size_t index) const;

    bool ExecuteCommandsForPCKey(uint32_t keycode) const;
protected:
private:
    std::vector<std::shared_ptr<CommandContext>> m_ccs;
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#endif
