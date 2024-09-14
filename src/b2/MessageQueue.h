#ifndef HEADER_67DC11ABBA384C1C9660A2EFD6F63CBE
#define HEADER_67DC11ABBA384C1C9660A2EFD6F63CBE

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#define MESSAGE_QUEUE_TRACK_LATENCY 1

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#include <shared/system.h>
#include <shared/debug.h>
#include <shared/mutex.h>
#include <condition_variable>
#include <vector>

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// T needs to be default-constructible and movable.
template <class T>
class MessageQueue {
  public:
    MessageQueue() = default;

    void SetName(const char *name) {
        (void)name;
        MUTEX_SET_NAME(m_mutex, name);
    }

    // Pushed messages are retrieved in the order they were submitted.
    void ProducerPush(T message) {
        LockGuard<Mutex> lock(m_mutex);

        m_messages.push_back(Message(std::move(message)));

        m_cv.notify_one();
    }

    // When no pushed messages are available, an indexed pushed
    // message is retrieved. Each message pushed with a particular
    // index replaces the previous message with that index, so that
    // messages that supercede any previous messages of the same type
    // can be pushed without having to worry about filling the queue
    // up.
    //
    // The consumer can't distinguished indexed messages from
    // non-indexed ones. The distinction is for the producer's
    // benefit.
    //
    // Valid indexes are from 0 to 63.
    void ProducerPushIndexed(uint8_t index, T message) {
        ASSERT(index < 64);

        uint64_t old_indexed_messages_pending;
        {
            LockGuard<Mutex> lock(m_mutex);

            old_indexed_messages_pending = m_indexed_messages_pending;

            m_indexed_messages[index] = Message(std::move(message));
            m_indexed_messages_pending |= 1ull << index;
        }

        if (old_indexed_messages_pending == 0) {
            m_cv.notify_one();
        }
    }

    void ConsumerWaitForMessages(std::vector<T> *messages) {
        UniqueLock<Mutex> lock(m_mutex);

        for (;;) {
            if (this->PollLocked(messages)) {
                return;
            }

            m_cv.wait(lock);
        }
    }

    bool ConsumerPollForMessages(std::vector<T> *messages) {
        LockGuard<Mutex> lock(m_mutex);

        return this->PollLocked(messages);
    }

  protected:
  private:
    struct Message {
        T value;
#if MESSAGE_QUEUE_TRACK_LATENCY
        uint64_t push_ticks = GetCurrentTickCount();
#endif

        Message() = default;

        explicit Message(T value_)
            : value(std::move(value_)) {
        }
    };

    Mutex m_mutex;
    std::condition_variable_any m_cv;
    std::vector<Message> m_messages;
    Message m_indexed_messages[64];
    uint64_t m_indexed_messages_pending = 0;
#if MESSAGE_QUEUE_TRACK_LATENCY
    uint64_t m_total_latency = 0;
    uint64_t m_min_latency = UINT64_MAX;
    uint64_t m_max_latency = 0;
#endif

    bool PollLocked(std::vector<T> *messages) {
        bool any = false;
        uint64_t push_ticks = UINT64_MAX;

        if (!m_messages.empty()) {
#if MESSAGE_QUEUE_TRACK_LATENCY
            push_ticks = m_messages[0].push_ticks;
#endif

            for (Message &message : m_messages) {
                messages->push_back(std::move(message.value));
            }

            m_messages.clear();
            any = true;
        }

        if (m_indexed_messages_pending != 0) {
            for (uint8_t i = 0; i < 64; ++i) {
                if (m_indexed_messages_pending & 1ull << i) {
                    Message *m = &m_indexed_messages[i];

#if MESSAGE_QUEUE_TRACK_LATENCY
                    push_ticks = std::min(push_ticks, m->push_ticks);
#endif
                    messages->push_back(std::move(m->value));

                    any = true;
                }
            }

            m_indexed_messages_pending = 0;
        }

#if MESSAGE_QUEUE_TRACK_LATENCY
        if (any) {
            uint64_t latency = GetCurrentTickCount() - push_ticks;
            m_total_latency += latency;
            m_min_latency = std::min(m_min_latency, latency);
            m_max_latency = std::max(m_max_latency, latency);
        }
#endif

        return any;
    }
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#endif
