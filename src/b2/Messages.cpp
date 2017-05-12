#include <shared/system.h>
#include "Messages.h"
#include <shared/debug.h>
//#include <shared/threads.h>
#include <algorithm>

#include <shared/enum_def.h>
#include "Messages.inl"
#include <shared/enum_end.h>

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

MessageList MessageList::stdio(1,true);

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

MessageList::Message::Message(MessageType type_,
                              uint64_t ticks_,
                              std::string text_):
    type(type_),
    ticks(ticks_),
    text(std::move(text_)),
    seen(false)
{
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

MessageList::MessageList(size_t max_num_messages,bool print_to_stdio):
    m_info_printer(this,MessageType_Info,nullptr),
    m_warning_printer(this,MessageType_Warning,&m_num_errors_and_warnings_printed),
    m_error_printer(this,MessageType_Error,&m_num_errors_and_warnings_printed),
    m_max_num_messages(max_num_messages),
    m_print_to_stdio(print_to_stdio)
{
    this->ClearMessages();
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

MessageList::~MessageList() {
    if(m_print_to_stdio) {
        this->FlushMessagesToStdio();
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

uint64_t MessageList::GetNumMessagesPrinted() const {
    return m_num_messages_printed;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

uint64_t MessageList::GetNumErrorsAndWarningsPrinted() const {
    return m_num_errors_and_warnings_printed;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void MessageList::ForEachMessage(std::function<void(Message *)> fun) const {
    this->ForEachMessage(SIZE_MAX,fun);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void MessageList::ForEachMessage(size_t n,std::function<void(Message *)> fun) const {
    if(!!fun) {
        std::lock_guard<std::mutex> lock(m_mutex);

        this->LockedForEachMessage(n,fun);
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void MessageList::ClearMessages() {
    std::lock_guard<std::mutex> this_lock(m_mutex);

    this->LockedClearMessages();
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void MessageList::InsertMessages(const MessageList &src) {
    if(m_print_to_stdio) {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        src.ForEachMessage(&PrintMessageToStdio);
    } else {
        // Don't know quite what the right thing is to do with the seen
        // flag exactly...

        std::unique_lock<std::mutex> this_lock(m_mutex,std::defer_lock);
        std::unique_lock<std::mutex> src_lock(src.m_mutex,std::defer_lock);

        std::lock(this_lock,src_lock);

        size_t old_size=m_messages.size();
    
        m_messages.insert(m_messages.end(),
                          src.m_messages.begin(),
                          src.m_messages.end());

        uint64_t num_errors_and_warnings=0;
    
        for(size_t j=old_size;j<m_messages.size();++j) {
            Message *message=&m_messages[j];

            message->seen=false;

            if(message->type==MessageType_Warning||
               message->type==MessageType_Error)
            {
                ++num_errors_and_warnings;
            }
        }

        m_num_messages_printed+=src.m_messages.size();
        m_num_errors_and_warnings_printed+=num_errors_and_warnings;

        std::stable_sort(m_messages.begin(),
                         m_messages.end(),
                         [](auto &&a,auto &&b) {
                             return a.ticks<b.ticks;
                         });

        if(m_messages.size()>m_max_num_messages) {
            // The new element will of course never be used, because the
            // list is always being shrunk - but it's necessary because
            // Message doesn't have a default constructor.
            m_messages.resize(m_max_num_messages,
                              Message(MessageType_Info,0,std::string()));
        }

        m_head=0;
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void MessageList::SetPrintToStdio(bool print_to_stdio) {
    std::lock_guard<std::mutex> this_lock(m_mutex);

    if(!m_print_to_stdio&&print_to_stdio) {
        this->LockedFlushMessagesToStdio();
        
        this->LockedClearMessages();
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// Print all accumulated messages to stdout/stderr and clear the
// list.
void MessageList::FlushMessagesToStdio() {
    std::lock_guard<std::mutex> this_lock(m_mutex);

    this->LockedFlushMessagesToStdio();
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

MessageList::Printer::Printer(MessageList *owner,
                              MessageType type,
                              std::atomic<uint64_t> *counter_ptr):
    m_owner(owner),
    m_type(type),
    m_counter_ptr(counter_ptr)
{
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void MessageList::Printer::Print(const char *str,size_t str_len) {
    if(m_owner->m_print_to_stdio) {
        if(FILE *f=MessageList::GetStdioFileForMessageType(m_type)) {
            fwrite(str,1,str_len,f);
        }
    } else {
        m_owner->AddMessage(m_type,str,str_len);
    }

    if(m_counter_ptr) {
        ++*m_counter_ptr;
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void MessageList::AddMessage(MessageType type,
                             const char *str,
                             size_t str_len)
{
    std::lock_guard<std::mutex> this_lock(m_mutex);
    
    if(m_messages.size()<m_max_num_messages) {
        ASSERT(m_head==0);
        m_messages.emplace_back(type,
                                GetCurrentTickCount(),
                                std::string(str,str_len));
    } else {
        ASSERT(m_head<m_messages.size());
        m_messages[m_head]=Message(type,
                                   GetCurrentTickCount(),
                                   std::string(str,str_len));

        ++m_head;
        m_head%=m_messages.size();
    }

    ++m_num_messages_printed;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void MessageList::LockedClearMessages()
{
    m_messages.clear();
    m_head=0;

    m_num_errors_and_warnings_printed=0;
    m_num_messages_printed=0;
}   

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void MessageList::LockedForEachMessage(size_t n,std::function<void(Message *)> fun) const {
    n=(std::min)(n,m_messages.size());

    size_t index=m_head+(m_messages.size()-n);
        
    for(size_t j=0;j<m_messages.size();++j) {
        fun(&m_messages[index%m_messages.size()]);

        ++index;
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void MessageList::LockedFlushMessagesToStdio() {
    this->LockedForEachMessage(SIZE_MAX,&PrintMessageToStdio);

    this->LockedClearMessages();
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

FILE *MessageList::GetStdioFileForMessageType(MessageType type) {
    if(type==MessageType_Info) {
        return stdout;
    } else {
        return stderr;
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void MessageList::PrintMessageToStdio(Message *m) {
    if(FILE *f=GetStdioFileForMessageType(m->type)) {
        fwrite(m->text.c_str(),1,m->text.size(),f);
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

Messages::Messages():
    i("",&log_printer_nowhere,false),
    w("",&log_printer_nowhere,false),
    e("",&log_printer_nowhere,false)
{
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

Messages::Messages(std::shared_ptr<MessageList> message_list):
    i("",&message_list->m_info_printer),
    w("",&message_list->m_warning_printer),
    e("",&message_list->m_error_printer),
    m_message_list(std::move(message_list))
{
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
