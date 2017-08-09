#ifndef HEADER_6958596828274A368C8F315251DB0613// -*- mode:c++ -*-
#define HEADER_6958596828274A368C8F315251DB0613

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#include <string>
#include <functional>
#include <memory>
#include <map>
#include "keymap.h"

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
    virtual bool IsTicked(void *object) const=0;
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
                  std::function<bool(T *)> ticked_fun,
                  std::function<bool(T *)> enabled_fun,
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

    bool IsTicked(void *object) const override {
        return this->DoBoolFun(object,m_ticked_fun,false);
    }

    bool IsEnabled(void *object) const override {
        return this->DoBoolFun(object,m_enabled_fun,true);
    }
private:
    std::function<void(T *)> m_execute_fun;
    std::function<bool(T *)> m_ticked_fun;
    std::function<bool(T *)> m_enabled_fun;

    bool DoBoolFun(void *object_,
                   const std::function<bool(T *)> &fun,
                   bool default_value) const
    {
        if(!fun) {
            return default_value;
        } else {
            auto object=(T *)object_;
            bool ticked=fun(object);
            return ticked;
        }
    }
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
        void (T::*mfn)()=nullptr;
        bool (T::*enabled_mfn)() const=nullptr;
        bool confirm=false;
        intptr_t arg=0;
        uint32_t (T::*get_flags_mfn)() const=nullptr;
        void (T::*set_flags_mfn)(uint32_t)=nullptr;
        uint32_t flags_mask=0;

        Initializer(std::string name_,std::string text_,void (T::*mfn_)(),bool (T::*enabled_mfn_)() const=nullptr):
            name(std::move(name_)),
            text(std::move(text_)),
            mfn(std::move(mfn_)),
            enabled_mfn(std::move(enabled_mfn_))
        {
        }

        Initializer(std::string name_,std::string text_,void (T::*mfn_)(),ConfirmCommand):
            name(std::move(name_)),
            text(std::move(text_)),
            mfn(std::move(mfn_)),
            confirm(true)
        {
        }

        Initializer(std::string name_,std::string text_,uint32_t (T::*get_flags_mfn_)() const,void (T::*set_flags_mfn_)(uint32_t),uint32_t flags_mask_):
            name(std::move(name_)),
            text(std::move(text_)),
            get_flags_mfn(get_flags_mfn_),
            set_flags_mfn(set_flags_mfn_),
            flags_mask(flags_mask_)
        {
        }
    };

    ObjectCommandTable(std::string table_name,std::initializer_list<Initializer> inits):
        CommandTable(std::move(table_name))
    {
        for(const Initializer &init:inits) {
            std::function<void(T *)> execute_fun;
            std::function<bool(T *)> enabled_fun;
            std::function<bool(T *)> ticked_fun;

            if(init.mfn) {
                // Command
                auto mfn=init.mfn;
                execute_fun=[=](T *p) {
                    (p->*mfn)();
                };
            } else if(init.flags_mask!=0) {
                // Toggle
                ASSERT(init.get_flags_mfn&&init.set_flags_mfn);
                auto get_flags_mfn=init.get_flags_mfn;
                auto set_flags_mfn=init.set_flags_mfn;
                auto flags_mask=init.flags_mask;
                execute_fun=[=](T *p) {
                    uint32_t flags=(p->*get_flags_mfn)();
                    flags^=flags_mask;
                    (p->*set_flags_mfn)(flags);
                };
                ticked_fun=[=](T *p) {
                    uint32_t flags=(p->*get_flags_mfn)();
                    return !!(flags&flags_mask);
                };
            } else {
                ASSERT(false);
            }

            if(init.enabled_mfn) {
                auto enabled_mfn=init.enabled_mfn;
                enabled_fun=[=](T *p) {
                    return (p->*enabled_mfn)();
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

    void DoMenuItemUI(const char *name);
    bool ExecuteCommandsForPCKey(uint32_t keycode) const;
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

    void DoMenuItemUI(const char *name) {
        this->cc->DoMenuItemUI(name);
    }
protected:
private:
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#endif
