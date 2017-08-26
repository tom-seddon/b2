#ifndef HEADER_6958596828274A368C8F315251DB0613// -*- mode:c++ -*-
#define HEADER_6958596828274A368C8F315251DB0613

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#include <string>
#include <functional>
#include <memory>
#include <map>
#include "keymap.h"
#include <string.h>

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
    Command(std::string name,std::string text,bool confirm);

    virtual void Execute(void *object) const=0;
    virtual const bool *IsTicked(void *object) const=0;
    virtual bool IsEnabled(void *object) const=0;

    const std::string &GetName() const;
    const std::string &GetText() const;
protected:
private:
    const std::string m_name;
    std::string m_text;
    uint32_t m_shortcut=0;
    bool m_confirm=false;

    friend class CommandTable;
    friend class CommandContext;
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

struct ConfirmCommand {
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
                  bool confirm):
        Command(std::move(name),std::move(text),confirm),
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

// the CommandTable conforms to roughly the same interface as Keymap,
// though not perfectly - just enough so that the keymap configuration
// UI can share code between the two cases (it has to be templated
// anyway because the value types are different...), and so that the
// window code for handling Beeb keypresses is pretty similar to the
// code for handling command keypresses.

class CommandTable {
public:
    typedef Command *ValueType;

    static void ForEachCommandTable(std::function<void(CommandTable *)> fun);
    static CommandTable *FindCommandTableByName(const std::string &name);

    CommandTable(std::string name);
    ~CommandTable();

    Command *FindCommandByName(const char *name) const;
    Command *FindCommandByName(const std::string &str) const;

    const std::string &GetName() const;

    void ForEachCommand(std::function<void(Command *)> fun);

    void ClearMappingsByCommand(Command *command);
    void SetMapping(uint32_t pc_key,Command *command,bool state);
    const uint32_t *GetPCKeysForValue(Command *command) const;
    const Command *const *GetValuesForPCKey(uint32_t key) const;
protected:
    Command *AddCommand(std::unique_ptr<Command> command);
private:
    struct KeymapTraits {
        typedef Command *ValueType;
        static constexpr Command *TERMINATOR=nullptr;
    };

    struct StringLessThan {
        inline bool operator()(const char *a,const char *b) const {
            return strcmp(a,b)<0;
        }
    };
    std::map<const char *,std::unique_ptr<Command>,StringLessThan> m_command_by_name;
    Keymap<KeymapTraits> m_keymap;

    std::vector<Command *> m_commands_sorted;
    bool m_commands_sorted_dirty=true;

    void UpdateSortedCommands();
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

template<class T>
class ObjectCommandTable:
    public CommandTable
{
public:
    struct Initializer {
        std::string name;
        std::string text;
        void (T::*execute_mfn)()=nullptr;
        bool (T::*ticked_mfn)() const=nullptr;
        bool (T::*enabled_mfn)() const=nullptr;
        bool confirm=false;
        intptr_t arg=0;

        Initializer(std::string name_,std::string text_,void (T::*execute_mfn_)(),bool (T::*ticked_mfn_)() const=nullptr,bool (T::*enabled_mfn_)() const=nullptr):
            name(std::move(name_)),
            text(std::move(text_)),
            execute_mfn(std::move(execute_mfn_)),
            ticked_mfn(ticked_mfn_),
            enabled_mfn(enabled_mfn_)
        {
        }

        Initializer(std::string name_,std::string text_,void (T::*execute_mfn_)(),ConfirmCommand):
            name(std::move(name_)),
            text(std::move(text_)),
            execute_mfn(std::move(execute_mfn_)),
            confirm(true)
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

            this->AddCommand(std::make_unique<ObjectCommand<T>>(std::move(init.name),
                                                                std::move(init.text),
                                                                std::move(execute_fun),
                                                                std::move(ticked_fun),
                                                                std::move(enabled_fun),
                                                                init.confirm));
        }
    }
protected:
private:
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

class CommandContext
{
public:
    CommandContext(void *object,const CommandTable *table);

    CommandContext(const CommandContext &)=delete;
    CommandContext &operator=(const CommandContext &)=delete;

    CommandContext(CommandContext &&)=delete;
    CommandContext &operator=(CommandContext &&)=delete;

    void Reset();

    void DoButton(const char *name);
    void DoMenuItemUI(const char *name);
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

    void DoButton(const char *name) {
        this->cc->DoButton(name);
    }

    void DoMenuItemUI(const char *name) {
        this->cc->DoMenuItemUI(name);
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
