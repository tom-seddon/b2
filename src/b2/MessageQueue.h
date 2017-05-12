#ifndef HEADER_67DC11ABBA384C1C9660A2EFD6F63CBE
#define HEADER_67DC11ABBA384C1C9660A2EFD6F63CBE

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#define MESSAGE_QUEUE_TRACK_LATENCY 1

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static const uint32_t MESSAGE_TYPE_SYNTHETIC=UINT32_MAX;

struct Message {
    struct Data {
        uint64_t u64;
        void *ptr;
    };

    // 0xffffffff is not a valid type.
    uint32_t type;
    uint32_t u32;
    Data data;
    void *sender;
    void (*destroy_fn)(Message *);
#ifdef MESSAGE_QUEUE_TRACK_LATENCY
    uint64_t push_ticks;
#endif
};

void MessageDestroy(Message *msg);

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

struct MessageQueue;

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

MessageQueue *MessageQueueAlloc(void);
void MessageQueueFree(MessageQueue **mq_ptr);

// Synthetic messages are retrieved when no other messages are
// available. 
//
// Synthetic messages are emitted based on a set of flags. One
// synthetic message with a given u32 will be retrieved, no matter how
// many times one was added since the last time one was retrieved.
//
// When a synthetic message is retrieved, its message type will be
// MESSAGE_TYPE_SYNTHETIC, its u32 the u32 supplied here, and its data
// the data supplied to the last call to
// MessageQueueAddSyntheticMessage for that u32.
void MessageQueueAddSyntheticMessage(MessageQueue *mq,uint32_t u32,Message::Data data);

// Add a message to the queue. One waiter is woken.
void MessageQueuePush(MessageQueue *mq,const Message *msg);

void MessageQueueWaitForMessage(MessageQueue *mq,Message *msg);

int MessageQueuePollForMessage(MessageQueue *mq,Message *msg);

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#endif
