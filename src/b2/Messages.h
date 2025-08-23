#ifndef HEADER_5A0BE1D25080409B836861825A614239 // -*- mode:c++ -*-
#define HEADER_5A0BE1D25080409B836861825A614239

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#include <shared/log.h>
#include <string>
#include <vector>
#include <functional>
#include <stdio.h>
#include <memory>
#include <atomic>

#include <shared/enum_decl.h>
#include "Messages.inl"
#include <shared/enum_end.h>

class Messages;

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// An object that collects a rolling queue of info/warning/error output in a
// format that's easy to display in the gui and easy to fill from a Log.
// Operations can take a pointer to one of these, so they can produce output for
// the window the operation was initiated from - tidier than passing Log
// pointers everywhere, and a bit more flexible than just having a single string
// to fill in with an error description.
//
// There's also a bit of fluff in there to print things to stdio (intended for
// debug verbosity purposes, or for spilling accumulated messages on error) and
// merging message lists.
//
// The Messages class is not threadsafe, and each thread needs its own.

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

class MessageList : public std::enable_shared_from_this<MessageList> {
  public:
    struct Message {
        MessageType type;
        uint64_t ticks;
        std::string text;

        // Set if message has been "seen" - whatever that means. For caller use.
        // Set to false when message is added to the list.
        bool seen : 1;

        // Set if message has been printed to stdio. Don't print it again if
        // flushing.
        bool printed_to_stdio : 1;

        Message(MessageType type, uint64_t ticks, std::string text);
    };

    explicit MessageList(std::string name,
                         size_t max_num_messages = 500,
                         uint32_t flags = MessageListFlags_Save);
    ~MessageList();

    MessageList(const MessageList &src) = delete;
    MessageList &operator=(const MessageList &src) = delete;

    MessageList(MessageList &&oth) = delete;
    MessageList &operator=(MessageList &&oth) = delete;

    // Total number of messages printed.
    uint64_t GetNumMessagesPrinted() const;

    // Total number of error messages printed.
    uint64_t GetNumErrorsPrinted() const;

    // Locks the mutex while it does its thing.
    //
    // The 2ary version returns only the most recent min(n,# messages)
    // messages.
    //
    // The Message is mutable, even if the Messages is const.
    void ForEachMessage(std::function<void(Message *)> fun) const;
    void ForEachMessage(size_t n, std::function<void(Message *)> fun) const;

    // This resets the message print counters.
    void ClearMessages();

    // Inserts all messages from src into their appropriate place in
    // the message list (based on their timestamp), respecting the
    // message count limit.
    //
    // Not terribly efficient.
    void InsertMessages(const MessageList &src);

    // Get/set the flags. A combination of MessageListFlags values.
    uint32_t GetFlags() const;
    void SetFlags(uint32_t flags);

    // A global Messages with the stdio flag set.
    static const std::shared_ptr<MessageList> stdio;

  protected:
  private:
    class Printer : public LogPrinter {
      public:
        Printer(MessageList *owner,
                MessageType type,
                std::atomic<uint64_t> *counter_ptr);

        void Print(const char *str, size_t str_len) override;

      protected:
      private:
        MessageList *m_owner = nullptr;
        MessageType m_type = MessageType_Error;
        std::atomic<uint64_t> *m_counter_ptr = nullptr;
    };

    Printer m_info_printer;
    Printer m_warning_printer;
    Printer m_error_printer;

    std::string m_name;
    mutable Mutex m_mutex;
    mutable std::vector<Message> m_messages;
    size_t m_head = 0;
    size_t m_max_num_messages = 0;
    std::atomic<uint64_t> m_num_messages_printed;
    std::atomic<uint64_t> m_num_errors_printed;

    uint32_t m_flags = 0;

    // Return value remains valid until mutex is unlocked or another non-const
    // MessageList member function is called.
    Message *LockedAddMessage(MessageType type, const char *str, size_t str_len);
    void LockedClearMessages();
    void LockedForEachMessage(size_t n, std::function<void(Message *)> fun) const;
    void LockedFlushMessagesToStdio();
    static FILE *GetStdioFileForMessageType(MessageType type);
    static void PrintMessageToStdio(Message *m);

    friend class Messages;
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// N.B. - this is quite a large object.
class Messages : public LogSet {
  public:
    struct STDIOType {};
    static const STDIOType STDIO;

    // When default-constructed, all three logs are disabled and go to
    // the nowhere printer.
    Messages();
    explicit Messages(const STDIOType &); // automatically create a MessageList
                                          // with the PrintToStdio flag set.
    explicit Messages(std::shared_ptr<MessageList> message_list);
    ~Messages();

    Messages(const Messages &) = delete;
    Messages &operator=(const Messages &) = delete;

    Messages(Messages &&) = delete;
    Messages &operator=(Messages &&) = delete;

    std::shared_ptr<MessageList> GetMessageList() const;

    // Setting the message list to non-null enables all logs.
    void SetMessageList(std::shared_ptr<MessageList> message_list);

  protected:
  private:
    std::shared_ptr<MessageList> m_message_list;
    Log m_info, m_warning, m_error;

    void UpdateLog(Log *log, LogPrinter *printer);
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#endif
