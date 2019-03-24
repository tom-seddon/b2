#include <shared/system.h>
#include <beeb/Trace.h>

#if BBCMICRO_TRACE

#include <shared/debug.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <shared/log.h>

struct M6502Config;

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static const size_t CHUNK_SIZE=16777216;

static const size_t MAX_TIME_DELTA=127;

static const size_t MAX_EVENT_SIZE=65535;

static_assert(MAX_EVENT_SIZE<=CHUNK_SIZE,"chunks must be large enough for at least one event");

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#include <shared/pshpack1.h>
struct EventHeader {
    uint8_t type;
    uint8_t time_delta:7;
    uint8_t canceled:1;
};
#include <shared/poppack.h>
CHECK_SIZEOF(EventHeader,2);

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#include <shared/pshpack1.h>
struct EventWithSizeHeader {
    EventHeader h;
    uint16_t size;
};
#include <shared/poppack.h>
CHECK_SIZEOF(EventWithSizeHeader,4);

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

enum {
    STRING_EVENT_ID,
    DISCONTINUITY_EVENT_ID,
    WRITE_ROMSEL_EVENT_ID,
    WRITE_ACCCON_EVENT_ID,
    FIRST_CUSTOM_EVENT_ID,
};

static TraceEventType *g_trace_event_types[256];
static size_t g_trace_next_id=FIRST_CUSTOM_EVENT_ID;

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

