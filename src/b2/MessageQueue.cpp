#include <shared/system.h>
#include <shared/debug.h>
#include "MessageQueue.h"
#include <stdlib.h>
#include <limits.h>
#include <shared/mutex.h>
#include <vector>

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

typedef uint64_t SyntheticMessageSet;

#define NUM_SYNTHETIC_MESSAGES (sizeof(SyntheticMessageSet)*CHAR_BIT)

struct MessageQueue {
    Mutex mutex;
    ConditionVariable cv;
    std::vector<Message> msgs;
    SyntheticMessageSet synthetic_messages_pending=0;
    Message::Data synthetic_messages_data[NUM_SYNTHETIC_MESSAGES]={};

#if MESSAGE_QUEUE_TRACK_LATENCY
    uint64_t min_latency=UINT64_MAX;
    uint64_t max_latency=0;
    uint64_t total_latency=0;
    size_t num_pops=0;
    uint64_t synthetic_add_ticks[NUM_SYNTHETIC_MESSAGES]={};
#endif
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void MessageDestroy(Message *msg) {
    if(msg->destroy_fn) {
        (*msg->destroy_fn)(msg);
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

MessageQueue *MessageQueueAlloc(void) {
    auto mq=new MessageQueue;

    MUTEX_SET_NAME(mq->mutex,"MessageQueue");

    return mq;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void MessageQueueFree(MessageQueue **mq_ptr) {
    MessageQueue *mq=*mq_ptr;
    *mq_ptr=NULL;

    if(!mq) {
        return;
    }

    for(Message &msg:mq->msgs) {
        MessageDestroy(&msg);
    }

    delete mq;
    mq=nullptr;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void MessageQueueAddSyntheticMessage(MessageQueue *mq,uint32_t u32,Message::Data data) {
    ASSERT(u32<NUM_SYNTHETIC_MESSAGES);

    uint64_t old;
    {
        std::lock_guard<Mutex> lock(mq->mutex);

        old=mq->synthetic_messages_pending;
        uint64_t mask=1ull<<u32;

#if MESSAGE_QUEUE_TRACK_LATENCY
        mq->synthetic_add_ticks[u32]=GetCurrentTickCount();
#endif

        mq->synthetic_messages_pending|=mask;
        mq->synthetic_messages_data[u32]=data;
    }

    if(old==0) {
        mq->cv.notify_one();
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void MessageQueuePush(MessageQueue *mq,const Message *msg) {
    ASSERT(msg->type!=MESSAGE_TYPE_SYNTHETIC);

    {
        std::lock_guard<Mutex> lock(mq->mutex);

        mq->msgs.push_back(*msg);

#if MESSAGE_QUEUE_TRACK_LATENCY
        mq->msgs.back().push_ticks=GetCurrentTickCount();
#endif
    }

    mq->cv.notify_one();
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static bool Poll(MessageQueue *mq,Message *msg) {
#if MESSAGE_QUEUE_TRACK_LATENCY
    uint64_t push_ticks;
#endif

    if(!mq->msgs.empty()) {
        *msg=mq->msgs[0];

        // Crap.
        mq->msgs.erase(mq->msgs.begin());

#if MESSAGE_QUEUE_TRACK_LATENCY
        push_ticks=msg->push_ticks;
#endif
    } else if(mq->synthetic_messages_pending!=0) {
        int n=GetLowestSetBitIndex64(mq->synthetic_messages_pending);
        mq->synthetic_messages_pending&=~1ull<<n;

        msg->type=MESSAGE_TYPE_SYNTHETIC;
        msg->u32=(uint32_t)n;
        msg->data=mq->synthetic_messages_data[n];
        msg->sender=NULL;
        msg->destroy_fn=NULL;

#if MESSAGE_QUEUE_TRACK_LATENCY
        push_ticks=mq->synthetic_add_ticks[n];
#endif
    } else {
        return false;
    }

#if MESSAGE_QUEUE_TRACK_LATENCY
    uint64_t latency=GetCurrentTickCount()-push_ticks;
    mq->total_latency+=latency;
    ++mq->num_pops;

    if(latency<mq->min_latency) {
        mq->min_latency=latency;
    }

    if(latency>mq->max_latency) {
        mq->max_latency=latency;
    }
#endif

    return true;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void MessageQueueWaitForMessage(MessageQueue *mq,Message *msg) {
    std::unique_lock<Mutex> lock(mq->mutex);

    while(!Poll(mq,msg)) {
        mq->cv.wait(lock);
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

int MessageQueuePollForMessage(MessageQueue *mq,Message *msg) {
    std::lock_guard<Mutex> lock(mq->mutex);

    return Poll(mq,msg);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
