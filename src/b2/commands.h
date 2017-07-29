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
    Command(std::string name,std::string text);

    //void DoMenuItemUI(void *object,bool enabled=true);
    virtual void Execute(void *object)=0;

    const std::string &GetName() const;
    const std::string &GetText() const;
protected:
private:
    const std::string m_name;
    std::string m_text;
    uint32_t m_shortcut=0;

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
    ObjectCommand(std::string name,std::string text,void (T::*mfn)()):
        Command(std::move(name),std::move(text)),
        m_mfn(mfn)
    {
    }

    void Execute(void *object_) override {
        auto object=(T *)object_;

        (object->*m_mfn)();
    }
protected:
private:
    void (T::*m_mfn)()=nullptr;
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

    CommandTable(std::string name);
    ~CommandTable();

    Command *FindCommandByName(const char *name) const;
    Command *FindCommandByName(const std::string &str) const;

    const std::string &GetName() const;

    void ForEachCommand(std::function<void(Command *)> fun);

    void SetMapping(uint32_t pc_key,Command *command,bool state);
    const uint32_t *GetPCKeysForValue(Command *command) const;
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
        void (T::*mfn)();
        intptr_t arg;
    };

    ObjectCommandTable(std::string name,std::initializer_list<Initializer> inits):
        CommandTable(std::move(name))
    {
        for(const Initializer &init:inits) {
            this->AddCommand(init.name,init.text,init.mfn);
        }
    }

    Command *AddCommand(std::string name,std::string text,void (T::*mfn)()) {
        return this->CommandTable::AddCommand(std::make_unique<ObjectCommand<T>>(std::move(name),std::move(text),mfn));
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

    void DoMenuItemUI(const char *name,bool enabled=true);
protected:
private:
    void *m_object=nullptr;
    const CommandTable *m_table=nullptr;
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

template<class T>
class ObjectCommandContext:
    private CommandContext
{
public:
    ObjectCommandContext(T *object,const ObjectCommandTable<T> *table):
        CommandContext(object,table)
    {
    }

    using CommandContext::DoMenuItemUI;
protected:
private:
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#endif