TraceEventType::TraceEventType(const char *name,size_t size_,int8_t type_id_):
    type_id(type_id_>=0?(uint8_t)type_id_:(uint8_t)g_trace_next_id++),
    size(size_),
    m_name(name)
{
    ASSERT(size<=MAX_EVENT_SIZE);
    ASSERT(type_id_>=-1);
    ASSERT(g_trace_next_id<=256);
    ASSERT(!g_trace_event_types[this->type_id]);

    g_trace_event_types[this->type_id]=this;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

TraceEventType::~TraceEventType() {
    ASSERT(g_trace_event_types[this->type_id]==this);

    g_trace_event_types[this->type_id]=nullptr;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

const std::string &TraceEventType::GetName() const {
    return m_name;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

const TraceEventType Trace::STRING_EVENT("_string",0,STRING_EVENT_ID);

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#include <shared/pshpack1.h>
struct DiscontinuityTraceEvent {
    uint64_t new_time;
};
typedef struct DiscontinuityTraceEvent DiscontinuityTraceEvent;
#include <shared/poppack.h>

const TraceEventType Trace::DISCONTINUITY_EVENT("_discontinuity",sizeof(DiscontinuityTraceEvent),DISCONTINUITY_EVENT_ID);

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

const TraceEventType Trace::WRITE_ROMSEL_EVENT("_write_romsel",sizeof(WriteTraceEvent),WRITE_ROMSEL_EVENT_ID);
const TraceEventType Trace::WRITE_ACCCON_EVENT("_write_acccon",sizeof(WriteTraceEvent),WRITE_ACCCON_EVENT_ID);

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// must be a POD type - it's allocated with malloc.
struct Trace::Chunk {
    struct Chunk *next;
    size_t size;
    size_t capacity;
    size_t num_events;

    uint64_t initial_time;
    uint64_t last_time;

    uint8_t initial_romsel_value;
    uint8_t initial_acccon_value;
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

Trace::Trace(size_t max_num_bytes,
             int bbc_micro_type,
             uint8_t initial_romsel_value,
             uint8_t initial_acccon_value):
m_max_num_bytes(max_num_bytes),
m_bbc_micro_type(bbc_micro_type),
m_romsel_value(initial_romsel_value),
m_acccon_value(initial_acccon_value)
{
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

Trace::~Trace() {
    Chunk *c=m_head;
    while(c) {
        Chunk *next=c->next;

        free(c);

        c=next;
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void Trace::SetTime(const uint64_t *time_ptr) {
    m_time_ptr=time_ptr;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void *Trace::AllocEvent(const TraceEventType &type) {
    ASSERT(type.size>0);

    uint64_t time=m_last_time;
    if(m_time_ptr) {
        time=*m_time_ptr;
    }

    auto h=(EventHeader *)this->Alloc(time,sizeof(EventHeader)+type.size);

    h->type=type.type_id;
    h->time_delta=(uint8_t)(time-m_last_time);
    h->canceled=0;

    m_tail->last_time=time;
    m_last_time=time;

    this->Check();

    return h+1;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void Trace::CancelEvent(const TraceEventType &type,void *data) {
    if(type.size==0) {
        EventWithSizeHeader *h=(EventWithSizeHeader *)data-1;

        h->h.canceled=1;
    } else {
        EventHeader *h=(EventHeader *)data-1;

        h->canceled=1;
    }

    this->Check();
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void Trace::AllocStringf(const char *fmt,...) {
    va_list v;

    va_start(v,fmt);
    this->AllocStringv(fmt,v);
    va_end(v);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void Trace::AllocStringv(const char *fmt,va_list v) {
    char buf[1024];

    vsnprintf(buf,sizeof buf,fmt,v);

    this->AllocString(buf);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void Trace::AllocString(const char *str) {
    this->AllocString2(str,strlen(str));
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void Trace::AllocWriteROMSELEvent(uint8_t value) {
    auto ev=(WriteTraceEvent *)this->AllocEvent(WRITE_ROMSEL_EVENT);

    ev->value=value;
    m_romsel_value=value;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void Trace::AllocWriteACCCONEvent(uint8_t value) {
    auto ev=(WriteTraceEvent *)this->AllocEvent(WRITE_ACCCON_EVENT);

    ev->value=value;
    m_acccon_value=value;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

LogPrinter *Trace::GetLogPrinter(size_t max_len) {
    if(max_len>MAX_EVENT_SIZE) {
        max_len=MAX_EVENT_SIZE;
    }

    m_log_data=this->AllocString2(NULL,max_len);
    m_log_len=0;
    m_log_max_len=max_len;

    return &m_log_printer;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void Trace::FinishLog(Log *log) {
    ASSERT(log->GetLogPrinter()==&m_log_printer);
    ASSERT(m_last_alloc);

    log->Flush();

    if(m_log_data!=(char *)m_last_alloc+sizeof(EventWithSizeHeader)) {
        // Can't truncate - there are further allocations.
        return;
    }

    EventWithSizeHeader *h=(EventWithSizeHeader *)m_last_alloc;

#if ASSERT_ENABLED
    const char *tail_data=(const char *)(m_tail+1);
#endif
    
    ASSERT(m_log_data+h->size==tail_data+m_tail->size);

    ASSERT(m_log_max_len>=m_log_len);
    size_t delta=m_log_max_len-m_log_len;

    ASSERT(h->size>=delta);
    h->size-=(uint16_t)delta;

    ASSERT(m_tail->size>=delta);
    m_tail->size-=delta;

    ASSERT(m_stats.num_used_bytes>=delta);
    m_stats.num_used_bytes-=delta;

    ASSERT(m_log_data[h->size-1]==0);

    m_log_data=NULL;

    this->Check();
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void Trace::GetStats(TraceStats *stats) const {
    *stats=m_stats;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

int Trace::GetBBCMicroType() const {
    return m_bbc_micro_type;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

uint8_t Trace::GetInitialROMSELValue() const {
    if(m_head) {
        return m_head->initial_romsel_value;
    } else {
        return m_romsel_value;
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

uint8_t Trace::GetInitialACCCONValue() const {
    if(m_head) {
        return m_head->initial_acccon_value;
    } else {
        return m_acccon_value;
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

int Trace::ForEachEvent(ForEachEventFn fn,void *context) {
    if(!m_head) {
        return 1;
    }

    TraceEvent e;
    e.time=m_head->initial_time;

    Chunk *c=m_head;
    while(c) {
        const uint8_t *p=(uint8_t *)(c+1);
        const uint8_t *end=p+c->size;

        while(p<end) {
            const EventHeader *h=(const EventHeader *)p;
            p+=sizeof(EventHeader);

            ASSERT(h->type<g_trace_next_id);

            e.type=g_trace_event_types[h->type];
            ASSERT(e.type);

            e.size=e.type->size;
            if(e.size==0) {
                const EventWithSizeHeader *hs=(const EventWithSizeHeader *)h;

                e.size=hs->size;
                p+=sizeof(EventWithSizeHeader)-sizeof(EventHeader);
            }

            e.event=p;

            if(e.type==&DISCONTINUITY_EVENT) {
                auto de=(const DiscontinuityTraceEvent *)e.event;

                // Shouldn't be able to find one to cancel it...
                ASSERT(!h->canceled);

                e.time=de->new_time;
            } else {
                e.time+=h->time_delta;

                if(!h->canceled) {
                    if(!(*fn)(this,&e,context)) {
                        return 0;
                    }
                }
            }

            p+=e.size;
        }

        ASSERT(p==end);

        c=c->next;
    }

    return 1;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void *Trace::AllocEventWithSize(const TraceEventType &type,size_t size) {
    ASSERT(type.size==0);

    if(size>MAX_EVENT_SIZE) {
        return nullptr;
    }

    uint64_t time=m_last_time;
    if(m_time_ptr) {
        time=*m_time_ptr;
    }

    auto h=(EventWithSizeHeader *)this->Alloc(time,sizeof(EventWithSizeHeader)+size);

    h->h.type=type.type_id;
    h->h.time_delta=(uint8_t)(time-m_last_time);
    h->h.canceled=0;
    h->size=(uint16_t)size;

    m_last_time=time;

    this->Check();

    return h+1;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

char *Trace::AllocString2(const char *str,size_t len) {
    if(len+1>MAX_EVENT_SIZE) {
        return nullptr;
    }

    auto p=(char *)this->AllocEventWithSize(STRING_EVENT,len+1);
    if(p) {
        if(str) {
            memcpy(p,str,len+1);
        } else {
            *p=0;
        }
    }

    this->Check();

    return p;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void *Trace::Alloc(uint64_t time,size_t n) {
    //ASSERT(ENABLED(t));
    this->Check();

    if(n>MAX_EVENT_SIZE) {
        return nullptr;
    }

    if(!m_tail||m_tail->size+n>m_tail->capacity) {
        size_t size=CHUNK_SIZE;

        if(m_stats.num_allocated_bytes+size>m_max_num_bytes) {
            if(m_head==m_tail) {
                // Always leave at least one used chunk around.
            } else {
                Chunk *old_head=m_head;

                this->Check();

                m_head=m_head->next;

                ASSERT(m_stats.num_used_bytes>=old_head->size);
                m_stats.num_used_bytes-=old_head->size;

                ASSERT(m_stats.num_allocated_bytes>=old_head->capacity);
                m_stats.num_allocated_bytes-=old_head->capacity;

                ASSERT(m_stats.num_events>=old_head->num_events);
                m_stats.num_events-=old_head->num_events;

                // Could/should maybe reuse the old head instead...
                free(old_head);
                old_head=nullptr;

                this->Check();
            }
        }

        Chunk *c=(Chunk *)malloc(sizeof *c+size);

        memset(c,0,sizeof *c);
        c->capacity=size;
        c->initial_time=c->last_time=time;

        if(!m_head) {
            m_head=c;
        } else {
            m_tail->next=c;
        }

        m_tail=c;

        // Don't bother accounting for the header... it's just noise.
        m_stats.num_allocated_bytes+=m_tail->capacity;

        this->Check();
    }

    if(time>m_stats.max_time) {
        m_stats.max_time=time;
    }

    if(time<m_last_time||time-m_last_time>MAX_TIME_DELTA) {
        // Insert a discontinuity event. Ensure it has a time_delta of
        // 0.
        m_last_time=time;

        auto de=(DiscontinuityTraceEvent *)this->AllocEvent(DISCONTINUITY_EVENT);

        de->new_time=time;
    }
    ASSERT(time>=m_last_time&&time-m_last_time<=MAX_TIME_DELTA);

    uint8_t *p=(uint8_t *)(m_tail+1)+m_tail->size;

    m_tail->size+=n;
    m_stats.num_used_bytes+=n;

    ++m_tail->num_events;
    ++m_stats.num_events;

    m_tail->last_time=time;

    m_last_alloc=p;

    this->Check();

    return p;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void Trace::Check() {
#if ASSERT_ENABLED
//    size_t total_num_used_bytes=0;
//    size_t total_num_allocated_bytes=0;
//    size_t total_num_events=0;
//
//    for(const Chunk *c=m_head;c;c=c->next) {
//        total_num_used_bytes+=c->size;
//        total_num_allocated_bytes+=c->capacity;
//        total_num_events+=c->num_events;
//    }
//
//    ASSERT(total_num_used_bytes==m_stats.num_used_bytes);
//    ASSERT(total_num_allocated_bytes==m_stats.num_allocated_bytes);
//    ASSERT(total_num_events==m_stats.num_events);
#endif
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

Trace::LogPrinterTrace::LogPrinterTrace(Trace *t):
    m_t(t)
{
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void Trace::LogPrinterTrace::Print(const char *str,size_t str_len) {
    if(!m_t->m_log_data) {
        // This could actually be a stale log... it's not actually a
        // problem.
        return;
    }

    if(m_t->m_log_len==m_t->m_log_max_len) {
        return;
    }

    if(str_len>m_t->m_log_max_len-m_t->m_log_len) {
        str_len=m_t->m_log_max_len-m_t->m_log_len;
    }

    memcpy(m_t->m_log_data+m_t->m_log_len,str,str_len+1);
    m_t->m_log_len+=str_len;
    ASSERT(m_t->m_log_data[m_t->m_log_len]==0);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#endif
